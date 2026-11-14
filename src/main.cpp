#include "isomateric.h"
#include "log.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <string>

namespace
{
	IsoMateric* engineOf(GLFWwindow* window)
	{
		return static_cast<IsoMateric*>(glfwGetWindowUserPointer(window));
	}
}

int main(int argc, char** argv)
{
	glfwSetErrorCallback([](int code, const char* description) { LOG_ERROR("glfw %d: %s", code, description); });
	if (!glfwInit())
		return 1;

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE); // required on macOS
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	glfwWindowHint(GLFW_SAMPLES, 8);

	GLFWwindow* window = glfwCreateWindow(1280, 720, "IsoMateric v0.1.0", nullptr, nullptr);
	if (!window)
	{
		LOG_ERROR("could not create a window with an OpenGL 3.3 core context");
		glfwTerminate();
		return 1;
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
	{
		LOG_ERROR("could not load OpenGL functions");
		glfwDestroyWindow(window);
		glfwTerminate();
		return 1;
	}

	int exitCode = 0;
	{
		std::string levelPath = argc > 1 ? argv[1] : std::string(ISOMATERIC_ASSETS_DIR) + "/levels/default.level";
		IsoMateric engine(window, levelPath);
		glfwSetWindowUserPointer(window, &engine);

		// installed before ImGui so its backend chains to these callbacks
		glfwSetFramebufferSizeCallback(window, [](GLFWwindow* w, int width, int height) {
			engineOf(w)->onFramebufferResize(width, height);
		});
		glfwSetKeyCallback(window, [](GLFWwindow* w, int key, int, int action, int) {
			if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
				glfwSetWindowShouldClose(w, GLFW_TRUE);
			engineOf(w)->input().onKey(key, action);
		});
		glfwSetMouseButtonCallback(window, [](GLFWwindow* w, int button, int action, int) {
			engineOf(w)->input().onMouseButton(button, action);
		});
		glfwSetCursorPosCallback(window, [](GLFWwindow* w, double x, double y) {
			engineOf(w)->input().onCursor(x, y);
		});
		glfwSetScrollCallback(window, [](GLFWwindow* w, double, double dy) {
			engineOf(w)->input().onScroll(dy);
		});

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::GetIO().IniFilename = nullptr; // do not litter imgui.ini into the working directory
		ImGui::StyleColorsDark();
		ImGui_ImplGlfw_InitForOpenGL(window, true);
		ImGui_ImplOpenGL3_Init("#version 330");

		if (engine.init())
			engine.run();
		else
			exitCode = 1;

		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
		glfwSetWindowUserPointer(window, nullptr);
	} // engine (and its GL objects) destroyed while the context is still alive

	glfwDestroyWindow(window);
	glfwTerminate();
	return exitCode;
}
