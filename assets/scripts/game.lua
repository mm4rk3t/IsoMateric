-- IsoMateric game script. Saved changes are hot-reloaded (entities spawned here are
-- destroyed and on_start runs again). Engine API: the global table `iso`, see README.

local npc
local npc_angle = 0
local npc_speed = 0.35
local center_x, center_z = 7.5, 7.5
local radius = 5.2
local flames = {}

function on_start()
	iso.log("game.lua started")

	-- a second character walking laps around the arena
	npc = iso.spawn_model("models/CesiumMan.glb", center_x + radius, 0.5, center_z, 1.1)
	iso.play_animation(npc, 0, 1.0)

	-- fire above every point light, plus some sparkles over the central pillar
	for i = 1, iso.light_count() do
		local x, y, z = iso.get_light(i)
		flames[i] = { base = 1.2, phase = i * 1.7 }
		iso.spawn_emitter("fire", x, y - 0.6, z)
	end
	iso.spawn_emitter("sparkle", 7.5, 3.2, 7.5)

	-- glass test row along the right edge (x = 14), resting on the floor
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
		iso.spawn_glass(14, 0.5 + r, 2 * i - 1, opts)
	end
end

function on_update(dt)
	-- walk the circle; the model faces along the tangent
	npc_angle = npc_angle + dt * npc_speed
	local x = center_x + math.cos(npc_angle) * radius
	local z = center_z + math.sin(npc_angle) * radius
	local ground = iso.height(math.floor(x + 0.5), math.floor(z + 0.5)) - 0.5
	iso.set_position(npc, x, ground, z)
	iso.set_yaw(npc, math.deg(math.atan(-math.sin(npc_angle), math.cos(npc_angle))))

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
		iso.set_tint(npc, 1.0, 0.35, 0.3)
		npc_speed = 0.9 -- it walks faster under the blood moon
	elseif name == "blood_moon_end" then
		iso.set_tint(npc, 1, 1, 1)
		npc_speed = 0.35
	elseif name == "respawn" then
		iso.play_sound("chime")
		iso.burst("sparkle", x, y + 0.5, z)
	end
end
