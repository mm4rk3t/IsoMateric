#include "camera.h"

Camera::Camera(glm::vec3 viewpoint, float width, float height, float zoom)
{
	this->viewpoint = viewpoint;
	this->width = width;
	this->height = height;
	this->zoom = zoom;
}

glm::mat4 Camera::getView()
{

	glm::mat4 view = glm::lookAt(this->viewpoint + this->positionOffsets[this->positionIndex], this->viewpoint, glm::vec3(0.0f, 1.0f, 0.0f));
	return view;
}

glm::mat4 Camera::getProjection()
{

       	float orthoX = (float)((this->width / 2) / this->zoom);
       	float orthoY = (float)((this->height / 2) / this->zoom);
	glm::mat4 projection = glm::ortho(-orthoX, orthoX, -orthoY, orthoY, 0.1f, 100.0f);
	return projection;
}
