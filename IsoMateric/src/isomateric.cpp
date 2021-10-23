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
Renderer* lightRenderer;

Shader shader;
Shader lightShader;

Camera* camera;

float glfwTime = 0.0f;
glm::vec3 playerPos(0.0f, 0.0f, 0.0f);

// state bools
bool showDebug = true;
bool lockCamera = false;

// game loop
void IsoMateric::init()
{
	// resources
	ResourceManager::loadShader("/home/mm4rk3t/IsoMateric/IsoMateric/assets/shaders/flat_color.vs", "/home/mm4rk3t/IsoMateric/IsoMateric/assets/shaders/flat_color.fs", nullptr, "flat_color");	
	ResourceManager::loadShader("/home/mm4rk3t/IsoMateric/IsoMateric/assets/shaders/light.vs", "/home/mm4rk3t/IsoMateric/IsoMateric/assets/shaders/light.fs", nullptr, "light");	
	
	// shaders
	shader = ResourceManager::getShader("flat_color");		
	lightShader = ResourceManager::getShader("light");
	
	// renderers
	renderer = new Renderer(shader);
	lightRenderer = new Renderer(lightShader);
	
	// camera
	glm::vec3 viewPoint = glm::vec3(0.0f, 0.0f, 0.0f);
	camera = new Camera(viewPoint, this->width, this->height, 45.0f);

	// lighting
	
	Light* light0 = new Light(glm::vec3(-3.75f, -0.25f, -3.75f), glm::vec3(1.0f, 0.0f, 0.0f), 1.0f);
	Light* light1 = new Light(glm::vec3(-3.75f, -0.25f,  3.75f), glm::vec3(0.0f, 1.0f, 0.0f), 1.0f);
	Light* light2 = new Light(glm::vec3( 3.75f, -0.25f, -3.75f), glm::vec3(0.0f, 0.0f, 1.0f), 1.0f);
	Light* light3 = new Light(glm::vec3( 3.75f, -0.25f,  3.75f), glm::vec3(1.0f, 1.0f, 1.0f), 1.0f);
	
	this->lights.push_back(light0);
	this->lights.push_back(light1);
	this->lights.push_back(light2);
	this->lights.push_back(light3);
	
}

void IsoMateric::handleInput()
{

	if(this->keys[47]) // minus
	{
		if(camera->zoom <= 10)
			camera->zoom = 10;
		else
			camera->zoom--;
	}

	if(this->keys[93]) // plus
	{
		if(camera->zoom >= 90)
			camera->zoom = 90;
		else
			camera->zoom++;
	}

	if(this->keys[341]) // left ctrl
	{
		camera->viewpoint.y--;
	}

	if(this->keys[340]) // left shift
	{
		camera->viewpoint.y++;
	}

	if(this->keys[GLFW_KEY_T] && !this->keysProcessed[GLFW_KEY_T])
	{
		lockCamera = !lockCamera;
		this->keysProcessed[GLFW_KEY_T] = true;
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

	if(this->keys[GLFW_KEY_F2] && !this->keysProcessed[GLFW_KEY_F2])
	{
		showDebug =! showDebug;
		this->keysProcessed[GLFW_KEY_F2] = true;
	}

}


void IsoMateric::update(float dt)
{
	// time
	glfwTime = (float)glfwGetTime();

	// camera
	if(lockCamera)
		camera->viewpoint = playerPos;
	glm::mat4 view = camera->getView();
	glm::mat4 projection = camera->getProjection();
	
	// shader uniforms
	shader.use();
	shader.setMat4("view", view);
	shader.setMat4("projection", projection);
	shader.setVec3f("viewPos", camera->position);
	
	int nrLights = this->lights.size();
	shader.setInt("nrLights", nrLights);

	for (unsigned int i = 0; i < this->lights.size(); i++)
	{
		std::string index = std::to_string(i);
		shader.setVec3f(("lights[" + index + "].position").c_str(),  this->lights[i]->position);	
		shader.setVec3f(("lights[" + index + "].color").c_str(),     this->lights[i]->color);
		shader.setFloat(("lights[" + index + "].intensity").c_str(), this->lights[i]->intensity);
	}


	lightShader.use();
	lightShader.setMat4("view", view);
	lightShader.setMat4("projection", projection);
}

void IsoMateric::render()
{

	glm::vec3 floor = glm::vec3(0.0f, -1.0f, 0.0f);
	renderer->draw(floor, glm::vec3(20.0f, 1.0f, 20.0f), 0.0f, glm::vec3(0.0f, 1.0f, 0.0f));
	renderer->draw(playerPos, glm::vec3(1.0f, 1.0f, 1.0f), 0.0f, glm::vec3(1.0f, 1.0f, 0.0f));

	for(unsigned int i = 0; i < this->lights.size(); i++)
	{
		lightRenderer->draw(this->lights[i]->position, glm::vec3(0.5f, 0.5f, 0.5), 0.0f, this->lights[i]->color);
	}

}

void IsoMateric::ui()
{
	if(showDebug)
	{
		// starting frame
                ImGui_ImplOpenGL3_NewFrame();
                ImGui_ImplGlfw_NewFrame();
                ImGui::NewFrame();
                
		ImGui::Checkbox("lock camera (t)", &lockCamera);
	
		ImGui::Dummy(ImVec2(0.0f, 0.0f));

		if(ImGui::CollapsingHeader("lights"))
		{

			for (int unsigned i = 0; i < this->lights.size(); i++)
			{

				std::string index = std::to_string(i);
				
				if(ImGui::CollapsingHeader(("light" + index).c_str()))
				{
					ImGui::Dummy(ImVec2(0.0f, 0.0f));
					std::string position_tag = "light_pos" + index;
					ImGui::SliderFloat3(position_tag.c_str(), &this->lights[i]->position.x, -20.0f, 20.0f);
				
					std::string color_tag = "light_col" + index;
					ImGui::SliderFloat3(color_tag.c_str(), &this->lights[i]->color.x, 0.0f, 1.0f);
	
					std::string intensity_tag = "light_int" + index;
					ImGui::SliderFloat(intensity_tag.c_str(), &this->lights[i]->intensity, 0.0f, 1.0f);
				
					std::string button_tag = "del_light" + index;
			
					if(ImGui::Button(button_tag.c_str()))
					{
						std::vector<Light*>::iterator it;
						it = this->lights.begin();
						this->lights.erase(it + i);
					}

				ImGui::Dummy(ImVec2(0.0f, 0.0f));
			
				}
			}	

		ImGui::Dummy(ImVec2(0.0f, 0.0f));
		
		if(ImGui::Button("add"))
			this->lights.push_back(new Light());
		}
		// ending frame
                ImGui::Render();
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}
}
