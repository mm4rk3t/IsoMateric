-- sky_island.level: a quiet town square floating in space, rebuilt from the concept sketch.
-- Everything decorative is built here with iso.spawn_box; the walkable ground is the level.

local npc
local marker
local C = 12 -- island centre

local function box(x, y, z, sx, sy, sz, r, g, b, glow)
	return iso.spawn_box(x, y, z, sx, sy, sz, r, g, b, glow)
end

-- a lamp post standing on the floor at (x, z); the level file puts a light at its head
local function lamp(x, z)
	box(x, 1.1, z, 0.12, 1.2, 0.12, 0.18, 0.16, 0.16)
	box(x, 1.75, z, 0.3, 0.08, 0.3, 0.18, 0.16, 0.16)
	box(x, 1.95, z, 0.22, 0.3, 0.22, 1.0, 0.8, 0.45, true)
end

-- a signpost; dir 1 = board along x, 2 = along z
local function sign(x, z, dir)
	box(x, 0.95, z, 0.1, 0.9, 0.1, 0.35, 0.24, 0.14)
	if dir == 1 then
		box(x, 1.3, z, 0.8, 0.35, 0.06, 0.62, 0.45, 0.26)
	else
		box(x, 1.3, z, 0.06, 0.35, 0.8, 0.62, 0.45, 0.26)
	end
end

local function bench(x, z)
	box(x, 0.85, z, 1.4, 0.1, 0.42, 0.52, 0.34, 0.2)
	box(x, 1.12, z - 0.18, 1.4, 0.32, 0.07, 0.52, 0.34, 0.2)
	box(x - 0.6, 0.67, z, 0.08, 0.3, 0.38, 0.2, 0.18, 0.18)
	box(x + 0.6, 0.67, z, 0.08, 0.3, 0.38, 0.2, 0.18, 0.18)
end

local function bush(x, z, s)
	box(x, 0.5 + s * 0.45, z, s, s * 0.9, s, 0.22, 0.42, 0.22)
	box(x + s * 0.3, 0.5 + s * 0.7, z - s * 0.2, s * 0.7, s * 0.6, s * 0.7, 0.26, 0.5, 0.25)
end

-- the house on tiles 6..8: stepped roof, door towards the square, glowing windows
local function house()
	local hx, hz = 7, 7
	local roof = { { 3.6, 3.75 }, { 2.8, 4.15 }, { 2.0, 4.55 }, { 1.2, 4.95 } }
	for i, step in ipairs(roof) do
		local shade = 0.85 + i * 0.04
		box(hx, step[2], hz, step[1], 0.4, step[1], 0.55 * shade, 0.22 * shade, 0.18 * shade)
	end
	box(hx + 0.6, 5.6, hz - 0.6, 0.35, 1.0, 0.35, 0.4, 0.36, 0.34) -- chimney
	box(8.53, 1.1, 7, 0.06, 1.2, 0.7, 0.32, 0.2, 0.12)              -- door
	box(8.53, 1.1, 7.25, 0.08, 0.08, 0.08, 0.9, 0.8, 0.3, true)     -- door knob
	box(7, 2.3, 8.53, 0.7, 0.55, 0.06, 1.0, 0.82, 0.45, true)        -- window
	box(8.53, 2.4, 6.2, 0.06, 0.5, 0.5, 1.0, 0.82, 0.45, true)       -- side window
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

	lamp(18.4, 11.2)          -- at the "start here" bridge
	lamp(10.6, 18.4)          -- at the long road
	sign(18.6, 14.4, 1)       -- "start here"
	sign(13.6, 18.6, 2)
	sign(5.6, 11.4, 1)        -- at the pier: "zoom in / below"
	bench(12, 16.2)
	bench(15.6, 12)
	for _, p in ipairs({ { 15, 8, 0.9 }, { 16, 8, 0.8 }, { 8, 16, 0.9 }, { 8, 15, 0.7 }, { 16, 16, 1.0 },
	                     { 17.5, 9.5, 0.6 }, { 9.5, 17.5, 0.6 }, { 6.5, 13.6, 0.55 }, { 17, 15.5, 0.6 } }) do
		bush(p[1], p[2], p[3])
	end

	-- fireflies by the great tree, a flame in a brazier by the house
	iso.spawn_emitter("sparkle", 13.2, 3.0, 13.2)
	box(9.4, 0.75, 9.0, 0.45, 0.5, 0.45, 0.25, 0.22, 0.22)
	iso.spawn_emitter("fire", 9.4, 1.1, 9.0)

	-- a crystal orb on a pedestal
	box(14.6, 0.85, 10.4, 0.5, 0.7, 0.5, 0.55, 0.53, 0.5)
	iso.spawn_glass(14.6, 1.55, 10.4, { radius = 0.35, ior = 1.6, dispersion = 0.05, tint = { 0.8, 0.85, 1.0 }, label = "orb" })

	-- someone to talk to, with a quest marker above their head
	npc = iso.spawn_model("models/CesiumMan.glb", 10.2, 0.5, 14.6, 1.1)
	iso.play_animation(npc, 0, 0)
	iso.set_yaw(npc, 120)
	marker = box(10.2, 2.05, 14.6, 0.14, 0.32, 0.14, 1.0, 0.85, 0.2, true)
end

function on_update(dt)
	-- the quest marker bobs
	if marker then
		iso.set_position(marker, 10.2, 2.05 + math.sin(iso.time() * 2.5) * 0.08, 14.6)
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
