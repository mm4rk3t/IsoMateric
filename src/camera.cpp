#include "camera.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

namespace
{
	const float kBaseYaw = glm::radians(135.0f);
}

Camera::Camera() : yaw(kBaseYaw), yawGoal(kBaseYaw) { }

void Camera::setViewport(int width, int height)
{
	viewportW = width > 0 ? width : 1;
	viewportH = height > 0 ? height : 1;
}

void Camera::rotate(int steps)
{
	yawGoal += glm::radians(90.0f) * static_cast<float>(steps);
}

void Camera::update(float dt)
{
	// frame-rate independent exponential easing
	float kYaw = 1.0f - std::exp(-dt * 12.0f);
	float kFocus = 1.0f - std::exp(-dt * 8.0f);
	yaw += (yawGoal - yaw) * kYaw;
	focus += (focusGoal - focus) * kFocus;
}

void Camera::snap()
{
	yaw = yawGoal;
	focus = focusGoal;
}

float Camera::elevation() const
{
	return style == Style::Isometric ? std::atan(1.0f / std::sqrt(2.0f)) : glm::radians(30.0f);
}

glm::vec3 Camera::toCamera() const
{
	float el = elevation();
	return glm::vec3(std::cos(el) * std::cos(yaw), std::sin(el), std::cos(el) * std::sin(yaw));
}

glm::vec3 Camera::position() const
{
	return focus + toCamera() * distance;
}

glm::vec3 Camera::forward() const
{
	glm::vec3 d = toCamera();
	return glm::normalize(glm::vec3(-d.x, 0.0f, -d.z));
}

glm::vec3 Camera::right() const
{
	return glm::cross(forward(), glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 Camera::view() const
{
	glm::vec3 target = focus;
	if (snapUnits > 0.0f)
	{
		// round the focus to whole pixel blocks along the screen's right/up axes
		glm::vec3 back = toCamera();
		glm::vec3 right = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), back));
		glm::vec3 up = glm::cross(back, right);
		float r = std::round(glm::dot(target, right) / snapUnits) * snapUnits;
		float u = std::round(glm::dot(target, up) / snapUnits) * snapUnits;
		float b = glm::dot(target, back);
		target = right * r + up * u + back * b;
	}
	return glm::lookAt(target + toCamera() * distance, target, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 Camera::projection() const
{
	float halfW = static_cast<float>(viewportW) / zoom * 0.5f;
	float halfH = static_cast<float>(viewportH) / zoom * 0.5f;
	return glm::ortho(-halfW, halfW, -halfH, halfH, 0.1f, distance * 3.0f);
}

Ray Camera::screenRay(float nx, float ny) const
{
	glm::vec2 ndc(nx * 2.0f - 1.0f, 1.0f - ny * 2.0f);
	glm::mat4 inv = glm::inverse(viewProjection());
	glm::vec4 nearP = inv * glm::vec4(ndc, -1.0f, 1.0f);
	glm::vec4 farP = inv * glm::vec4(ndc, 1.0f, 1.0f);
	glm::vec3 a = glm::vec3(nearP) / nearP.w;
	glm::vec3 b = glm::vec3(farP) / farP.w;
	return Ray{a, glm::normalize(b - a)};
}
