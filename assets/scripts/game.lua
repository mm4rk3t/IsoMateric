-- IsoMateric game script. Saved changes are hot-reloaded (entities spawned here are
-- destroyed and on_start runs again). Engine API: the global table `iso`, see README.

local npc
local npc_angle = 0
local center_x, center_z = 7.5, 7.5
local radius = 6.2
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
end

function on_update(dt)
	-- walk the circle; the model faces along the tangent
	npc_angle = npc_angle + dt * 0.35
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
	elseif name == "respawn" then
		iso.play_sound("chime")
		iso.burst("sparkle", x, y + 0.5, z)
	end
end
