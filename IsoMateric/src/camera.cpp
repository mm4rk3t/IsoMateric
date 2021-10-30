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
	this->position = this->viewpoint + this->positionOffsets[this->positionIndex];
	glm::mat4 view = glm::lookAt(this->position, this->viewpoint, glm::vec3(0.0f, 1.0f, 0.0f));
	return view;
}

glm::mat4 Camera::getProjection()
{

       	float orthoX = ((float)(this->width) / this->zoom) / 2.0f;
       	float orthoY = ((float)(this->height) / this->zoom) / 2.0f;
	glm::mat4 projection = glm::ortho(-orthoX, orthoX, -orthoY, orthoY, -100.0f, 100.0f);
	return projection;
}

glm::vec3 Camera::getForward()
{
	switch (this->positionIndex)
	{
		case 0:
			return glm::vec3(1.0f, 0.0f, -1.0f); 
	
		case 1:
			return glm::vec3(1.0f, 0.0f, 1.0f); 
		case 2:
			return glm::vec3(-1.0f, 0.0f, 1.0f); 
		case 3:
			return glm::vec3(-1.0f, 0.0f, -1.0f); 
	}
}
