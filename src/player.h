#ifndef PLAYER_H
#define PLAYER_H

#include "level.h"

#include <glm/glm.hpp>

#include <deque>

// Character controller: an axis-aligned box moved through the block grid with
// gravity, jumping and per-axis collision (so it slides along walls).
class Player
{
public:
	struct Events
	{
		bool jumped = false;
		bool landed = false;
		bool respawned = false;
	};

	// moveDir: desired ground-plane direction (x, z), length 0..1. Keyboard input
	// cancels any click-to-move path.
	Events update(float dt, const Level& level, glm::vec2 moveDir, bool jump);
	void placeAt(const Level& level, glm::ivec2 tile);
	glm::ivec2 tile() const;
	glm::vec3 feet() const { return position - glm::vec3(0.0f, size.y * 0.5f, 0.0f); }

	glm::vec3 position{0.0f, 1.0f, 0.0f}; // centre of the body
	glm::vec3 size{0.5f, 1.0f, 0.5f};     // full extents
	glm::vec3 velocity{0.0f};
	glm::vec2 facing{0.0f, 1.0f};          // last horizontal move direction
	float horizontalSpeed = 0.0f;          // actual speed this step, for animation
	bool grounded = false;

	float speed = 3.5f;       // world units per second
	float gravity = 25.0f;
	float jumpSpeed = 8.2f;   // clears one block (apex ~1.35)
	float killHeight = -10.0f;
	std::deque<glm::ivec2> path;

private:
	bool moveAxis(const Level& level, int axis, float delta); // true if blocked
	glm::ivec2 spawn{0, 0};
};

#endif
