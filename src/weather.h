#ifndef WEATHER_H
#define WEATHER_H

#include "level.h"

#include <glm/glm.hpp>

#include <random>
#include <vector>

// Rain simulated in a box around the camera focus. Drops that hit a block top turn
// into short-lived splashes; drops that leave the box re-enter on the opposite side,
// so the volume follows the camera without visible popping.
class Rain
{
public:
	struct Drop
	{
		glm::vec3 position;
		float speed;
	};
	struct Splash
	{
		glm::vec3 position;
		glm::vec3 velocity;
		float age;
	};

	// intensity 0..1 scales the number of drops; wind is (x, z) units per second
	void update(float dt, const Level& level, glm::vec3 center, float intensity, glm::vec2 wind, std::mt19937& rng);

	std::vector<Drop> drops;
	std::vector<Splash> splashes;
	int maxDrops = 2500;
	float halfExtent = 14.0f; // half size of the rain box around the centre
	float height = 14.0f;     // drops start up to this far above the centre
	float splashLife = 0.25f;
	int hitsLastUpdate = 0;
};

#endif
