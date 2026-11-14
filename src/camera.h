#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>

struct Ray
{
	glm::vec3 origin;
	glm::vec3 dir;
};

// Orthographic camera orbiting a focus point at a fixed elevation, snapping between
// the four diagonal views and interpolating smoothly between them.
class Camera
{
public:
	enum class Style
	{
		Isometric, // true isometric, 35.26 deg elevation
		Dimetric   // 2:1 "pixel art" isometric, 30 deg elevation
	};

	Camera();

	void setViewport(int width, int height);
	void rotate(int steps);           // +1 / -1 quarter turns
	void update(float dt);            // eases yaw and focus towards their goals
	void snap();                      // jump straight to the goals

	glm::vec3 toCamera() const;       // unit vector from focus towards the eye
	glm::vec3 position() const;
	glm::vec3 forward() const;        // "up the screen" on the ground plane
	glm::vec3 right() const;          // "right on screen" on the ground plane
	float elevation() const;

	glm::mat4 view() const;
	glm::mat4 projection() const;
	glm::mat4 viewProjection() const { return projection() * view(); }

	// nx, ny in [0, 1], window coordinates (y grows downwards)
	Ray screenRay(float nx, float ny) const;

	Style style = Style::Isometric;
	glm::vec3 focus{0.0f};
	glm::vec3 focusGoal{0.0f};
	float zoom = 48.0f;      // pixels per world unit
	float distance = 100.0f; // eye distance from focus, far enough that nothing is clipped
	// world size of one output pixel block; when > 0 the view is snapped to it so a
	// pixelated image does not shimmer while the camera moves (0 = off)
	float snapUnits = 0.0f;

private:
	int viewportW = 800, viewportH = 600;
	float yaw, yawGoal; // radians
};

#endif
