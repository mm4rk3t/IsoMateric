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

struct Player
{
	glm::vec3 position = glm::vec3(0.0f, 1.0f, 0.0f);
	glm::vec3 size = glm::vec3(1.0f, 1.0f, 1.0f);
	glm::vec3 color = glm::vec3(1.0f, 0.0f, 0.0f);
	float speed = 2.5;
} player;

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

// terrain
const int MAX_CHUNK_X = 16;
const int MAX_CHUNK_Z = 16;

float centerX = MAX_CHUNK_X / 2;
float centerZ = MAX_CHUNK_Z / 2;

int terrain[MAX_CHUNK_X][MAX_CHUNK_Z] = {
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  
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
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1  

};

std::vector<glm::vec3> collidePositions;

bool AABB(Player player, glm::vec3 object)
{
	bool collisionX = player.position.x + player.size.x >= object.x && object.x + 1.0f >= player.position.x;
	bool collisionZ = player.position.z + player.size.z >= object.z && object.z + 1.0f >= player.position.z;
	return collisionX && collisionZ;
}

glm::vec2 AABBdirection(glm::vec2 target)
{
	glm::vec2 compass[4] = {
		glm::vec2(0.0f, 1.0f),
		glm::vec2(1.0f, 0.0f),
		glm::vec2(0.0f, -1.0f),
		glm::vec2(-1.0f, 1.0f),
	};
	float max = 0.0f;
	unsigned int best_match = -1;
	for(unsigned int i = 0; i < 4; i++)
	{
		float dot_product = glm::dot(glm::normalize(target), compass[i]);
		if(dot_product > max)
		{
			max = dot_product;
			best_match = i;
		}
	}
	return compass[best_match];
}

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
	glm::vec3 viewPoint = glm::vec3(0.0f, 0.0f, 0.0f);
	camera = new Camera(viewPoint, this->width, this->height, 45.0f);

	// lighting
	Light* light0 = new Light(glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.3f, 0.4f, 0.5f), 1.0f);	
	this->lights.push_back(light0);

	// terrain
	for (int x = 0; x < MAX_CHUNK_X; x++)
	{
		for (int z = 0; z < MAX_CHUNK_Z; z++)
		{
			for (int y = 0; y < terrain[x][z]; y++)	
			{
				if(terrain[x][z] > 2)
					collidePositions.push_back(glm::vec3(float(x) - centerX, y, float(z) - centerZ));
			}
		}
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
		player.position += camera->getForward() * player.speed * dt;
	}

	if(this->keys[GLFW_KEY_S])
	{
		player.position -= camera->getForward() * player.speed * dt;
	}

	if(this->keys[GLFW_KEY_D])
	{
		player.position += glm::cross(camera->getForward(), glm::vec3(0.0f, 1.0f, 0.0f)) * player.speed * dt;
	}

	if(this->keys[GLFW_KEY_A])
	{
		player.position -= glm::cross(camera->getForward(), glm::vec3(0.0f, 1.0f, 0.0f)) * player.speed * dt;
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
		camera->viewpoint = player.position;
	glm::mat4 view = camera->getView();
	glm::mat4 projection = camera->getProjection();
	
	// collisions
	for (unsigned int i = 0; i < collidePositions.size(); i++)
	{
		if(AABB(player, collidePositions[i]))
		{
			std::cout << "collision!" << std::endl;

			glm::vec2 target = (player.position - collidePositions[i]);
			glm::vec2 direction = -AABBdirection(target);
		
			
			player.position.x += dt * player.speed * direction.y;
			player.position.z += dt * player.speed * direction.x;

		}	
	}

	// borders
	if(player.position.z > MAX_CHUNK_Z / 2 - 0.5 - player.size.z / 2)
		player.position.z = MAX_CHUNK_Z / 2 - 0.5 - player.size.z / 2;
	if(player.position.z < -MAX_CHUNK_Z / 2)
		player.position.z = -MAX_CHUNK_Z / 2;

	if(player.position.x > MAX_CHUNK_X / 2 - 0.5 - player.size.x / 2)
		player.position.x = MAX_CHUNK_X / 2 - 0.5 - player.size.x / 2;
	if(player.position.x < -MAX_CHUNK_X / 2)
		player.position.x = -MAX_CHUNK_X / 2;

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
	renderer->draw(player.position, player.size, 0.0f, player.color);

	for(unsigned int i = 0; i < this->lights.size(); i++)
	{
		lightRenderer->draw(this->lights[i]->position, glm::vec3(0.5f, 0.5f, 0.5), 0.0f, this->lights[i]->color);
	}
	
	// terrain
	for (int x = 0; x < MAX_CHUNK_X; x++)
	{
		for (int z = 0; z < MAX_CHUNK_Z; z++)
		{
			for (int y = 0; y < terrain[x][z]; y++)	
			{
				glm::vec3 tileColor;
				if(y > 0)

					tileColor = glm::vec3(0.0f, 0.0f, 0.0f);
				else
					tileColor = glm::vec3(1.0f, 1.0f, 1.0f);
				
				renderer->draw(glm::vec3(float(x) - centerX, y, float(z) - centerZ), glm::vec3(1.0f, 1.0f, 1.0f), 0.0f, tileColor);
			}
		}
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
		ImGui::Text("\tx: %f", player.position.x);
		ImGui::Text("\ty: %f", player.position.y);
		ImGui::Text("\tz: %f", player.position.z);

		ImGui::Text("collidePositions[0]");
		ImGui::Text("\tx: %f", collidePositions[0].x);
		ImGui::Text("\ty: %f", collidePositions[0].y);
		ImGui::Text("\tz: %f", collidePositions[0].z);

		// ending frame
                ImGui::Render();
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}
}
