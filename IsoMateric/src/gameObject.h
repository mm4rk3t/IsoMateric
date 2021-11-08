#ifndef GAMEOBJECT_H
#define GAMEOBJECT_H

#include "renderer.h"
#include <glm/glm.hpp>

class GameObject {
public:
	inline GameObject(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3 size = glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f)) 
		: position(position), size(size), color(color){ }
	~GameObject();

	glm::vec3 position;
	glm::vec3 size;
	glm::vec3 color; 
	Renderer* renderer;
	void draw();
};

#endif
