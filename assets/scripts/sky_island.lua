-- Script for sky_island.level: a quiet floating town square.

function on_start()
	iso.log("sky island")
	-- fireflies by the great tree and a lantern flame by the house
	iso.spawn_emitter("sparkle", 11.2, 3.0, 11.2) -- fireflies by the tree
	iso.spawn_emitter("fire", 8.6, 1.7, 8.6)
	-- a single crystal ball on the plaza
	iso.spawn_glass(12.5, 0.95, 12.5, { radius = 0.45, ior = 1.6, dispersion = 0.05, tint = { 0.8, 0.85, 1.0 }, label = "orb" })
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
