// custom headers
#include "isomateric.h"

// system headers
#include <iostream>

// vendor headers
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <imgui.h>

#include <imgui_impl_opengl3.h>
#include <imgui_impl_glfw.h>

// variables
const unsigned int SCR_W = 800;
const unsigned int SCR_H = 600; 

IsoMateric game(SCR_W, SCR_H);

// callbacks
void frame_buffer_size_callback(GLFWwindow* window, int width, int height);
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);

int main()
{
	// window
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_RESIZABLE, true);
	glfwWindowHint(GLFW_SAMPLES, 8);
	
	GLFWwindow* window = glfwCreateWindow(SCR_W, SCR_H, "IsoMateric v0.0.1", nullptr, nullptr);
	if(window == nullptr)
		std::cout << "GLFW::Could not create window" << std::endl;
	glfwMakeContextCurrent(window);
	
	// glad
	if(!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "GLAD::Could not load GLAD" << std::endl;
		return -1;
	}

	// window callbacks
	glfwSetFramebufferSizeCallback(window, frame_buffer_size_callback);
	glfwSetKeyCallback(window, key_callback);

	// set viewport
	glViewport(0, 0, SCR_W, SCR_H);
	
	// imgui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 330");

	// init state
	game.init();

	// dt variables
	float deltaTime = 0.0f;
	float lastFrame = 0.0f;

	// main loop
	while(!glfwWindowShouldClose(window))
	{

		float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;
		
		game.handleInput(deltaTime);
		game.update(deltaTime);

		glfwPollEvents();
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_MULTISAMPLE);	
	
		glDisable(GL_CULL_FACE);
		
		game.render();
		game.ui();		
		
		glfwSwapBuffers(window);
	}

	ImGui_ImplOpenGL3_Shutdown();
    	ImGui_ImplGlfw_Shutdown();
    	ImGui::DestroyContext();

	glfwDestroyWindow(window);
    	glfwTerminate();

	return 0;
}

void frame_buffer_size_callback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
	
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	if(key >= 0 && key < 1024)
	{
		if (action == GLFW_PRESS)
			game.keys[key] = true;
		else if (action == GLFW_RELEASE)
		{
			game.keys[key] = false;
			game.keysProcessed[key] = false;
		}
	}
	std::cout << key << std::endl;
}
