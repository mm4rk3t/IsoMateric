#ifndef ISOMATERIC_H
#define ISOMATERIC_H

#include "audio.h"
#include "camera.h"
#include "input.h"
#include "inventory.h"
#include "level.h"
#include "model.h"
#include "player.h"
#include "renderer.h"
#include "scripting.h"
#include "weather.h"

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
	bool onEscape(); // true if Esc closed a menu instead of quitting

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
	void drawLabels();
	void drawHud();
	void updateFlashlight();
	void applyQuality(int level);
	entt::entity nearestPickup() const;
	void setDayNight(bool enabled);
	void applyDayNight();
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
	bool mouseLooking = false; // first person: cursor captured while a look button is held
	void setMouseLook(bool on);
	Player::Events pendingEvents;
	bool showDebug = true;
	bool followPlayer = true; // on by default; panning with the mouse turns it off, T toggles
	bool autoReload = true;
	bool pixelSnap = false;
	bool showLabels = false;

	// day/night test cycle; overrides the level's sun while running and restores it after
	struct DayNight
	{
		bool running = false;
		float hour = 8.0f;   // 0..24
		float speed = 1.0f;  // in-game hours per real second
		glm::vec3 savedSunDirection{0.0f}, savedSunColor{0.0f}, savedClear{0.0f};
		float savedAmbient = 0.0f;

		// blood moon: rolled at dusk, lasts until dawn
		bool bloodMoon = false;
		float bloodMoonChance = 0.25f; // per night
		float bloodMoonFade = 0.0f;    // 0..1, eased so the sky turns red gradually
		entt::entity ash = entt::null; // falling ash emitter that follows the camera
	} dayNight;
	void advanceDayNight(float dt);
	void setBloodMoon(bool active);

	struct Weather
	{
		bool raining = false;
		float rainIntensity = 0.7f;
		glm::vec2 wind{1.5f, 0.6f};
		bool fogFollowsSky = true;
		bool lightning = false;      // strikes while it rains
		float lightningTimer = 4.0f; // seconds to the next strike
		float flashAge = -1.0f;      // seconds since the last strike, < 0 when idle
		float thunderDelay = 0.0f;   // the sound arrives this long after the flash
		bool thunderPlayed = true;
	} weather;
	void strikeLightning();
	Rain rain;
	void updateWeather(float dt);
	void updateSky();

	// decorative block asteroids orbiting the level (count comes from the level file)
	struct Asteroid
	{
		float orbitRadius, angle, speed, height, bobPhase;
		std::vector<std::pair<glm::vec3, float>> parts; // offset, cube size
		glm::vec3 color;
	};
	std::vector<Asteroid> asteroids;
	void generateAsteroids();
	float reloadTimer = 0.0f;
	float fpsSmoothed = 0.0f;
	float elapsed = 0.0f;

	// items: Silent Hill style inventory, pickups in the world, a flashlight
	Inventory inventory;
	struct ImFont* fontTitle = nullptr;
	struct ImFont* fontBody = nullptr;
	std::string hudMessage;
	float hudMessageTime = 0.0f;

	// performance: vsync off by default so the fps shows what the engine can do
	bool vsync = false;
	int quality = -1; // -1 custom (level defaults), 0 low .. 3 ultra, see applyQuality
	float frameMsSmoothed = 0.0f;
	int windowW = 1, windowH = 1;
	float sidebarWidth = 340.0f; // debug sidebar, window coordinates; resizable
	float viewX = 0.0f;          // where the scene starts, window coordinates
};

#endif
