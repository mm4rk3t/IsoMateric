#include "isomateric.h"

#include "resourceManager.h"
#include "renderer.h"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <iostream>
#include <vector>

// constructor and destructor
IsoMateric::IsoMateric(unsigned int width, unsigned int height)
	: width(width), height(height), state(ACTIVE), keys(){ }
IsoMateric::~IsoMateric() { }

// renderer
Renderer* renderer;

// shader
Shader shader;

glm::vec3 color = glm::vec3(1.0f);

glm::mat4 model = glm::mat4(1.0f);
glm::mat4 view = glm::mat4(1.0f);
glm::mat4 projection = glm::mat4(1.0f);

// game loop
void IsoMateric::init()
{
	ResourceManager::loadShader("/home/mm4rk3t/IsoMateric/IsoMateric/assets/shaders/flat_color.vs", "/home/mm4rk3t/IsoMateric/IsoMateric/assets/shaders/flat_color.fs", nullptr, "flat_color");	

	shader = ResourceManager::getShader("flat_color");		
	
	float divider = 50;
	float orthoX = (float)((this->width / 2) / divider);
	float orthoY = (float)((this->height / 2) / divider);
	view = glm::lookAt(glm::vec3(2.0, 1.15f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	projection = glm::ortho(-orthoX, orthoX, -orthoY, orthoY, 0.1f, 100.0f);  

	renderer = new Renderer(shader);

	shader.use();
	shader.setVec3f("color", color);
	shader.setMat4("view", view);
	shader.setMat4("projection", projection);
	
}
void IsoMateric::handleInput(float dt) { }
void IsoMateric::update(float dt) { }  
void IsoMateric::render()
{
	std::vector<glm::vec3> positions = {
		/*
		glm::vec3(1.0f, 0.0f, 0.0f),
		glm::vec3(0.0f, 1.0f, 0.0f),
		glm::vec3(0.0f, 0.0f, 1.0f)
		*/
		
		glm::vec3(0.0f, -1.0f, 0.0f),
		glm::vec3(1.0f, -1.0f, 0.0f),
		glm::vec3(0.0f, -1.0f, 1.0f),
		glm::vec3(1.0f, -1.0f, 1.0f),

		glm::vec3(0.0f, 0.0f, 0.0f),
		glm::vec3(1.0f, 0.0f, 0.0f),
		glm::vec3(0.0f, 0.0f, 1.0f),
		glm::vec3(1.0f, 0.0f, 1.0f),

		glm::vec3(0.0f, 1.0f, 0.0f),
		glm::vec3(1.0f, 1.0f, 0.0f),
		glm::vec3(0.0f, 1.0f, 1.0f),
		glm::vec3(1.0f, 1.0f, 1.0f)
		
	};
	
	for (unsigned int i = 0; i < positions.size(); i++)
	{
		renderer->draw(positions[i], glm::vec3(1.0f, 1.0f, 1.0f), 0.0f, positions[i]);
	}

}
