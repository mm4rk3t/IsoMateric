#ifndef PARTICLES_H
#define PARTICLES_H

#include <entt.hpp>
#include <glm/glm.hpp>

#include <random>
#include <string>
#include <vector>

struct Particle
{
	glm::vec3 position;
	glm::vec3 velocity;
	float age;
	float life;
};

// CPU particle emitter component. Lives on an entity with a Transform; particles are
// simulated in world space, so moving the emitter leaves a trail.
struct ParticleEmitter
{
	glm::vec3 offset{0.0f};
	float rate = 0.0f;        // particles per second
	int burst = 0;            // emitted once on the next update
	bool oneShot = false;     // destroy the entity once the burst has died out
	float lifetime = 1.0f;
	float lifetimeJitter = 0.3f;
	glm::vec3 velocity{0.0f, 1.0f, 0.0f};
	glm::vec3 spread{0.3f};   // random +- added to velocity
	float positionJitter = 0.1f;
	float gravity = 0.0f;
	float drag = 0.0f;
	float sizeStart = 0.15f, sizeEnd = 0.0f;
	glm::vec4 colorStart{1.0f}, colorEnd{1.0f, 1.0f, 1.0f, 0.0f};
	bool additive = false;
	int maxParticles = 512;

	float accumulator = 0.0f;
	std::vector<Particle> particles;
};

// "fire", "smoke", "sparkle", "dust"; unknown names give a plain white emitter
ParticleEmitter particlePreset(const std::string& name);

void updateParticles(entt::registry& registry, float dt, std::mt19937& rng);

#endif
