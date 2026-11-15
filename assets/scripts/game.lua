-- IsoMateric game script. Saved changes are hot-reloaded (entities spawned here are
-- destroyed and on_start runs again). Engine API: the global table `iso`, see README.

local flames = {}

-- glow: unlit; alpha < 1: see-through (glass); emissive > 0: lit, plus its own glow
local function box(x, y, z, sx, sy, sz, r, g, b, glow, alpha, emissive)
	return iso.spawn_box(x, y, z, sx, sy, sz, r, g, b, glow or false, alpha or 1, emissive or 0)
end

-- dark stone, matching the 'S' pillar tiles; shade < 1 for mouldings
local function stone(x, y, z, sx, sy, sz, shade)
	local s = (shade or 1) * (0.96 + 0.08 * math.sin(x * 12.9 + y * 7.1))
	box(x, y, z, sx, sy, sz, 0.44 * s, 0.42 * s, 0.42 * s)
end

-- A pointed (equilateral) gothic arch across the path. The pillars are level columns
-- (x 29-30 and 35-36, z 31-32, 6 high), so they are solid; everything above the
-- opening is decoration built here, so the player walks through underneath.
local function build_arch()
	local zc, depth = 31.5, 2.0
	local left, right = 30.5, 34.5 -- inner faces of the pillars
	local span = right - left
	local spring = 4.0             -- where the pointed curve starts
	local top = 8.0                -- top of the wall

	-- wall over the pillars (the level columns end at y 5.5)
	for _, px in ipairs({ 29.5, 35.5 }) do
		stone(px, (5.5 + top) * 0.5, zc, 2.0, top - 5.5, depth)
		stone(px, 0.7, zc, 2.3, 0.4, depth + 0.3, 0.8)    -- plinth
		stone(px, spring, zc, 2.3, 0.25, depth + 0.3, 0.8) -- impost where the arch springs
	end

	-- the curve: each side is an arc of radius `span` centred on the opposite pillar
	local function curve(x)
		local c = x <= left + span * 0.5 and right or left
		return spring + math.sqrt(span * span - (x - c) * (x - c))
	end
	local n = 24
	local w = span / n
	for i = 0, n - 1 do
		local x = left + (i + 0.5) * w
		local y = curve(x)
		stone(x, (y + top) * 0.5, zc, w, top - y, depth) -- spandrel strip above the opening
		-- archivolt: a darker, deeper band tracing the curve, thicker where it is steep
		local dy = math.abs(curve(math.min(x + w, right - 0.001)) - y)
		local band = math.max(0.22, dy + 0.08)
		stone(x, y + band * 0.5 - 0.05, zc, w, band, depth + 0.24, 0.72)
	end

	-- cornice, pinnacles and a cross on the apex
	stone(32.5, top + 0.12, zc, 8.6, 0.25, depth + 0.4, 0.8)
	for _, px in ipairs({ 29.5, 35.5 }) do
		local y = top + 0.25
		for _, s in ipairs({ 1.0, 0.7, 0.45, 0.22, 0.1 }) do
			stone(px, y + 0.35, zc, s, 0.7, s)
			y = y + 0.7
		end
	end
	stone(32.5, top + 0.85, zc, 0.16, 1.2, 0.16, 0.7)
	stone(32.5, top + 1.05, zc, 0.65, 0.16, 0.16, 0.7)
end

-- A tall gothic standing mirror on the grass: dark wooden frame with a pointed top,
-- a backing so the back is not empty, and feet. facing = "x-" faces -x, "z+" faces +z.
local function standing_mirror(x, z, facing, tarnish)
	local w, h = 1.3, 2.3
	local cy = 0.5 + 0.35 + h * 0.5         -- glass centre: floor top + feet + half height
	local yaw = facing == "x-" and -90 or 0
	-- along = the glass's width direction, n = the direction it faces
	local ax, az = 1, 0
	local nx, nz = 0, 1
	if facing == "x-" then ax, az, nx, nz = 0, 1, -1, 0 end
	local wood = { 0.16, 0.10, 0.08 }
	local function part(u, v, depth, su, sv, sd, col)
		-- u along the glass, v up, depth along the normal (negative = behind the glass)
		local px, pz = x + ax * u + nx * depth, z + az * u + nz * depth
		local sx = math.abs(ax) * su + math.abs(nx) * sd
		local sz = math.abs(az) * su + math.abs(nz) * sd
		box(px, v, pz, sx, sv, sz, col[1], col[2], col[3])
	end
	part(0, cy, -0.09, w + 0.2, h + 0.2, 0.1, wood)                 -- backing
	part(-(w + 0.1) * 0.5, cy, -0.02, 0.12, h + 0.2, 0.12, wood)     -- sides
	part((w + 0.1) * 0.5, cy, -0.02, 0.12, h + 0.2, 0.12, wood)
	part(0, cy - h * 0.5 - 0.06, -0.02, w + 0.3, 0.12, 0.14, wood)   -- bottom rail
	part(0, cy + h * 0.5 + 0.06, -0.02, w + 0.3, 0.12, 0.14, wood)   -- top rail
	for i = 0, 3 do                                                  -- pointed crest
		local s = (w + 0.2) * (1 - i / 4)
		part(0, cy + h * 0.5 + 0.2 + i * 0.16, -0.05, s, 0.16, 0.12, wood)
	end
	part(0, cy + h * 0.5 + 0.95, -0.05, 0.08, 0.3, 0.08, wood)      -- finial
	part(-w * 0.4, 0.68, -0.05, 0.14, 0.36, 0.5, wood)               -- feet
	part(w * 0.4, 0.68, -0.05, 0.14, 0.36, 0.5, wood)
	iso.spawn_mirror(x + nx * 0.03, cy, z + nz * 0.03, w, h, yaw, { tarnish = tarnish, tint = { 0.9, 0.92, 0.95 } })
end

-- A Victorian greenhouse: brick base, iron frame, glass walls and a pitched glass roof.
-- The floor and dirt beds are level tiles (x 8-16, z 38-46); the level file has the lamp.
local function build_greenhouse()
	local x0, x1, z0, z1 = 7.6, 16.4, 37.6, 46.4 -- outer walls
	local cx, cz = (x0 + x1) * 0.5, (z0 + z1) * 0.5
	local floor, base, eave, ridge = 0.5, 1.0, 3.0, 4.3
	local iron = { 0.09, 0.09, 0.1 }
	local glass = { 0.72, 0.86, 0.82 }
	local door0, door1 = cx - 0.6, cx + 0.6 -- door in the +z wall
	local function i(x, y, z, sx, sy, sz) box(x, y, z, sx, sy, sz, iron[1], iron[2], iron[3]) end
	local function pane(x, y, z, sx, sy, sz) box(x, y, z, sx, sy, sz, glass[1], glass[2], glass[3], false, 0.22) end
	local function brick(x, y, z, sx, sy, sz) box(x, y, z, sx, sy, sz, 0.38, 0.22, 0.17) end

	-- brick base wall, with a gap for the door
	brick(cx, (floor + base) * 0.5, z0, x1 - x0, base - floor, 0.22)
	brick((x0 + door0) * 0.5, (floor + base) * 0.5, z1, door0 - x0, base - floor, 0.22)
	brick((door1 + x1) * 0.5, (floor + base) * 0.5, z1, x1 - door1, base - floor, 0.22)
	brick(x0, (floor + base) * 0.5, cz, 0.22, base - floor, z1 - z0)
	brick(x1, (floor + base) * 0.5, cz, 0.22, base - floor, z1 - z0)

	-- glass walls between the base and the eaves
	local h = eave - base
	pane(cx, base + h * 0.5, z0, x1 - x0, h, 0.04)
	pane(x0, base + h * 0.5, cz, 0.04, h, z1 - z0)
	pane(x1, base + h * 0.5, cz, 0.04, h, z1 - z0)
	pane((x0 + door0) * 0.5, base + h * 0.5, z1, door0 - x0, h, 0.04)
	pane((door1 + x1) * 0.5, base + h * 0.5, z1, x1 - door1, h, 0.04)
	pane(cx, 2.6 + (eave - 2.6) * 0.5, z1, door1 - door0, eave - 2.6, 0.04) -- transom over the door

	-- iron posts every ~1.5 units, a rail at mid height, the eave beam
	for x = x0, x1 + 0.01, (x1 - x0) / 6 do
		i(x, (floor + eave) * 0.5, z0, 0.09, eave - floor, 0.09)
		if x < door0 - 0.05 or x > door1 + 0.05 then
			i(x, (floor + eave) * 0.5, z1, 0.09, eave - floor, 0.09)
		end
	end
	for z = z0, z1 + 0.01, (z1 - z0) / 6 do
		i(x0, (floor + eave) * 0.5, z, 0.09, eave - floor, 0.09)
		i(x1, (floor + eave) * 0.5, z, 0.09, eave - floor, 0.09)
	end
	i(door0, (floor + 2.6) * 0.5, z1, 0.09, 2.6 - floor, 0.09) -- door frame
	i(door1, (floor + 2.6) * 0.5, z1, 0.09, 2.6 - floor, 0.09)
	i(cx, 2.6, z1, door1 - door0, 0.08, 0.09)
	for _, y in ipairs({ 2.0, eave }) do
		i(cx, y, z0, x1 - x0, 0.07, 0.08)
		i(x0, y, cz, 0.08, 0.07, z1 - z0)
		i(x1, y, cz, 0.08, 0.07, z1 - z0)
		i((x0 + door0) * 0.5, y, z1, door0 - x0, 0.07, 0.08)
		i((door1 + x1) * 0.5, y, z1, x1 - door1, 0.07, 0.08)
	end

	-- the door, standing open against the wall outside
	i(door0 - 0.02, (floor + 2.55) * 0.5, z1 + 0.62, 0.06, 2.05, 1.15)
	pane(door0 - 0.02, (floor + 2.55) * 0.5 + 0.05, z1 + 0.62, 0.03, 1.8, 0.95)

	-- pitched roof, ridge along x: stepped glass slabs and iron rafters on both slopes
	local steps = 10
	local run = (z1 - z0) * 0.5
	for side = -1, 1, 2 do
		for k = 0, steps - 1 do
			local t = (k + 0.5) / steps
			local z = cz + side * run * (1 - t)
			local y = eave + (ridge - eave) * t
			pane(cx, y, z, x1 - x0 + 0.2, 0.05, run / steps + 0.02)
			for x = x0, x1 + 0.01, (x1 - x0) / 6 do
				i(x, y + 0.04, z, 0.07, 0.07, run / steps + 0.02)
			end
		end
		-- gable ends: glass triangles, stepped
		for k = 0, steps - 1 do
			local t = (k + 0.5) / steps
			local hz = run * (1 - t)
			local y = eave + (ridge - eave) * t
			pane(x0, y - (ridge - eave) / steps * 0.5, cz, 0.04, (ridge - eave) / steps, hz * 2)
			pane(x1, y - (ridge - eave) / steps * 0.5, cz, 0.04, (ridge - eave) / steps, hz * 2)
		end
	end
	i(cx, ridge + 0.06, cz, x1 - x0 + 0.3, 0.1, 0.12) -- ridge beam
	for _, x in ipairs({ x0, x1 }) do                -- finials
		i(x, ridge + 0.35, cz, 0.08, 0.5, 0.08)
		i(x, ridge + 0.62, cz, 0.16, 0.16, 0.16)
	end

	-- raised wooden beds along both long sides, crowded with plants, a few dead
	local wood = { 0.36, 0.24, 0.15 }
	for _, bx in ipairs({ 9.5, 15.5 }) do
		box(bx, 0.72, 42.0, 1.8, 0.44, 6.8, wood[1], wood[2], wood[3])
		box(bx, 0.95, 42.0, 1.6, 0.04, 6.6, 0.22, 0.15, 0.1) -- soil
		for k = 0, 13 do
			local pz = 39.0 + k * 0.46
			for c = -1, 1, 2 do
				local seed = math.sin(bx * 3.1 + k * 1.7 + c) * 0.5 + 0.5
				local px = bx + c * 0.4 + (seed - 0.5) * 0.2
				local hgt = 0.25 + seed * 0.55
				if seed > 0.82 then -- dead stalk
					box(px, 0.97 + hgt * 0.4, pz, 0.06, hgt * 0.8, 0.06, 0.32, 0.26, 0.16)
				else
					local g = 0.55 + seed * 0.5
					box(px, 0.97 + hgt * 0.5, pz, 0.3 + seed * 0.15, hgt, 0.3, 0.16 * g, 0.36 * g, 0.14 * g)
					if seed > 0.62 then -- flowers, dark red
						box(px, 0.97 + hgt + 0.05, pz, 0.12, 0.1, 0.12, 0.5, 0.06, 0.1)
					end
				end
			end
		end
		-- pale glowing mushrooms in the shade of the bed
		for k = 0, 3 do
			local pz = 39.4 + k * 1.7
			box(bx + 0.95, 0.56, pz, 0.05, 0.12, 0.05, 0.75, 0.82, 0.7)
			box(bx + 0.95, 0.64, pz, 0.14, 0.05, 0.14, 0.45, 0.9, 0.75, false, 1, 1.6)
		end
	end

	-- potting table in the middle with pots; the flashlight lies on it
	box(cx, 1.3, 40.6, 2.0, 0.08, 0.9, wood[1], wood[2], wood[3])
	for _, lx in ipairs({ -0.9, 0.9 }) do
		for _, lz in ipairs({ -0.38, 0.38 }) do
			box(cx + lx, 0.9, 40.6 + lz, 0.08, 0.8, 0.08, wood[1] * 0.8, wood[2] * 0.8, wood[3] * 0.8)
		end
	end
	for k, px in ipairs({ -0.65, 0.55 }) do
		box(cx + px, 1.47, 40.5, 0.28, 0.26, 0.28, 0.6, 0.3, 0.18) -- terracotta
		box(cx + px, 1.63 + k * 0.05, 40.5, 0.22, 0.1 + k * 0.1, 0.22, 0.2, 0.4, 0.15)
	end
	iso.spawn_pickup("flashlight", cx + 0.05, 1.42, 40.7)

	-- watering can and a sack on the floor
	box(cx + 2.2, 0.72, 45.2, 0.34, 0.4, 0.22, 0.32, 0.38, 0.36)
	box(cx + 2.42, 0.85, 45.2, 0.2, 0.05, 0.05, 0.32, 0.38, 0.36)
	box(cx - 2.2, 0.8, 39.0, 0.5, 0.6, 0.4, 0.55, 0.47, 0.32)

	-- hanging lamp under the ridge (the level's light sits in it)
	i(cx, 3.6, cz, 0.03, 1.3, 0.03)
	i(cx, 2.95, cz, 0.3, 0.06, 0.3)
	box(cx, 2.78, cz, 0.2, 0.26, 0.2, 1.0, 0.75, 0.45, false, 1, 2.2)
end

-- iron sconce under a torch light
local function sconce(x, y, z, wall_z)
	local dir = z < wall_z and 1 or -1 -- towards the pillar face
	box(x, y - 0.9, z + dir * 0.22, 0.1, 0.35, 0.1, 0.12, 0.11, 0.11)
	box(x, y - 0.68, z + dir * 0.1, 0.1, 0.08, 0.3, 0.12, 0.11, 0.11)
	box(x, y - 0.66, z, 0.26, 0.12, 0.26, 0.15, 0.13, 0.12)
end

function on_start()
	iso.log("game.lua started")

	build_arch()

	-- two old mirrors: one beside the path facing it, one facing back at the arch
	standing_mirror(35.0, 22.5, "x-", 0.25)
	standing_mirror(27.5, 25.5, "z+", 0.6)

	-- a torch at every point light: sconce, flame and a flickering light
	build_greenhouse()

	-- a torch at every point light on the arch: sconce, flame and a flickering light;
	-- other lights (the greenhouse lamp) only flicker gently
	for i = 1, iso.light_count() do
		local x, y, z = iso.get_light(i)
		if x > 28 and x < 37 and z > 29 and z < 34 then
			flames[i] = { base = 1.2, phase = i * 1.7, amount = 1 }
			sconce(x, y, z, 31.5)
			iso.spawn_emitter("fire", x, y - 0.6, z)
		else
			flames[i] = { base = 1.1, phase = i * 1.7, amount = 0.3 }
		end
	end

	-- things to find
	iso.spawn_pickup("rusty_key", 33.6, 0.58, 34.2)  -- just past the arch
	iso.spawn_pickup("torn_note", 33.9, 0.52, 21.6)  -- at the foot of the mirror

	-- glass test row beside the path (x = 40), resting on the grass
	local r = 0.4
	local row = {
		{ label = "clear 1.5",        ior = 1.5 },
		{ label = "rough 0.3",        ior = 1.5, roughness = 0.3 },
		{ label = "frosted 0.8",      ior = 1.5, roughness = 0.8 },
		{ label = "water 1.33",       ior = 1.33, tint = { 0.8, 0.95, 1.0 }, absorption = 0.4 },
		{ label = "diamond 2.4",      ior = 2.4, dispersion = 0.06 },
		{ label = "bottle green",     ior = 1.5, tint = { 0.35, 0.8, 0.4 }, absorption = 2.0 },
		{ label = "amber dense",      ior = 1.55, tint = { 1.0, 0.6, 0.15 }, absorption = 4.0, roughness = 0.1 },
	}
	for i, opts in ipairs(row) do
		opts.radius = r
		iso.spawn_glass(40, 0.5 + r, 18 + 2 * i, opts)
	end
end

function on_update(dt)
	-- flickering lights
	local t = iso.time()
	for i, f in pairs(flames) do
		local flicker = (math.sin(t * 13 + f.phase) * 0.08 + math.sin(t * 7.3 + f.phase * 2) * 0.1) * f.amount
		iso.set_light_intensity(i, f.base + flicker)
	end
end

function on_event(name, x, y, z)
	if name == "jump" then
		iso.play_sound("jump", 0.7)
	elseif name == "land" then
		iso.play_sound("land")
		iso.burst("dust", x, y + 0.05, z)
	elseif name == "blood_moon_start" then
		iso.log("the blood moon rises...")
	elseif name == "respawn" then
		iso.play_sound("chime")
		iso.burst("sparkle", x, y + 0.5, z)
	end
end
