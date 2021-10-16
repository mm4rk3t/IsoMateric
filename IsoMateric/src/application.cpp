// custom headers
#include "isomateric.h"

// system headers
#include <iostream>

// vendor headers
#include <glad/glad.h>
#include <GLFW/glfw3.h>

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
	glfwWindowHint(GLFW_RESIZABLE, false);
	
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

		game.update(deltaTime);
		glfwPollEvents();
		glClearColor(0.3f, 0.3f, 0.3f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		game.render();
		glfwSwapBuffers(window);
	}

	return 0;
}

void frame_buffer_size_callback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
	
	if (key == GLFW_KEY_Q && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	if(key >= 0 && key < 1024)
	{
		if (action == GLFW_PRESS)
			game.keys[key] = true;
		else if (action == GLFW_RELEASE)
			game.keys[key] = false;
	}
}
