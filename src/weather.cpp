#include "weather.h"

#include <algorithm>
#include <cmath>

namespace
{
	int tileOf(float c) { return static_cast<int>(std::floor(c + 0.5f)); }
}

void Rain::update(float dt, const Level& level, glm::vec3 center, float intensity, glm::vec2 wind, std::mt19937& rng)
{
	std::uniform_real_distribution<float> unit(0.0f, 1.0f);
	const float bottom = center.y - 6.0f;
	const float top = center.y + height;
	auto spawnAt = [&](float y) {
		return Drop{glm::vec3(center.x + (unit(rng) * 2.0f - 1.0f) * halfExtent, y, center.z + (unit(rng) * 2.0f - 1.0f) * halfExtent),
		            14.0f + unit(rng) * 4.0f};
	};

	// grow or shrink towards the requested density; new drops fill the whole column
	const size_t target = static_cast<size_t>(std::clamp(intensity, 0.0f, 1.0f) * static_cast<float>(maxDrops));
	while (drops.size() < target)
		drops.push_back(spawnAt(bottom + unit(rng) * (top - bottom)));
	if (drops.size() > target)
		drops.resize(target);

	hitsLastUpdate = 0;
	for (Drop& d : drops)
	{
		d.position += glm::vec3(wind.x, -d.speed, wind.y) * dt;

		// keep the box around the centre: wrap horizontally
		for (int axis : {0, 2})
		{
			float rel = d.position[axis] - center[axis];
			if (rel > halfExtent)
				d.position[axis] -= 2.0f * halfExtent;
			else if (rel < -halfExtent)
				d.position[axis] += 2.0f * halfExtent;
		}

		int tx = tileOf(d.position.x), tz = tileOf(d.position.z);
		bool hit = level.standable(tx, tz) && d.position.y <= level.surface(tx, tz);
		if (hit)
		{
			hitsLastUpdate++;
			glm::vec3 p(d.position.x, level.surface(tx, tz) + 0.02f, d.position.z);
			for (int i = 0; i < 2; i++)
			{
				float a = unit(rng) * 6.2831853f;
				splashes.push_back({p, glm::vec3(std::cos(a) * 0.8f, 1.2f + unit(rng), std::sin(a) * 0.8f), 0.0f});
			}
		}
		if (hit || d.position.y < bottom)
			d = spawnAt(top - unit(rng) * 2.0f);
	}

	for (size_t i = 0; i < splashes.size();)
	{
		Splash& s = splashes[i];
		s.age += dt;
		if (s.age >= splashLife)
		{
			s = splashes.back();
			splashes.pop_back();
			continue;
		}
		s.velocity.y -= 9.0f * dt;
		s.position += s.velocity * dt;
		i++;
	}
}
