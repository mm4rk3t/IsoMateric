#include "scripting.h"
#include "audio.h"
#include "components.h"
#include "level.h"
#include "log.h"
#include "model.h"
#include "particles.h"

// Lua is compiled as C++ (see CMakeLists.txt) so luaL_error unwinds with an exception
// and destructors of C++ locals in these functions still run. That also means the
// headers are included without lua.hpp's extern "C" wrapper.
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#include <glm/gtc/constants.hpp>

#include <system_error>

namespace
{
	ScriptContext& ctxOf(lua_State* L)
	{
		return *static_cast<ScriptContext*>(lua_touserdata(L, lua_upvalueindex(1)));
	}

	entt::entity checkEntity(lua_State* L, int arg)
	{
		ScriptContext& ctx = ctxOf(L);
		auto e = static_cast<entt::entity>(static_cast<std::uint32_t>(luaL_checkinteger(L, arg)));
		if (!ctx.registry->valid(e))
			luaL_error(L, "invalid entity %d", static_cast<int>(lua_tointeger(L, arg)));
		return e;
	}

	void pushEntity(lua_State* L, entt::entity e)
	{
		lua_pushinteger(L, static_cast<lua_Integer>(entt::to_integral(e)));
	}

	glm::vec3 checkVec3(lua_State* L, int arg)
	{
		return glm::vec3(static_cast<float>(luaL_checknumber(L, arg)), static_cast<float>(luaL_checknumber(L, arg + 1)),
		                 static_cast<float>(luaL_checknumber(L, arg + 2)));
	}

	int apiLog(lua_State* L)
	{
		int n = lua_gettop(L);
		std::string out;
		for (int i = 1; i <= n; i++)
		{
			out += luaL_tolstring(L, i, nullptr);
			lua_pop(L, 1);
			if (i < n)
				out += ' ';
		}
		LOG_INFO("lua: %s", out.c_str());
		return 0;
	}

	int apiTime(lua_State* L)
	{
		lua_pushnumber(L, *ctxOf(L).time);
		return 1;
	}

	// iso.spawn_model(path, x, y, z [, height = 1, yaw_offset_degrees = 0]) -> entity
	int apiSpawnModel(lua_State* L)
	{
		ScriptContext& ctx = ctxOf(L);
		std::string path = luaL_checkstring(L, 1);
		glm::vec3 pos = checkVec3(L, 2);
		float height = static_cast<float>(luaL_optnumber(L, 5, 1.0));
		float yawOffset = glm::radians(static_cast<float>(luaL_optnumber(L, 6, 0.0)));

		auto model = ctx.models->get(ctx.assetsDir + "/" + path);
		if (!model)
			return luaL_error(L, "cannot load model '%s'", path.c_str());

		entt::registry& reg = *ctx.registry;
		entt::entity e = reg.create();
		reg.emplace<Transform>(e, Transform{pos, 0.0f});
		reg.emplace<MeshRenderer>(e, MeshRenderer{model, height, yawOffset, glm::vec4(1.0f)});
		Animator anim;
		anim.playing = !model->clips.empty();
		reg.emplace<Animator>(e, anim);
		reg.emplace<Name>(e, Name{path});
		reg.emplace<ScriptOwned>(e);
		pushEntity(L, e);
		return 1;
	}

	// iso.play_animation(entity, clip_index [, speed = 1])  speed 0 pauses
	int apiPlayAnimation(lua_State* L)
	{
		entt::entity e = checkEntity(L, 1);
		auto* anim = ctxOf(L).registry->try_get<Animator>(e);
		if (!anim)
			return luaL_error(L, "entity has no animator");
		int clip = static_cast<int>(luaL_checkinteger(L, 2));
		if (clip != anim->clip)
			anim->time = 0.0f;
		anim->clip = clip;
		anim->speed = static_cast<float>(luaL_optnumber(L, 3, 1.0));
		anim->playing = anim->speed != 0.0f;
		return 0;
	}

	int apiSetPosition(lua_State* L)
	{
		entt::entity e = checkEntity(L, 1);
		ctxOf(L).registry->get_or_emplace<Transform>(e).position = checkVec3(L, 2);
		return 0;
	}

	int apiGetPosition(lua_State* L)
	{
		entt::entity e = checkEntity(L, 1);
		const auto* t = ctxOf(L).registry->try_get<Transform>(e);
		glm::vec3 p = t ? t->position : glm::vec3(0.0f);
		lua_pushnumber(L, p.x);
		lua_pushnumber(L, p.y);
		lua_pushnumber(L, p.z);
		return 3;
	}

	int apiSetYaw(lua_State* L)
	{
		entt::entity e = checkEntity(L, 1);
		ctxOf(L).registry->get_or_emplace<Transform>(e).yaw = glm::radians(static_cast<float>(luaL_checknumber(L, 2)));
		return 0;
	}

	// iso.set_tint(entity, r, g, b)   multiplies a model's colours (1, 1, 1 = original)
	int apiSetTint(lua_State* L)
	{
		entt::entity e = checkEntity(L, 1);
		auto* mr = ctxOf(L).registry->try_get<MeshRenderer>(e);
		if (!mr)
			return luaL_error(L, "entity has no model");
		mr->tint = glm::vec4(checkVec3(L, 2), 1.0f);
		return 0;
	}

	entt::entity spawnEmitter(lua_State* L, bool oneShot)
	{
		ScriptContext& ctx = ctxOf(L);
		ParticleEmitter emitter = particlePreset(luaL_checkstring(L, 1));
		if (oneShot && emitter.burst == 0)
		{
			emitter.burst = static_cast<int>(emitter.rate * 0.3f) + 8;
			emitter.rate = 0.0f;
		}
		emitter.oneShot = emitter.oneShot || oneShot;
		entt::registry& reg = *ctx.registry;
		entt::entity e = reg.create();
		reg.emplace<Transform>(e, Transform{checkVec3(L, 2), 0.0f});
		reg.emplace<ParticleEmitter>(e, std::move(emitter));
		reg.emplace<ScriptOwned>(e);
		return e;
	}

	// iso.spawn_emitter(preset, x, y, z) -> entity     continuous ("fire", "smoke", "sparkle")
	int apiSpawnEmitter(lua_State* L)
	{
		pushEntity(L, spawnEmitter(L, false));
		return 1;
	}

	// iso.burst(preset, x, y, z)     one-shot, cleans itself up ("dust", ...)
	int apiBurst(lua_State* L)
	{
		spawnEmitter(L, true);
		return 0;
	}

	float optField(lua_State* L, int table, const char* key, float fallback)
	{
		lua_getfield(L, table, key);
		float v = lua_isnumber(L, -1) ? static_cast<float>(lua_tonumber(L, -1)) : fallback;
		lua_pop(L, 1);
		return v;
	}

	// iso.spawn_glass(x, y, z [, { radius, ior, roughness, absorption, dispersion,
	//                              tint = {r, g, b}, label }]) -> entity
	int apiSpawnGlass(lua_State* L)
	{
		ScriptContext& ctx = ctxOf(L);
		glm::vec3 pos = checkVec3(L, 1);
		Glass g;
		std::string label = "glass";
		if (lua_istable(L, 4))
		{
			g.radius = optField(L, 4, "radius", g.radius);
			g.ior = optField(L, 4, "ior", g.ior);
			g.roughness = optField(L, 4, "roughness", g.roughness);
			g.absorption = optField(L, 4, "absorption", g.absorption);
			g.dispersion = optField(L, 4, "dispersion", g.dispersion);
			if (lua_getfield(L, 4, "tint") == LUA_TTABLE)
			{
				for (int i = 0; i < 3; i++)
				{
					lua_rawgeti(L, -1, i + 1);
					if (lua_isnumber(L, -1))
						g.tint[i] = static_cast<float>(lua_tonumber(L, -1));
					lua_pop(L, 1);
				}
			}
			lua_pop(L, 1);
			if (lua_getfield(L, 4, "label") == LUA_TSTRING)
				label = lua_tostring(L, -1);
			lua_pop(L, 1);
		}
		entt::registry& reg = *ctx.registry;
		entt::entity e = reg.create();
		reg.emplace<Transform>(e, Transform{pos, 0.0f});
		reg.emplace<Glass>(e, g);
		reg.emplace<Name>(e, Name{label});
		reg.emplace<ScriptOwned>(e);
		pushEntity(L, e);
		return 1;
	}

	// iso.spawn_box(x, y, z, sx, sy, sz, r, g, b [, glow]) -> entity
	// a decorative box centred at x, y, z; walk-through, casts shadows unless glowing
	int apiSpawnBox(lua_State* L)
	{
		ScriptContext& ctx = ctxOf(L);
		Box box;
		glm::vec3 pos = checkVec3(L, 1);
		box.size = checkVec3(L, 4);
		box.color = glm::vec4(checkVec3(L, 7), 1.0f);
		box.glow = lua_toboolean(L, 10) != 0;
		entt::registry& reg = *ctx.registry;
		entt::entity e = reg.create();
		reg.emplace<Transform>(e, Transform{pos, 0.0f});
		reg.emplace<Box>(e, box);
		reg.emplace<ScriptOwned>(e);
		pushEntity(L, e);
		return 1;
	}

	int apiDestroy(lua_State* L)
	{
		ctxOf(L).registry->destroy(checkEntity(L, 1));
		return 0;
	}

	int apiPlayer(lua_State* L)
	{
		pushEntity(L, ctxOf(L).player);
		return 1;
	}

	// iso.play_sound(name [, volume = 1])   plays assets/sounds/<name>.wav
	int apiPlaySound(lua_State* L)
	{
		ctxOf(L).audio->play(luaL_checkstring(L, 1), static_cast<float>(luaL_optnumber(L, 2, 1.0)));
		return 0;
	}

	int apiHeight(lua_State* L)
	{
		lua_pushinteger(L, ctxOf(L).level->height(static_cast<int>(luaL_checkinteger(L, 1)), static_cast<int>(luaL_checkinteger(L, 2))));
		return 1;
	}

	int apiLightCount(lua_State* L)
	{
		lua_pushinteger(L, static_cast<lua_Integer>(ctxOf(L).level->lights.size()));
		return 1;
	}

	PointLight& checkLight(lua_State* L, int arg)
	{
		auto& lights = ctxOf(L).level->lights;
		lua_Integer i = luaL_checkinteger(L, arg);
		if (i < 1 || i > static_cast<lua_Integer>(lights.size()))
			luaL_error(L, "light index %d out of range (1..%d)", static_cast<int>(i), static_cast<int>(lights.size()));
		return lights[static_cast<size_t>(i - 1)];
	}

	// iso.get_light(i) -> x, y, z, intensity      (1-based, like Lua arrays)
	int apiGetLight(lua_State* L)
	{
		const PointLight& l = checkLight(L, 1);
		lua_pushnumber(L, l.position.x);
		lua_pushnumber(L, l.position.y);
		lua_pushnumber(L, l.position.z);
		lua_pushnumber(L, l.intensity);
		return 4;
	}

	int apiSetLightIntensity(lua_State* L)
	{
		checkLight(L, 1).intensity = static_cast<float>(luaL_checknumber(L, 2));
		return 0;
	}
}

ScriptHost::ScriptHost(ScriptContext context) : ctx(std::move(context)) { }

ScriptHost::~ScriptHost()
{
	if (L)
		lua_close(L);
}

void ScriptHost::reset()
{
	if (L)
		lua_close(L);
	L = nullptr;
	auto owned = ctx.registry->view<ScriptOwned>();
	ctx.registry->destroy(owned.begin(), owned.end());
	broken = false;
	lastError.clear();
}

bool ScriptHost::load(const std::string& path)
{
	reset();
	file = path;
	std::error_code ec;
	mtime = std::filesystem::last_write_time(path, ec);

	L = luaL_newstate();
	luaL_openlibs(L);

	static const luaL_Reg api[] = {
		{"log", apiLog},
		{"time", apiTime},
		{"spawn_model", apiSpawnModel},
		{"play_animation", apiPlayAnimation},
		{"set_position", apiSetPosition},
		{"get_position", apiGetPosition},
		{"set_yaw", apiSetYaw},
		{"set_tint", apiSetTint},
		{"spawn_emitter", apiSpawnEmitter},
		{"burst", apiBurst},
		{"spawn_glass", apiSpawnGlass},
		{"spawn_box", apiSpawnBox},
		{"destroy", apiDestroy},
		{"player", apiPlayer},
		{"play_sound", apiPlaySound},
		{"height", apiHeight},
		{"light_count", apiLightCount},
		{"get_light", apiGetLight},
		{"set_light_intensity", apiSetLightIntensity},
		{nullptr, nullptr},
	};
	lua_newtable(L);
	lua_pushlightuserdata(L, &ctx);
	luaL_setfuncs(L, api, 1); // every function gets the context as upvalue 1
	lua_setglobal(L, "iso");

	if (luaL_loadfile(L, path.c_str()) != LUA_OK)
	{
		lastError = lua_isstring(L, -1) ? lua_tostring(L, -1) : "cannot load script";
		LOG_ERROR("lua: %s", lastError.c_str());
		broken = true;
		return false;
	}
	if (!pcall(0)) // already logged
		return false;
	LOG_INFO("lua: loaded '%s'", path.c_str());
	if (pushCallback("on_start"))
		pcall(0);
	return !broken;
}

void ScriptHost::reloadIfChanged()
{
	if (file.empty())
		return;
	std::error_code ec;
	auto t = std::filesystem::last_write_time(file, ec);
	if (!ec && t != mtime)
		load(file);
}

bool ScriptHost::pushCallback(const char* name)
{
	if (!L || broken)
		return false;
	if (lua_getglobal(L, name) == LUA_TFUNCTION)
		return true;
	lua_pop(L, 1);
	return false;
}

bool ScriptHost::pcall(int nargs)
{
	if (lua_pcall(L, nargs, 0, 0) == LUA_OK)
		return true;
	lastError = lua_tostring(L, -1) ? lua_tostring(L, -1) : "unknown error";
	lua_pop(L, 1);
	LOG_ERROR("lua: %s (script paused until the file changes)", lastError.c_str());
	broken = true;
	return false;
}

void ScriptHost::update(float dt)
{
	if (pushCallback("on_update"))
	{
		lua_pushnumber(L, dt);
		pcall(1);
	}
}

void ScriptHost::event(const char* name, glm::vec3 p)
{
	if (pushCallback("on_event"))
	{
		lua_pushstring(L, name);
		lua_pushnumber(L, p.x);
		lua_pushnumber(L, p.y);
		lua_pushnumber(L, p.z);
		pcall(4);
	}
}
