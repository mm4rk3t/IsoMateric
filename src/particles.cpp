#include "particles.h"
#include "components.h"

#include <algorithm>

ParticleEmitter particlePreset(const std::string& name)
{
	ParticleEmitter e;
	if (name == "fire")
	{
		e.rate = 60.0f;
		e.lifetime = 0.7f;
		e.velocity = {0.0f, 1.4f, 0.0f};
		e.spread = {0.25f, 0.4f, 0.25f};
		e.positionJitter = 0.12f;
		e.sizeStart = 0.16f;
		e.sizeEnd = 0.02f;
		e.colorStart = {1.0f, 0.75f, 0.25f, 0.9f};
		e.colorEnd = {0.9f, 0.15f, 0.05f, 0.0f};
		e.additive = true;
	}
	else if (name == "smoke")
	{
		e.rate = 12.0f;
		e.lifetime = 2.0f;
		e.velocity = {0.0f, 0.6f, 0.0f};
		e.spread = {0.15f, 0.2f, 0.15f};
		e.sizeStart = 0.12f;
		e.sizeEnd = 0.4f;
		e.colorStart = {0.35f, 0.35f, 0.35f, 0.5f};
		e.colorEnd = {0.2f, 0.2f, 0.2f, 0.0f};
	}
	else if (name == "sparkle")
	{
		e.rate = 20.0f;
		e.lifetime = 1.2f;
		e.velocity = {0.0f, 0.5f, 0.0f};
		e.spread = {0.6f, 0.4f, 0.6f};
		e.positionJitter = 0.3f;
		e.sizeStart = 0.07f;
		e.sizeEnd = 0.0f;
		e.colorStart = {0.6f, 0.8f, 1.0f, 1.0f};
		e.colorEnd = {0.3f, 0.5f, 1.0f, 0.0f};
		e.additive = true;
	}
	else if (name == "dust")
	{
		e.burst = 18;
		e.oneShot = true;
		e.lifetime = 0.5f;
		e.velocity = {0.0f, 0.6f, 0.0f};
		e.spread = {1.4f, 0.3f, 1.4f};
		e.positionJitter = 0.2f;
		e.gravity = 2.0f;
		e.drag = 3.0f;
		e.sizeStart = 0.12f;
		e.sizeEnd = 0.02f;
		e.colorStart = {0.75f, 0.7f, 0.6f, 0.8f};
		e.colorEnd = {0.75f, 0.7f, 0.6f, 0.0f};
	}
	return e;
}

void updateParticles(entt::registry& registry, float dt, std::mt19937& rng)
{
	std::uniform_real_distribution<float> unit(-1.0f, 1.0f);
	auto rand3 = [&]() { return glm::vec3(unit(rng), unit(rng), unit(rng)); };

	std::vector<entt::entity> finished;
	auto view = registry.view<Transform, ParticleEmitter>();
	for (auto entity : view)
	{
		const Transform& t = view.get<Transform>(entity);
		ParticleEmitter& e = view.get<ParticleEmitter>(entity);

		// age and integrate, dropping dead particles (order does not matter)
		for (size_t i = 0; i < e.particles.size();)
		{
			Particle& p = e.particles[i];
			p.age += dt;
			if (p.age >= p.life)
			{
				p = e.particles.back();
				e.particles.pop_back();
				continue;
			}
			p.velocity.y -= e.gravity * dt;
			p.velocity *= 1.0f / (1.0f + e.drag * dt);
			p.position += p.velocity * dt;
			i++;
		}

		// emit
		int count = e.burst;
		e.burst = 0;
		e.accumulator += e.rate * dt;
		count += static_cast<int>(e.accumulator);
		e.accumulator -= static_cast<float>(static_cast<int>(e.accumulator));
		count = std::min(count, e.maxParticles - static_cast<int>(e.particles.size()));
		for (int i = 0; i < count; i++)
		{
			Particle p;
			p.position = t.position + e.offset + rand3() * e.positionJitter;
			p.velocity = e.velocity + rand3() * e.spread;
			p.age = 0.0f;
			p.life = std::max(0.05f, e.lifetime * (1.0f + unit(rng) * e.lifetimeJitter));
			e.particles.push_back(p);
		}

		if (e.oneShot && e.particles.empty() && e.rate <= 0.0f)
			finished.push_back(entity);
	}
	for (auto entity : finished)
		registry.destroy(entity);
}
