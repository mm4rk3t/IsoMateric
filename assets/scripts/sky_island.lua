-- sky_island.level: a quiet town square floating in space.
-- Built for the 1.8 tall player in a world of 1 block ~ 1 m: doors are 2.1 tall, seats
-- 0.45, railings come to the waist, lamp posts stand 3 m. Everything decorative is
-- spawned here; the walkable ground (and the house walls) are the level.

local npc
local marker
local C = 12      -- island centre
local FLOOR = 0.5 -- top of the ground blocks

-- glow: unlit; alpha < 1: see-through; emissive > 0: lit plus its own glow
local function box(x, y, z, sx, sy, sz, r, g, b, glow, alpha, emissive)
	return iso.spawn_box(x, y, z, sx, sy, sz, r, g, b, glow or false, alpha or 1, emissive or 0)
end

-- a box standing on the floor: y is its bottom above the floor, h its height
local function stand(x, y, z, sx, h, sz, r, g, b, glow, alpha, emissive)
	return box(x, FLOOR + y + h * 0.5, z, sx, h, sz, r, g, b, glow, alpha, emissive)
end

local IRON = { 0.16, 0.15, 0.15 }
local WOOD = { 0.45, 0.3, 0.18 }
local DARK_WOOD = { 0.3, 0.2, 0.12 }

-- a 3 m lamp post; the level file puts a light in its lantern (at about 3.45)
local function lamp(x, z)
	stand(x, 0, z, 0.3, 0.2, 0.3, IRON[1], IRON[2], IRON[3])           -- foot
	stand(x, 0.2, z, 0.12, 2.5, 0.12, IRON[1], IRON[2], IRON[3])       -- post
	stand(x, 2.7, z, 0.36, 0.06, 0.36, IRON[1], IRON[2], IRON[3])      -- lantern base
	stand(x, 2.76, z, 0.26, 0.36, 0.26, 1.0, 0.8, 0.45, true)           -- glass
	stand(x, 3.12, z, 0.4, 0.06, 0.4, IRON[1], IRON[2], IRON[3])       -- cap
	stand(x, 3.18, z, 0.14, 0.12, 0.14, IRON[1], IRON[2], IRON[3])
end

-- a signpost with the board at eye level; dir 1 = board along x, 2 = along z
local function sign(x, z, dir)
	stand(x, 0, z, 0.1, 2.1, 0.1, DARK_WOOD[1], DARK_WOOD[2], DARK_WOOD[3])
	if dir == 1 then
		stand(x, 1.45, z, 0.9, 0.4, 0.06, 0.62, 0.45, 0.26)
	else
		stand(x, 1.45, z, 0.06, 0.4, 0.9, 0.62, 0.45, 0.26)
	end
end

-- a park bench, seat at 0.45, along x; back = which side the backrest is on (-1 / 1 in z)
local function bench(x, z, back)
	stand(x, 0.4, z, 1.6, 0.06, 0.45, WOOD[1], WOOD[2], WOOD[3])                    -- seat
	stand(x, 0.55, z + back * 0.2, 1.6, 0.4, 0.06, WOOD[1], WOOD[2], WOOD[3])        -- backrest
	for _, dx in ipairs({ -0.7, 0.7 }) do
		stand(x + dx, 0, z, 0.06, 0.4, 0.42, IRON[1], IRON[2], IRON[3])            -- legs
		stand(x + dx, 0.46, z + back * 0.2, 0.06, 0.5, 0.06, IRON[1], IRON[2], IRON[3])
	end
end

local function bush(x, z, s)
	stand(x, 0, z, s, s * 0.9, s, 0.22, 0.42, 0.22)
	stand(x + s * 0.3, s * 0.35, z - s * 0.2, s * 0.7, s * 0.65, s * 0.7, 0.26, 0.5, 0.25)
end

local function barrel(x, z)
	stand(x, 0, z, 0.56, 0.9, 0.56, 0.42, 0.27, 0.15)
	for _, y in ipairs({ 0.15, 0.72 }) do
		stand(x, y, z, 0.6, 0.05, 0.6, IRON[1], IRON[2], IRON[3])
	end
end

local function crate(x, z, s, y)
	stand(x, y or 0, z, s, s, s, 0.55, 0.4, 0.24)
	stand(x, (y or 0) + s * 0.45, z, s + 0.02, 0.06, s + 0.02, 0.42, 0.3, 0.17)
end

-- waist-high railing from (x0, z0) to (x1, z1), axis aligned
local function railing(x0, z0, x1, z1)
	local len = math.abs(x1 - x0) + math.abs(z1 - z0)
	local alongX = math.abs(x1 - x0) > 0
	local n = math.max(1, math.floor(len + 0.5))
	for i = 0, n do
		local t = i / n
		stand(x0 + (x1 - x0) * t, 0, z0 + (z1 - z0) * t, 0.08, 1.0, 0.08, DARK_WOOD[1], DARK_WOOD[2], DARK_WOOD[3])
	end
	local cx, cz = (x0 + x1) * 0.5, (z0 + z1) * 0.5
	for _, y in ipairs({ 0.5, 0.95 }) do
		if alongX then
			stand(cx, y, cz, len, 0.06, 0.06, DARK_WOOD[1], DARK_WOOD[2], DARK_WOOD[3])
		else
			stand(cx, y, cz, 0.06, 0.06, len, DARK_WOOD[1], DARK_WOOD[2], DARK_WOOD[3])
		end
	end
end

-- the house: plaster walls are level columns (x 5-8, z 5-8, 3 m high); door, windows,
-- shutters and a stepped roof are decoration
local function house()
	local hx, hz = 6.5, 6.5
	local wall = 3.5 -- top of the walls
	local roof = { { 4.8, 0.0 }, { 4.0, 0.4 }, { 3.2, 0.8 }, { 2.4, 1.2 }, { 1.6, 1.6 }, { 0.8, 2.0 } }
	for i, step in ipairs(roof) do
		local shade = 0.8 + i * 0.04
		box(hx, wall + 0.2 + step[2], hz, step[1], 0.4, step[1], 0.55 * shade, 0.22 * shade, 0.18 * shade)
	end
	box(7.6, wall + 2.0, 5.4, 0.5, 1.8, 0.5, 0.4, 0.36, 0.34)  -- chimney
	box(7.6, wall + 2.95, 5.4, 0.6, 0.1, 0.6, 0.32, 0.3, 0.28)

	-- door on the square side (+x), 0.9 x 2.1, with a frame, step and a lamp beside it
	stand(8.53, 0, 6.5, 0.06, 2.1, 0.9, 0.32, 0.2, 0.12)
	stand(8.56, 0, 6.02, 0.08, 2.2, 0.08, 0.25, 0.16, 0.1)
	stand(8.56, 0, 6.98, 0.08, 2.2, 0.08, 0.25, 0.16, 0.1)
	stand(8.56, 2.12, 6.5, 0.08, 0.1, 1.04, 0.25, 0.16, 0.1)
	stand(8.6, 1.0, 6.85, 0.08, 0.08, 0.08, 0.9, 0.8, 0.3, true) -- knob at hand height
	stand(8.75, 0, 6.5, 0.4, 0.12, 1.1, 0.5, 0.48, 0.45)        -- step
	stand(8.6, 2.0, 7.35, 0.14, 0.24, 0.14, 1.0, 0.8, 0.45, false, 1, 1.8) -- wall lantern

	-- windows at head height, warm light inside, wooden shutters
	local function window(x, z, facingX)
		if facingX then
			stand(x, 1.2, z, 0.06, 1.0, 0.8, 1.0, 0.82, 0.45, true)
			stand(x + 0.03, 1.15, z, 0.1, 0.06, 0.95, 0.3, 0.2, 0.12)  -- sill
			stand(x + 0.03, 1.2, z - 0.52, 0.04, 1.0, 0.24, 0.25, 0.32, 0.22)
			stand(x + 0.03, 1.2, z + 0.52, 0.04, 1.0, 0.24, 0.25, 0.32, 0.22)
		else
			stand(x, 1.2, z, 0.8, 1.0, 0.06, 1.0, 0.82, 0.45, true)
			stand(x, 1.15, z + 0.03, 0.95, 0.06, 0.1, 0.3, 0.2, 0.12)
			stand(x - 0.52, 1.2, z + 0.03, 0.24, 1.0, 0.04, 0.25, 0.32, 0.22)
			stand(x + 0.52, 1.2, z + 0.03, 0.24, 1.0, 0.04, 0.25, 0.32, 0.22)
		end
	end
	window(8.53, 5.3, true)
	window(5.6, 8.53, false)
	window(7.4, 8.53, false)

	-- by the door: barrels, crates, a bench under the window
	barrel(9.0, 5.2)
	barrel(9.1, 5.85)
	crate(9.9, 5.3, 0.6)
	crate(9.9, 5.35, 0.45, 0.6)
	bench(6.5, 9.0, -1)
end

-- a market stall: counter, four posts and a striped canvas roof at 2.4
local function stall(x, z)
	stand(x, 0, z, 2.0, 0.95, 0.7, WOOD[1], WOOD[2], WOOD[3])        -- counter
	stand(x, 0.95, z, 2.1, 0.06, 0.8, DARK_WOOD[1], DARK_WOOD[2], DARK_WOOD[3])
	for _, dx in ipairs({ -1.0, 1.0 }) do
		for _, dz in ipairs({ -0.35, 0.9 }) do
			stand(x + dx, 0, z + dz, 0.08, 2.45, 0.08, DARK_WOOD[1], DARK_WOOD[2], DARK_WOOD[3])
		end
	end
	for k = 0, 5 do -- striped canvas, sloping down towards the customer
		local red = k % 2 == 0
		stand(x - 0.875 + k * 0.35, 2.45, z + 0.27, 0.35, 0.06, 1.5, red and 0.62 or 0.85, red and 0.16 or 0.8, red and 0.14 or 0.7)
	end
	-- goods: apples and a basket
	for k = 0, 4 do
		stand(x - 0.6 + k * 0.3, 1.01, z - 0.05, 0.14, 0.14, 0.14, 0.7, 0.12 + k * 0.03, 0.1)
	end
	stand(x + 0.75, 1.01, z, 0.4, 0.22, 0.35, 0.6, 0.45, 0.25)
end

-- rocky underside so the island reads as a floating chunk of ground
local function underside()
	for x = 0, 24 do
		for z = 0, 24 do
			if iso.height(x, z) > 0 then
				local d = math.sqrt((x - C) ^ 2 + (z - C) ^ 2)
				local depth = math.max(0.6, (8.0 - d) * 1.2 + math.sin(x * 1.7 + z * 2.3) * 0.8)
				if d > 7.6 then depth = 0.6 end -- bridges stay thin
				local v = 0.75 + 0.25 * math.abs(math.sin(x * 3.1 + z * 1.3))
				box(x, -0.5 - depth / 2, z, 1.0, depth, 1.0, 0.33 * v, 0.29 * v, 0.27 * v)
			end
		end
	end
	-- a few hanging rocks
	box(C, -11.5, C - 1, 1.2, 2.0, 1.2, 0.3, 0.27, 0.25)
	box(C + 2, -9.5, C + 1, 0.8, 1.4, 0.8, 0.3, 0.27, 0.25)
end

function on_start()
	iso.log("sky island")
	house()
	underside()

	-- railings along the narrow ways over the void
	railing(18.5, 11.5, 24.5, 11.5)   -- south road (spawn at its end)
	railing(18.5, 13.5, 24.5, 13.5)
	railing(10.5, 17.5, 10.5, 24.5)   -- east bridge
	railing(12.5, 17.5, 12.5, 24.5)
	railing(-0.5, 11.5, 4.5, 11.5)    -- north path
	railing(-0.5, 12.5, 4.5, 12.5)

	lamp(18.6, 11.2)                  -- where the south road meets the square
	lamp(10.2, 18.4)                  -- at the east bridge
	sign(18.4, 14.0, 1)
	sign(13.8, 18.4, 2)
	sign(5.6, 11.4, 1)                -- at the pier
	bench(12, 16.6, 1)
	bench(16.2, 12.0, 1)
	stall(15.6, 8.6)
	for _, p in ipairs({ { 8, 16, 1.1 }, { 8, 15, 0.8 }, { 16, 16, 1.2 }, { 17.4, 9.8, 0.8 }, { 9.5, 17.5, 0.8 },
	                     { 6.5, 13.6, 0.7 }, { 17, 15.5, 0.7 }, { 14, 6.2, 0.9 } }) do
		bush(p[1], p[2], p[3])
	end

	-- fireflies by the great tree, a brazier by the house
	iso.spawn_emitter("sparkle", 13.2, 3.4, 13.2)
	stand(9.6, 0, 9.6, 0.2, 0.7, 0.2, IRON[1], IRON[2], IRON[3])
	stand(9.6, 0.7, 9.6, 0.6, 0.22, 0.6, IRON[1], IRON[2], IRON[3])
	iso.spawn_emitter("fire", 9.6, FLOOR + 1.0, 9.6)

	-- a crystal orb on a 1 m pedestal
	stand(14.6, 0, 10.4, 0.6, 0.12, 0.6, 0.5, 0.48, 0.45)
	stand(14.6, 0.12, 10.4, 0.36, 0.78, 0.36, 0.55, 0.53, 0.5)
	stand(14.6, 0.9, 10.4, 0.54, 0.1, 0.54, 0.5, 0.48, 0.45)
	iso.spawn_glass(14.6, FLOOR + 1.0 + 0.35, 10.4, { radius = 0.35, ior = 1.6, dispersion = 0.05, tint = { 0.8, 0.85, 1.0 }, label = "orb" })

	-- someone to talk to, as tall as the player, with a quest marker above their head
	npc = iso.spawn_model("models/CesiumMan.glb", 10.2, FLOOR, 14.6, 1.72)
	iso.play_animation(npc, 0, 0)
	iso.set_yaw(npc, 120)
	marker = box(10.2, FLOOR + 2.15, 14.6, 0.14, 0.32, 0.14, 1.0, 0.85, 0.2, true)
end

function on_update(dt)
	-- the quest marker bobs
	if marker then
		iso.set_position(marker, 10.2, FLOOR + 2.15 + math.sin(iso.time() * 2.5) * 0.08, 14.6)
	end
end

function on_event(name, x, y, z)
	if name == "jump" then
		iso.play_sound("jump", 0.7)
	elseif name == "land" then
		iso.play_sound("land")
		iso.burst("dust", x, y + 0.05, z)
	elseif name == "respawn" then
		-- fell off the island
		iso.play_sound("chime")
		iso.burst("sparkle", x, y + 0.5, z)
	end
end
