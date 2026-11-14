-- IsoMateric game script. Saved changes are hot-reloaded (entities spawned here are
-- destroyed and on_start runs again). Engine API: the global table `iso`, see README.

local flames = {}

local function box(x, y, z, sx, sy, sz, r, g, b, glow)
	return iso.spawn_box(x, y, z, sx, sy, sz, r, g, b, glow)
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

	-- a torch at every point light: sconce, flame and a flickering light
	for i = 1, iso.light_count() do
		local x, y, z = iso.get_light(i)
		flames[i] = { base = 1.2, phase = i * 1.7 }
		sconce(x, y, z, 31.5)
		iso.spawn_emitter("fire", x, y - 0.6, z)
	end

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
	for i, f in ipairs(flames) do
		local flicker = math.sin(t * 13 + f.phase) * 0.08 + math.sin(t * 7.3 + f.phase * 2) * 0.1
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
