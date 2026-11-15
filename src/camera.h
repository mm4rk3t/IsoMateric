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

	// Isometric is the engine's home; first person is there to try things out
	enum class Mode
	{
		Isometric,
		FirstPerson
	};

	Camera();

	void setMode(Mode m);
	Mode mode() const { return currentMode; }
	void look(float dYaw, float dPitch); // mouse look in first person

	void setViewport(int width, int height);
	void rotate(int steps);           // +1 / -1 quarter turns
	void update(float dt);            // eases yaw and focus towards their goals
	void snap();                      // jump straight to the goals

	glm::vec3 toCamera() const;       // unit vector from focus towards the eye
	glm::vec3 position() const;
	glm::vec3 forward() const;        // "up the screen" on the ground plane
	glm::vec3 right() const;          // "right on screen" on the ground plane
	float elevation() const;
	float nearPlane() const { return isPerspective() ? 0.05f : 0.1f; }
	float farPlane() const { return isPerspective() ? 300.0f : distance * 3.0f; }
	bool isPerspective() const { return currentMode != Mode::Isometric; }
	float fieldOfView() const { return glm::radians(fov); } // vertical, first person

	glm::mat4 view() const;
	glm::mat4 projection() const;
	glm::mat4 viewProjection() const { return projection() * view(); }

	// nx, ny in [0, 1], window coordinates (y grows downwards)
	Ray screenRay(float nx, float ny) const;

	Style style = Style::Dimetric;
	glm::vec3 focus{0.0f};
	glm::vec3 focusGoal{0.0f};
	float zoom = 55.0f;      // pixels per world unit
	float distance = 100.0f; // eye distance from focus, far enough that nothing is clipped
	// world size of one output pixel block; when > 0 the view is snapped to it so a
	// pixelated image does not shimmer while the camera moves (0 = off)
	float snapUnits = 0.0f;

	float fov = 70.0f;           // degrees, first person
	float eyeOffset = 0.35f;     // first-person eye above the focus (the player's centre)

private:
	glm::vec3 eye() const;       // first-person eye position

	int viewportW = 800, viewportH = 600;
	float yaw, yawGoal; // radians
	Mode currentMode = Mode::Isometric;
	float lookPitch = 0.1f;      // radians, positive = looking down
};

#endif
