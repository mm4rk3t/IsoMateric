#ifndef SCRIPTING_H
#define SCRIPTING_H

#include <entt.hpp>
#include <glm/glm.hpp>

#include <filesystem>
#include <functional>
#include <string>

struct lua_State;
class Audio;
class Level;
class ModelCache;

// What scripts are allowed to touch.
struct ScriptContext
{
	entt::registry* registry = nullptr;
	Level* level = nullptr;
	Audio* audio = nullptr;
	ModelCache* models = nullptr;
	entt::entity player = entt::null;
	class Inventory* inventory = nullptr;
	std::function<void(glm::vec3 dir, float pitch)> lockCamera; // first person, looking along dir
	std::function<void()> unlockCamera;
	const float* time = nullptr;
	std::string assetsDir;
};

// Runs one Lua 5.4 game script. The script defines optional callbacks:
//   on_start()                 after (re)loading
//   on_update(dt)              every frame
//   on_event(name, x, y, z)    "jump", "land", "respawn", "blood_moon_start", "blood_moon_end"
// and calls the engine through the global table `iso` (see README).
// Saving the file reloads it: entities the script spawned are destroyed first.
class ScriptHost
{
public:
	explicit ScriptHost(ScriptContext context);
	~ScriptHost();
	ScriptHost(const ScriptHost&) = delete;
	ScriptHost& operator=(const ScriptHost&) = delete;

	bool load(const std::string& path);
	void reloadIfChanged();
	void update(float dt);
	void event(const char* name, glm::vec3 position);

	bool failed() const { return broken; }
	const std::string& error() const { return lastError; }
	const std::string& path() const { return file; }
	ScriptContext& context() { return ctx; }

private:
	void reset();
	bool pcall(int nargs);
	bool pushCallback(const char* name);

	lua_State* L = nullptr;
	ScriptContext ctx;
	std::string file;
	std::filesystem::file_time_type mtime{};
	std::string lastError;
	bool broken = false;
};

#endif
