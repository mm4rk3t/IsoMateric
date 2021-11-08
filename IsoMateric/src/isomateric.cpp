#include "isomateric.h"

#include "gameObject.h"
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

// renderers
Renderer* renderer;
Renderer* lightRenderer;

// shaders
Shader shader;
Shader lightShader;

// camera
Camera* camera;

// player
Player* player = new Player();

// variables
float glfwTime = 0.0f;
glm::vec3 playerPos(0.0f, 0.0f, 0.0f);

// state bools
bool showDebug = true;
bool lockCamera = false;

// terrain
const unsigned int terrainSizeX = 16;
const unsigned int terrainSizeZ = 16;

int terrain[terrainSizeX][terrainSizeZ] = {
	5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 5,  
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  
	1, 1, 1, 1, 1, 1, 4, 3, 4, 3, 4, 1, 1, 1, 1, 1,  
	1, 1, 1, 1, 1, 1, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1,  
	1, 1, 1, 1, 1, 1, 4, 1, 1, 1, 1, 1, 1, 1, 1, 1,  
	1, 1, 1, 1, 1, 1, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1,  
	1, 1, 1, 1, 1, 1, 4, 1, 1, 1, 1, 1, 1, 1, 1, 1,  
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  
	5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 

};

// game loop
void IsoMateric::init()
{
	// resources - relative to the path of the binary
	ResourceManager::loadShader("/home/mm4rk3t/IsoMateric/IsoMateric/assets/shaders/flat_color.vs", "/home/mm4rk3t/IsoMateric/IsoMateric/assets/shaders/flat_color.fs", nullptr, "flat_color");	
	ResourceManager::loadShader("/home/mm4rk3t/IsoMateric/IsoMateric/assets/shaders/light.vs", "/home/mm4rk3t/IsoMateric/IsoMateric/assets/shaders/light.fs", nullptr, "light");	
	
	// shaders
	shader = ResourceManager::getShader("flat_color");		
	lightShader = ResourceManager::getShader("light");
	
	// renderers
	renderer = new Renderer(shader);
	lightRenderer = new Renderer(lightShader);
	
	// camera
	glm::vec3 viewPoint = glm::vec3(float(terrainSizeX) / 2.0f, 0.0f, float(terrainSizeZ) / 2.0f);
	camera = new Camera(viewPoint, this->width, this->height, 45.0f);

	// lighting
	Light* light0 = new Light();
	
	light0->position = glm::vec3(20.0f);
	light0->size = glm::vec3(0.5f);
	light0->renderer = lightRenderer;
	light0->intensity = 0.25f;
	
	this->lights.push_back(light0);

	// player
	player->renderer = renderer;
	player->color = glm::vec3(1.0f, 0.0f, 0.0f);
	player->position = glm::vec3(8.0f, 1.0f, 8.0f);
	player->size = glm::vec3(0.5, 1.0f, 0.5f);

	// game objects
	for (unsigned int x = 0; x < terrainSizeX; x++)
	{
		for (unsigned int z = 0; z < terrainSizeZ; z++)
		{	
			GameObject* floor_tile = new GameObject(glm::vec3((float)(x), 0.0f, (float)(z)), glm::vec3(1.0f), glm::vec3(0.0f));
			floor_tile->renderer = renderer;
			this->floor.push_back(floor_tile);
			
			if (terrain[x][z] > 1)
			{
				for(int i = 1; i <= terrain[x][z]; i++)
				{
					GameObject* go = new GameObject(glm::vec3((float)(x), i, (float)(z)), glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(1.0f));
					go->renderer = renderer;
					this->obstacles.push_back(go);
				}

			}

		}
	}
}

void IsoMateric::update(float dt)
{
	// time
	glfwTime = (float)glfwGetTime();

	// camera
	if(lockCamera)
		camera->viewpoint = player->position;
	
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
	for(GameObject* go : this->floor)
		go->draw();

	for(GameObject* go : this->obstacles)
		go->draw();
	
	for(Light* light : this->lights)
		light->draw();
		
	player->draw();
}

void IsoMateric::ui()
{
	if(showDebug)
	{
		// starting frame
                ImGui_ImplOpenGL3_NewFrame();
                ImGui_ImplGlfw_NewFrame();
                ImGui::NewFrame();
                
		// camera
		ImGui::Checkbox("lock camera (t)", &lockCamera);

		ImGui::Text("* rotate right (e) and left (q)\n* go up (lshift) and down (lctrl)");

		ImGui::Text("camera->position");
		ImGui::Text("\tx: %f", camera->position.x);
		ImGui::Text("\ty: %f", camera->position.y);
		ImGui::Text("\tz: %f", camera->position.z);
		
		ImGui::Text("camera->viewpoint");
		ImGui::Text("\tx: %f", camera->viewpoint.x);
		ImGui::Text("\ty: %f", camera->viewpoint.y);
		ImGui::Text("\tz: %f", camera->viewpoint.z);
	

		ImGui::Text("zoom: %f", camera->zoom);
		ImGui::Text("positionIndex: %i", camera->positionIndex);

		// lights
		if(ImGui::CollapsingHeader("lights"))
		{

			for (int unsigned i = 0; i < this->lights.size(); i++)
			{
				
				std::string index = std::to_string(i);
				
				ImGui::Text(("light" + index).c_str());
					
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
					
			}	

		if(ImGui::Button("add"))
			this->lights.push_back(new Light());
		}
		
		
		ImGui::Text("player->position");
		ImGui::Text("\tx: %f", player->position.x);
		ImGui::Text("\ty: %f", player->position.y);
		ImGui::Text("\tz: %f", player->position.z);
		
		// ending frame
                ImGui::Render();
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}
}


void IsoMateric::handleInput(float dt)
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
	
	
	if(this->keys[GLFW_KEY_W])
	{
		player->position += camera->getForward() * player->speed * dt;
	}

	if(this->keys[GLFW_KEY_S])
	{
		player->position -= camera->getForward() * player->speed * dt;
	}

	if(this->keys[GLFW_KEY_D])
	{
		player->position += glm::cross(camera->getForward(), glm::vec3(0.0f, 1.0f, 0.0f)) * player->speed * dt;
	}

	if(this->keys[GLFW_KEY_A])
	{
		player->position -= glm::cross(camera->getForward(), glm::vec3(0.0f, 1.0f, 0.0f)) * player->speed * dt;
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

