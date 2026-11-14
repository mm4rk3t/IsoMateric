#include "player.h"

#include <algorithm>
#include <cmath>

namespace
{
	// tile index of a world coordinate (tile n spans n-0.5 .. n+0.5)
	int tileOf(float c) { return static_cast<int>(std::floor(c + 0.5f)); }
	const float kSkin = 1e-3f;
}

void Player::placeAt(const Level& level, glm::ivec2 t)
{
	spawn = t;
	position = glm::vec3(static_cast<float>(t.x), level.surface(t.x, t.y) + size.y * 0.5f + kSkin, static_cast<float>(t.y));
	velocity = glm::vec3(0.0f);
	grounded = true;
	path.clear();
}

glm::ivec2 Player::tile() const
{
	return glm::ivec2(tileOf(position.x), tileOf(position.z));
}

Player::Events Player::update(float dt, const Level& level, glm::vec2 moveDir, bool jump)
{
	Events events;
	glm::vec2 delta(0.0f);
	const float step = speed * dt;

	if (glm::dot(moveDir, moveDir) > 0.0f)
	{
		path.clear();
		delta = moveDir * step;
	}
	else if (!path.empty())
	{
		glm::ivec2 next = path.front();
		glm::vec2 toTarget = glm::vec2(next) - glm::vec2(position.x, position.z);
		float dist = glm::length(toTarget);
		// path goes one block up: jump when grounded and close to the edge
		if (grounded && level.surface(next.x, next.y) > feet().y + 0.25f && dist < 1.0f)
			jump = true;
		if (dist <= step)
		{
			delta = toTarget;
			path.pop_front();
		}
		else
		{
			delta = toTarget / dist * step;
		}
	}

	if (jump && grounded)
	{
		velocity.y = jumpSpeed;
		grounded = false;
		events.jumped = true;
	}

	glm::vec3 before = position;
	bool blockedX = moveAxis(level, 0, delta.x);
	bool blockedZ = moveAxis(level, 2, delta.y);
	if ((blockedX || blockedZ) && !path.empty() && grounded)
	{
		// stuck against something the path did not expect (e.g. the level was edited)
		path.clear();
	}

	glm::vec2 moved(position.x - before.x, position.z - before.z);
	horizontalSpeed = dt > 0.0f ? glm::length(moved) / dt : 0.0f;
	if (glm::dot(moved, moved) > 1e-8f)
		facing = glm::normalize(moved);

	velocity.y = std::max(velocity.y - gravity * dt, -30.0f);
	bool wasGrounded = grounded;
	grounded = false;
	if (moveAxis(level, 1, velocity.y * dt))
	{
		if (velocity.y < 0.0f)
		{
			grounded = true;
			if (!wasGrounded)
				events.landed = true;
		}
		velocity.y = 0.0f;
	}

	if (position.y < killHeight)
	{
		placeAt(level, spawn);
		events.respawned = true;
	}
	return events;
}

bool Player::moveAxis(const Level& level, int axis, float delta)
{
	if (delta == 0.0f)
		return false;

	position[axis] += delta;

	const glm::vec3 half = size * 0.5f;
	const glm::vec3 lo = position - half, hi = position + half;
	const int x0 = tileOf(lo.x), x1 = tileOf(hi.x);
	const int y0 = tileOf(lo.y), y1 = tileOf(hi.y);
	const int z0 = tileOf(lo.z), z1 = tileOf(hi.z);

	float resolved = position[axis];
	bool hit = false;
	for (int x = x0; x <= x1; x++)
		for (int y = y0; y <= y1; y++)
			for (int z = z0; z <= z1; z++)
			{
				if (!level.solid(x, y, z))
					continue;
				hit = true;
				float edge = static_cast<float>(axis == 0 ? x : axis == 1 ? y : z);
				if (delta > 0.0f)
					resolved = std::min(resolved, edge - 0.5f - half[axis] - kSkin);
				else
					resolved = std::max(resolved, edge + 0.5f + half[axis] + kSkin);
			}
	if (hit)
		position[axis] = resolved;
	return hit;
}
