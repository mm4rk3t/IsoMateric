#ifndef ISOMATERIC_H
#define ISOMATERIC_H

#include "audio.h"
#include "camera.h"
#include "input.h"
#include "level.h"
#include "model.h"
#include "player.h"
#include "renderer.h"
#include "scripting.h"

#include <entt.hpp>
#include <glm/glm.hpp>

#include <memory>
#include <random>
#include <string>

struct GLFWwindow;

// Owns the game state and runs the main loop. One instance per window; GLFW
// callbacks reach it through glfwGetWindowUserPointer.
class IsoMateric
{
public:
	IsoMateric(GLFWwindow* window, std::string levelPath);
	~IsoMateric();

	bool init();
	void run();

	void onFramebufferResize(int width, int height);
	Input& input() { return inputState; }

private:
	enum class Mode { Play, Editor };

	struct Hover
	{
		bool valid = false;
		glm::ivec3 block{0};  // the block under the cursor
		glm::ivec3 normal{0}; // face of that block the cursor is on
	};

	// per-frame steps
	void handleFrameInput(float dt);
	void fixedUpdate(float dt);
	void updateHover();
	void syncPlayerEntity(float dt);
	void updateAnimation(float dt);
	void buildFrame(FrameData& frame);
	void drawUi(float frameTime);
	void updateLayout();

	void loadLevel(const std::string& path);
	void resetPlayer();
	void createPlayer();
	Player& player() { return registry.get<Player>(playerEntity); }
	const Player& player() const { return registry.get<Player>(playerEntity); }

	GLFWwindow* window;
	Input inputState;
	Camera camera;
	Level level;
	Renderer renderer;
	RenderSettings renderSettings;
	Audio audio;
	ModelCache models;
	entt::registry registry;
	entt::entity playerEntity = entt::null;
	std::unique_ptr<ScriptHost> scripts;
	std::mt19937 rng{1234};

	Mode mode = Mode::Play;
	Hover hover;
	std::string levelPath;
	char levelPathBuffer[512] = {};
	int newLevelSize[2] = {16, 16};

	bool jumpRequested = false;
	Player::Events pendingEvents;
	bool showDebug = true;
	bool followPlayer = false;
	bool autoReload = true;
	bool pixelSnap = false;
	float reloadTimer = 0.0f;
	float fpsSmoothed = 0.0f;
	float elapsed = 0.0f;
	int windowW = 1, windowH = 1;
	float sidebarWidth = 340.0f; // debug sidebar, window coordinates; resizable
	float viewX = 0.0f;          // where the scene starts, window coordinates
};

#endif
