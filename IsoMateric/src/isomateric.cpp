#include "isomateric.h"

#include "resourceManager.h"
#include "renderer.h"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_glfw.h>

#include <iostream>
#include <vector>

// constructor and destructor
IsoMateric::IsoMateric(unsigned int width, unsigned int height)
	: width(width), height(height), state(ACTIVE), keys(){ }
IsoMateric::~IsoMateric() { }


Renderer* renderer;
Shader shader;
Camera* camera;

float glfwTime = 0.0f;
glm::vec3 playerPos = {};

// game loop
void IsoMateric::init()
{
	// resources
	ResourceManager::loadShader("/home/mm4rk3t/IsoMateric/IsoMateric/assets/shaders/flat_color.vs", "/home/mm4rk3t/IsoMateric/IsoMateric/assets/shaders/flat_color.fs", nullptr, "flat_color");	

	// shaders
	shader = ResourceManager::getShader("flat_color");		
	
	// renderer
	renderer = new Renderer(shader);
	
	// camera
	glm::vec3 viewPoint = glm::vec3(0.0f, 0.0f, 0.0f);
	camera = new Camera(viewPoint, this->width, this->height, 45.0f);

}

void IsoMateric::handleInput()
{

	if(this->keys[47] && !this->keysProcessed[47])// minus
	{
		if(camera->zoom <= 10)
			camera->zoom = 10;
		else
			camera->zoom--;
		this->keysProcessed[47] = true;
	}
	if(this->keys[93] && !this->keysProcessed[93]) //plus
	{
		if(camera->zoom >= 90)
			camera->zoom = 90;
		else
			camera->zoom++;
		this->keysProcessed[93] = true;
	}

	if(this->keys[GLFW_KEY_Q] && !this->keysProcessed[GLFW_KEY_Q])
	{
		if(camera->positionIndex == 3)
			camera->positionIndex = 0;
		else
			camera->positionIndex++;
		this->keysProcessed[GLFW_KEY_Q] = true;
	}
		
	if(this->keys[GLFW_KEY_E] && !this->keysProcessed[GLFW_KEY_E])
	{
		if(camera->positionIndex == 0)
			camera->positionIndex = 3;
		else
			camera->positionIndex--;
		this->keysProcessed[GLFW_KEY_E] = true;
	}

}


void IsoMateric::update(float dt)
{
	// camera position
	camera->viewpoint = playerPos;

	// camera
	glm::mat4 view = camera->getView();
	glm::mat4 projection = camera->getProjection();
	
	// shader uniforms
	shader.use();
	shader.setMat4("view", view);
	shader.setMat4("projection", projection);

	// time
	glfwTime = (float)glfwGetTime();
}

void IsoMateric::render()
{

	glm::vec3 floor = glm::vec3(0.0f, -1.0f, 0.0f);
	renderer->draw(floor, glm::vec3(10.0f, 1.0f, 10.0f), 0.0f, glm::vec3(0.0f, 1.0f, 0.0f));
	renderer->draw(playerPos, glm::vec3(1.0f, 1.0f, 1.0f), 0.0f, glm::vec3(1.0f, 1.0f, 0.0f));
}

void IsoMateric::ui()
{
		// starting frame
                ImGui_ImplOpenGL3_NewFrame();
                ImGui_ImplGlfw_NewFrame();
                ImGui::NewFrame();
                
		// the juice
           	ImGui::SliderFloat3("playerPos", &playerPos.x, -5.0f, 5.0f);

		// ending frame
                ImGui::Render();
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
