#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera
{
public:
	Camera(glm::vec3 viewpoint, float width, float height, float zoom);
	
	glm::mat4 getView();
	glm::mat4 getProjection();
	
	float zoom, width, height;
	glm::vec3 position, viewpoint;

	unsigned int positionIndex = 0;
	glm::vec3 positionOffsets[4]
	{
		glm::vec3(-2.0f, 1.15f,  2.0f),
		glm::vec3(-2.0f, 1.15f, -2.0f),
		glm::vec3( 2.0f, 1.15f, -2.0f),
		glm::vec3( 2.0f, 1.15f,  2.0f),
	};
	glm::vec3 getForward();
};

#endif
