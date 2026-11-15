#ifndef COMPONENTS_H
#define COMPONENTS_H

// ECS components (EnTT). Entities are dynamic things in the world: the player,
// characters and props spawned by scripts, particle emitters. The block grid itself
// stays a Level, since a dense grid is cheaper as an array than as entities.

#include <glm/glm.hpp>

#include <memory>
#include <string>
#include <vector>

class Model;

struct Transform
{
	glm::vec3 position{0.0f}; // feet / pivot
	float yaw = 0.0f;         // radians, around +y
};

struct MeshRenderer
{
	std::shared_ptr<Model> model;
	float height = 1.0f;     // model is scaled to this many world units tall
	float yawOffset = 0.0f;  // corrects the model's own forward axis
	glm::vec4 tint{1.0f};
	float squash = 1.0f;     // vertical scale on top of `height` (crouching)
};

struct Animator
{
	int clip = 0;
	float time = 0.0f;
	float speed = 1.0f;
	bool playing = true;
	std::vector<glm::mat4> globals; // filled by the animation system
	std::vector<glm::mat4> joints;
};

struct Name
{
	std::string value;
};

// created by Lua; removed when the script reloads
struct ScriptOwned { };

// A decorative box centred on Transform::position (no collision). Glowing boxes are
// drawn unlit, e.g. windows and lamp heads.
struct Box
{
	glm::vec3 size{1.0f};
	glm::vec4 color{1.0f};
	bool glow = false;     // unlit, full colour (drawn without lighting or shadows)
	float emissive = 0.0f; // lit as usual, plus its own colour times this: glows and blooms
};

// A refractive glass sphere centred on Transform::position.
struct Glass
{
	float radius = 0.4f;
	glm::vec3 tint{1.0f};     // colour absorbed through the thickness
	float ior = 1.5f;         // index of refraction: 1.0 air, 1.33 water, 1.5 glass, 2.4 diamond
	float roughness = 0.0f;   // 0 clear .. 1 frosted
	float absorption = 0.0f;  // how fast the tint builds up with thickness
	float dispersion = 0.0f;  // per-channel IOR spread (rainbow fringes)
};

// An item lying in the world, picked up with the interact key (see inventory.cpp).
struct Pickup
{
	std::string item;
};

// A flat, vertical mirror centred on the entity's Transform, facing along its yaw
// (yaw 0 faces +z, 90 degrees faces +x). Shows the opaque world (blocks, grass, models)
// reflected; only the front is a mirror, so give it a backing in the script.
struct Mirror
{
	glm::vec2 size{1.0f, 2.0f}; // width, height
	glm::vec3 tint{0.92f, 0.95f, 1.0f};
	float tarnish = 0.0f;       // 0 clean .. 1 old, blotchy silvering
};

#endif
