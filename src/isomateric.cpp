#include "isomateric.h"
#include "components.h"
#include "log.h"
#include "particles.h"
#include "pathfinding.h"
#include "profiler.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <utility>

namespace
{
	const double kFixedStep = 1.0 / 120.0;
	const float kMinZoom = 8.0f, kMaxZoom = 256.0f;
	const std::string kAssets = ISOMATERIC_ASSETS_DIR;
	const char* kPlayerModel = "models/CesiumMan.glb";
	const float kPlayerModelYaw = 0.0f; // CesiumMan faces +z (glTF convention)

	float wrapAngle(float a)
	{
		while (a > glm::pi<float>()) a -= glm::two_pi<float>();
		while (a < -glm::pi<float>()) a += glm::two_pi<float>();
		return a;
	}

	// slab test; returns distance along the ray and the face normal that was hit
	bool rayBox(const Ray& ray, glm::vec3 bmin, glm::vec3 bmax, float& tHit, glm::ivec3& normal)
	{
		float tNear = -std::numeric_limits<float>::infinity();
		float tFar = std::numeric_limits<float>::infinity();
		int nearAxis = 0;
		for (int a = 0; a < 3; a++)
		{
			if (std::abs(ray.dir[a]) < 1e-8f)
			{
				if (ray.origin[a] < bmin[a] || ray.origin[a] > bmax[a])
					return false;
				continue;
			}
			float t1 = (bmin[a] - ray.origin[a]) / ray.dir[a];
			float t2 = (bmax[a] - ray.origin[a]) / ray.dir[a];
			if (t1 > t2)
				std::swap(t1, t2);
			if (t1 > tNear)
			{
				tNear = t1;
				nearAxis = a;
			}
			tFar = std::min(tFar, t2);
			if (tNear > tFar || tFar < 0.0f)
				return false;
		}
		tHit = tNear;
		normal = glm::ivec3(0);
		normal[nearAxis] = ray.dir[nearAxis] > 0.0f ? -1 : 1;
		return true;
	}

	float tileNoise(int x, int z)
	{
		unsigned h = static_cast<unsigned>(x) * 73856093u ^ static_cast<unsigned>(z) * 19349663u;
		h = (h ^ (h >> 13)) * 1274126177u;
		return static_cast<float>((h ^ (h >> 16)) & 0xff) / 255.0f;
	}

	// colour of the floor block (y == 0) or of the stack above it, from the column material
	glm::vec3 blockColor(const Level& level, int x, int y, int z)
	{
		const float n = tileNoise(x, z);
		const bool stack = y > 0;
		switch (level.tile(x, z))
		{
			case 's': return glm::vec3(0.63f, 0.61f, 0.58f) * (0.93f + 0.1f * n) * (stack ? 0.92f : 1.0f);
			case 'S': return glm::vec3(0.44f, 0.42f, 0.42f) * (0.93f + 0.1f * n);
			case 'g': return stack ? glm::vec3(0.27f, 0.45f, 0.25f) * (0.9f + 0.15f * n)
			                       : glm::vec3(0.34f, 0.54f, 0.29f) * (0.88f + 0.18f * n);
			case 'w': return glm::vec3(0.55f, 0.37f, 0.21f) * (((x + z) & 1) ? 0.92f : 1.0f) * (0.95f + 0.08f * n);
			case 'd': return glm::vec3(0.50f, 0.37f, 0.26f) * (0.9f + 0.15f * n);
			case 'r': return glm::vec3(0.40f, 0.38f, 0.36f) * (0.85f + 0.2f * n);
			case 'b': return glm::vec3(0.82f, 0.72f, 0.58f) * (0.96f + 0.06f * n);
			default: break;
		}
		if (y == 0)
			return ((x + z) & 1) ? glm::vec3(0.47f, 0.52f, 0.42f) : glm::vec3(0.51f, 0.57f, 0.45f);
		float s = 0.70f + 0.05f * static_cast<float>(y);
		return glm::vec3(s, s * 0.94f, s * 0.84f);
	}
}

IsoMateric::IsoMateric(GLFWwindow* window, std::string path)
	: window(window), levelPath(std::move(path))
{
	std::snprintf(levelPathBuffer, sizeof(levelPathBuffer), "%s", levelPath.c_str());
}

IsoMateric::~IsoMateric()
{
	scripts.reset(); // before the registry it points into
}

bool IsoMateric::init()
{
	if (!renderer.init())
		return false;

	int fbW, fbH;
	glfwGetFramebufferSize(window, &fbW, &fbH);
	onFramebufferResize(fbW, fbH);

	audio.init(kAssets + "/sounds");

	// typewriter font for the item screen and prompts (default font stays for the sidebar)
	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->AddFontDefault();
	const std::string font = kAssets + "/fonts/Cousine-Regular.ttf";
	fontBody = io.Fonts->AddFontFromFileTTF(font.c_str(), 20.0f);
	fontTitle = io.Fonts->AddFontFromFileTTF(font.c_str(), 34.0f);
	if (!fontBody || !fontTitle)
		LOG_WARN("ui: could not load %s, using the default font", font.c_str());

	createPlayer();
	loadLevel(levelPath);
	if (level.sizeX == 0)
	{
		LOG_WARN("falling back to an empty 16x16 level");
		level = Level::flat(16, 16);
		resetPlayer();
	}

	ScriptContext ctx;
	ctx.registry = &registry;
	ctx.level = &level;
	ctx.audio = &audio;
	ctx.models = &models;
	ctx.player = playerEntity;
	ctx.time = &elapsed;
	ctx.inventory = &inventory;
	ctx.lockCamera = [this](glm::vec3 dir, float pitch) { lockCamera(dir, pitch); };
	ctx.unlockCamera = [this]() { unlockCamera(); };
	ctx.assetsDir = kAssets;
	scripts = std::make_unique<ScriptHost>(ctx);
	scripts->load(kAssets + "/" + level.script);
	return true;
}

bool IsoMateric::onEscape()
{
	if (inventory.open)
	{
		inventory.toggle();
		return true;
	}
	if (camLock.active && camLock.mirror != entt::null)
	{
		endMirrorGaze();
		return true;
	}
	return false;
}

entt::entity IsoMateric::nearestMirror() const
{
	const glm::vec3 feet = player().feet();
	entt::entity best = entt::null;
	float bestDist = 1.7f;
	for (auto [e, t, m] : registry.view<const Transform, const Mirror>().each())
	{
		if (!m.interactive)
			continue;
		const glm::vec3 n(std::sin(t.yaw), 0.0f, std::cos(t.yaw));
		glm::vec3 d = feet - t.position;
		d.y = 0.0f;
		const float front = glm::dot(d, n);
		const float side = std::abs(glm::dot(d, glm::vec3(n.z, 0.0f, -n.x)));
		if (front > 0.2f && side < m.size.x * 0.5f + 0.4f && front < bestDist && std::abs(feet.y - (t.position.y - m.size.y * 0.5f)) < 1.5f)
		{
			best = e;
			bestDist = front;
		}
	}
	return best;
}

void IsoMateric::requestCameraMode(Camera::Mode m, const glm::vec3* face, float pitch)
{
	const Camera::Mode current = camFade.active ? camFade.target : camera.mode();
	if (m == current && !face)
		return;
	if (m == current && camFade.active)
	{
		camFade.hasFace = face != nullptr; // already heading there: just update where to look
		if (face)
			camFade.face = *face;
		camFade.pitch = pitch;
		return;
	}
	camFade.active = true;
	camFade.switched = false;
	camFade.t = 0.0f;
	camFade.target = m;
	camFade.hasFace = face != nullptr;
	if (face)
		camFade.face = *face;
	camFade.pitch = pitch;
}

void IsoMateric::updateCameraFade(float dt)
{
	// a quick dip to black hides the jump between the two cameras
	constexpr float kHalf = 0.16f;
	if (!camFade.active)
		return;
	camFade.t += dt;
	if (!camFade.switched && camFade.t >= kHalf)
	{
		setMouseLook(false);
		camera.setMode(camFade.target);
		if (camFade.hasFace && camFade.target == Camera::Mode::FirstPerson)
			camera.face(camFade.face, camFade.pitch);
		camFade.switched = true;
	}
	if (camFade.t >= 2.0f * kHalf)
		camFade.active = false;
}

void IsoMateric::updateCameraZones()
{
	if (camLock.active || mode != Mode::Play)
		return;
	const glm::vec3 feet = player().feet() + glm::vec3(0.0f, 0.1f, 0.0f);
	entt::entity inside = entt::null;
	for (auto [e, z] : registry.view<const CameraZone>().each())
		if (glm::all(glm::greaterThanEqual(feet, z.lo)) && glm::all(glm::lessThanEqual(feet, z.hi)))
			inside = e;
	if (inside == activeZone)
		return;

	const Camera::Mode current = camFade.active ? camFade.target : camera.mode();
	const glm::vec3 facing(player().facing.x, 0.0f, player().facing.y);
	if (inside == entt::null)
	{
		// left the zone: give back the camera it took, unless the player picked one since
		if (zoneSwitched && current != modeBeforeZone)
			requestCameraMode(modeBeforeZone);
		zoneSwitched = false;
	}
	else
	{
		const CameraZone& z = registry.get<CameraZone>(inside);
		if (activeZone == entt::null)
			modeBeforeZone = current; // from zone to zone keeps the original camera
		const Camera::Mode want = z.firstPerson ? Camera::Mode::FirstPerson : Camera::Mode::Isometric;
		if (want != current)
		{
			requestCameraMode(want, &facing, 0.05f);
			zoneSwitched = true;
		}
	}
	activeZone = inside;
}

void IsoMateric::rebuildColliders()
{
	level.colliders.clear();
	for (auto [e, t, b] : registry.view<const Transform, const Box>().each())
		if (b.solid)
			level.colliders.push_back({t.position - b.size * 0.5f, t.position + b.size * 0.5f});
}

void IsoMateric::lockCamera(glm::vec3 dir, float pitch)
{
	if (!camLock.active)
		camLock.previous = camFade.active ? camFade.target : camera.mode();
	camLock.active = true;
	camLock.mirror = entt::null;
	requestCameraMode(Camera::Mode::FirstPerson, &dir, pitch);
}

void IsoMateric::unlockCamera()
{
	if (!camLock.active)
		return;
	camLock.active = false;
	camLock.mirror = entt::null;
	// a forced first-person zone keeps first person
	const CameraZone* zone = registry.valid(activeZone) ? registry.try_get<CameraZone>(activeZone) : nullptr;
	if (!(zone && zone->force && zone->firstPerson))
		requestCameraMode(camLock.previous);
}

void IsoMateric::startMirrorGaze(entt::entity e)
{
	const Transform& t = registry.get<Transform>(e);
	const glm::vec3 n(std::sin(t.yaw), 0.0f, std::cos(t.yaw));
	Player& p = player();
	// step in front of the glass and face it
	p.position.x = t.position.x + n.x * 0.95f;
	p.position.z = t.position.z + n.z * 0.95f;
	p.velocity = glm::vec3(0.0f);
	p.path.clear();
	p.facing = glm::vec2(-n.x, -n.z);
	const float eyeY = p.feet().y + eyeHeight;
	const float pitch = -std::atan2(t.position.y - eyeY, 0.95f);
	lockCamera(-n, pitch);
	camLock.mirror = e;
	scripts->event("mirror_look", t.position);
}

void IsoMateric::endMirrorGaze()
{
	glm::vec3 at = registry.valid(camLock.mirror) ? registry.get<Transform>(camLock.mirror).position : player().feet();
	unlockCamera();
	scripts->event("mirror_leave", at);
}

entt::entity IsoMateric::nearestPickup() const
{
	const glm::vec3 feet = player().feet();
	entt::entity best = entt::null;
	float bestDist = 1.3f; // reach
	for (auto [e, t, p] : registry.view<const Transform, const Pickup>().each())
	{
		glm::vec2 d(t.position.x - feet.x, t.position.z - feet.z);
		float dist = glm::length(d);
		if (dist < bestDist && std::abs(t.position.y - feet.y) < 1.6f)
		{
			best = e;
			bestDist = dist;
		}
	}
	return best;
}

void IsoMateric::updateFlashlight()
{
	RenderSettings& rs = renderSettings;
	rs.spotOn = inventory.equipped == "flashlight" && inventory.flashlightOn;
	if (!rs.spotOn)
		return;
	const Player& p = player();
	glm::vec3 dir;
	if (camera.mode() == Camera::Mode::FirstPerson)
		dir = -camera.toCamera(); // where you look
	else
		dir = glm::normalize(glm::vec3(p.facing.x, -0.42f, p.facing.y)); // held at the chest, aimed ahead and down
	rs.spotDir = glm::normalize(dir);
	rs.spotPos = p.position + glm::vec3(0.0f, 0.2f, 0.0f) + glm::vec3(p.facing.x, 0.0f, p.facing.y) * 0.3f;
	// a cheap bulb: tiny wobble in brightness
	rs.spotIntensity = 2.4f * (0.96f + 0.04f * std::sin(elapsed * 23.0f) * std::sin(elapsed * 3.7f));
}

void IsoMateric::drawHud()
{
	const ImVec2 display = ImGui::GetIO().DisplaySize;
	ImDrawList* dl = ImGui::GetForegroundDrawList();
	ImFont* f = fontBody ? fontBody : ImGui::GetFont();
	const float fs = fontBody ? fontBody->FontSize : 16.0f;
	const float viewCenter = viewX + (display.x - viewX) * 0.5f;
	auto centered = [&](float y, const std::string& t, ImU32 col) {
		ImVec2 sz = f->CalcTextSizeA(fs, FLT_MAX, 0.0f, t.c_str());
		ImVec2 p(viewCenter - sz.x * 0.5f, y);
		dl->AddRectFilled(ImVec2(p.x - 12, p.y - 6), ImVec2(p.x + sz.x + 12, p.y + sz.y + 6), IM_COL32(0, 0, 0, 150));
		dl->AddText(f, fs, p, col, t.c_str());
	};

	if (!inventory.open && mode == Mode::Play)
	{
		entt::entity e = camLock.active ? entt::null : nearestPickup();
		if (e != entt::null)
		{
			const ItemDef* d = findItem(registry.get<Pickup>(e).item);
			centered(display.y * 0.82f, std::string("[F]  Pick up ") + (d ? d->name : "item"), IM_COL32(215, 205, 185, 255));
		}
		else if (camLock.active && camLock.mirror != entt::null)
			centered(display.y * 0.9f, "[F]  Step back", IM_COL32(150, 142, 128, 200));
		else if (!camLock.active && nearestMirror() != entt::null)
			centered(display.y * 0.82f, "[F]  Look into the mirror", IM_COL32(215, 205, 185, 255));
	}
	if (camFade.active)
	{
		const float a = 1.0f - std::abs(camFade.t - 0.16f) / 0.16f;
		dl->AddRectFilled(ImVec2(viewX, 0.0f), ImVec2(display.x, display.y), IM_COL32(0, 0, 0, static_cast<int>(255 * std::clamp(a, 0.0f, 1.0f))));
	}
	if (hudMessageTime > 0.0f && !inventory.open)
	{
		hudMessageTime -= ImGui::GetIO().DeltaTime;
		centered(display.y * 0.75f, hudMessage, IM_COL32(215, 205, 185, static_cast<int>(255 * std::min(1.0f, hudMessageTime))));
	}
	inventory.draw(glm::vec2(display.x, display.y), fontTitle, fontBody, static_cast<float>(glfwGetTime()));
}

void IsoMateric::applyQuality(int q)
{
	// graphics presets, to see what each feature costs; everything stays editable after.
	//                     very low  low   medium  high  ultra
	static const int   grassDensity[] = {0, 10, 16, 24, 48};
	static const int   mirrors[]      = {0, 1, 2, 2, 4};
	static const int   rainDrops[]    = {500, 1200, 2000, 2500, 4000};
	static const int   raySteps[]     = {16, 16, 24, 32, 48};
	RenderSettings& rs = renderSettings;
	quality = q = std::clamp(q, 0, 4);
	rs.shadows = q >= 1;
	rs.grass = q >= 1;
	rs.grassDensity = std::max(grassDensity[q], 4);
	rs.bloom = q >= 3;
	rs.bloomPasses = q >= 4 ? 3 : 2;
	rs.godRays = q >= 4;
	rs.godRaySteps = raySteps[q];
	rs.maxMirrors = mirrors[q];
	rain.maxDrops = rainDrops[q];
}

void IsoMateric::createPlayer()
{
	playerEntity = registry.create();
	registry.emplace<Transform>(playerEntity);
	registry.emplace<Player>(playerEntity);
	registry.emplace<Name>(playerEntity, Name{"player"});
	if (auto model = models.get(kAssets + "/" + kPlayerModel))
	{
		registry.emplace<MeshRenderer>(playerEntity, MeshRenderer{model, 1.8f, kPlayerModelYaw, glm::vec4(1.0f)});
		registry.emplace<Animator>(playerEntity);
	}
}

void IsoMateric::onFramebufferResize(int width, int height)
{
	renderer.resize(width, height);
	updateLayout();
}

void IsoMateric::updateLayout()
{
	int fbW, fbH;
	glfwGetFramebufferSize(window, &fbW, &fbH);
	glfwGetWindowSize(window, &windowW, &windowH);
	float scale = windowW > 0 ? static_cast<float>(fbW) / static_cast<float>(windowW) : 1.0f;

	// the scene gets whatever the sidebar leaves (at least 100 px)
	viewX = showDebug ? std::min(sidebarWidth, static_cast<float>(windowW) - 100.0f) : 0.0f;
	viewX = std::max(viewX, 0.0f);
	int sidePx = static_cast<int>(std::lround(viewX * scale));
	renderer.setViewRect(sidePx, fbW - sidePx);
	camera.setViewport(fbW - sidePx, fbH);
}

void IsoMateric::loadLevel(const std::string& path)
{
	Level loaded;
	if (!loaded.load(path))
		return;
	bool cycling = dayNight.running;
	setDayNight(false); // re-capture the new level's own sun below
	level = std::move(loaded);
	setDayNight(cycling);
	generateAsteroids();
	if (scripts)
		scripts->load(kAssets + "/" + level.script); // each level can bring its own script
	levelPath = path;
	resetPlayer();
}

void IsoMateric::resetPlayer()
{
	player().placeAt(level, level.spawn);
	camera.focusGoal = player().position;
	camera.snap();
}

void IsoMateric::run()
{
	double last = glfwGetTime();
	double accumulator = 0.0;

	while (!glfwWindowShouldClose(window))
	{
		{
			PROFILE_SCOPE("events");
			glfwPollEvents();
		}

		double now = glfwGetTime();
		double frameTime = std::min(now - last, 0.25); // avoid a spiral after a stall
		last = now;
		float dt = static_cast<float>(frameTime);
		frameMsSmoothed = frameMsSmoothed * 0.95f + static_cast<float>(frameTime * 1000.0) * 0.05f;
		// the item screen pauses the world (and its clock: grass, rain and fire freeze)
		const bool paused = inventory.open;
		const float worldDt = paused ? 0.0f : dt;
		elapsed += worldDt;

		glfwGetWindowSize(window, &windowW, &windowH);

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		const ImGuiIO& io = ImGui::GetIO();
		inputState.keyboardBlocked = io.WantCaptureKeyboard;
		inputState.mouseBlocked = io.WantCaptureMouse;

		{
			PROFILE_SCOPE("input");
			// hover is computed with the camera the user is currently looking at
			updateHover();
			handleFrameInput(dt);
		}

		rebuildColliders();
		{
			PROFILE_SCOPE("physics");
			accumulator += paused ? 0.0 : frameTime;
			while (accumulator >= kFixedStep)
			{
				fixedUpdate(static_cast<float>(kFixedStep));
				accumulator -= kFixedStep;
			}
		}

		{
			PROFILE_SCOPE("scripts");
			glm::vec3 feet = player().feet();
			if (pendingEvents.jumped)
				scripts->event("jump", feet);
			if (pendingEvents.landed)
				scripts->event("land", feet);
			if (pendingEvents.respawned)
				scripts->event("respawn", feet);
			pendingEvents = {};
			scripts->update(worldDt);
		}

		{
			PROFILE_SCOPE("animation");
			syncPlayerEntity(worldDt);
			updateAnimation(worldDt);
		}
		if (dayNight.running)
			advanceDayNight(worldDt);
		{
			PROFILE_SCOPE("particles");
			updateParticles(registry, worldDt, rng);
		}
		{
			PROFILE_SCOPE("weather");
			updateWeather(worldDt);
			for (Asteroid& a : asteroids)
				a.angle += a.speed * worldDt;
			updateSky();
			updateFlashlight();
		}

		// camera after movement and collision, so it never trails a frame behind
		updateCameraZones();
		updateCameraFade(dt);
		if (followPlayer || camera.isPerspective())
			camera.focusGoal = player().position;
		// first-person eye above the feet, eased down when crouching; focus is the body centre
		const float eyeTarget = player().crouching ? eyeHeight * 0.6f : eyeHeight;
		eyeNow += (eyeTarget - eyeNow) * (1.0f - std::exp(-dt * 12.0f));
		camera.eyeOffset = eyeNow - player().size.y * 0.5f;
		camera.update(dt);
		if (camera.isPerspective())
			camera.focus = camera.focusGoal; // no lag behind the player's head
		camera.snapUnits = pixelSnap && renderSettings.pixelSize > 1 && !camera.isPerspective() ? static_cast<float>(renderSettings.pixelSize) / camera.zoom : 0.0f;

		if (autoReload && (reloadTimer += dt) > 0.5f)
		{
			reloadTimer = 0.0f;
			renderer.reloadShadersIfChanged();
			scripts->reloadIfChanged();
		}

		{
			PROFILE_SCOPE("ui");
			if (showDebug)
				drawUi(dt);
			if (showLabels)
				drawLabels();
			drawHud();
			ImGui::Render();
			updateLayout(); // sidebar may have been resized or toggled
		}

		FrameData frame;
		{
			PROFILE_SCOPE("build frame");
			buildFrame(frame);
		}
		{
			PROFILE_SCOPE("render");
			renderer.render(camera, level, frame, renderSettings);
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		}

		{
			PROFILE_SCOPE("swap");
			glfwSwapBuffers(window);
		}
		inputState.endFrame();
		PROFILE_FRAME();
	}
}

void IsoMateric::handleFrameInput(float dt)
{
	Input& in = inputState;

	if (in.pressed(Action::ToggleDebug))
		showDebug = !showDebug;

	// item screen: takes over the keys while open
	if (inventory.open)
	{
		Inventory::Keys k;
		k.left = in.pressed(Action::MoveLeft);
		k.right = in.pressed(Action::MoveRight);
		k.up = in.pressed(Action::MoveForward);
		k.down = in.pressed(Action::MoveBack);
		k.confirm = in.pressed(Action::Interact);
		k.close = in.pressed(Action::Inventory);
		if (inventory.update(k, dt))
			audio.play("click", 0.5f);
		return;
	}
	if (mode == Mode::Play && in.pressed(Action::Inventory))
	{
		inventory.toggle();
		audio.play("click", 0.6f);
		return;
	}
	if (camLock.active)
	{
		// looking into a mirror: F, Backspace or a step away ends it
		if (camLock.mirror != entt::null &&
		    (in.pressed(Action::Interact) || in.pressed(Action::Inventory) || in.pressed(Action::MoveBack) || in.pressed(Action::MoveLeft) ||
		     in.pressed(Action::MoveRight)))
			endMirrorGaze();
	}
	else if (mode == Mode::Play && in.pressed(Action::Interact))
	{
		entt::entity e = nearestPickup();
		if (e == entt::null)
		{
			if (entt::entity m = nearestMirror(); m != entt::null)
				startMirrorGaze(m);
		}
		else
		{
			const std::string id = registry.get<Pickup>(e).item;
			const ItemDef* d = findItem(id);
			glm::vec3 at = registry.get<Transform>(e).position;
			inventory.add(id);
			registry.destroy(e);
			hudMessage = "Picked up the " + (d ? d->name : id) + ".";
			hudMessageTime = 3.0f;
			audio.play("chime", 0.6f);
			scripts->event(("pickup:" + id).c_str(), at);
		}
	}
	if (in.pressed(Action::Flashlight) && inventory.equipped == "flashlight")
	{
		inventory.flashlightOn = !inventory.flashlightOn;
		audio.play("click", 0.8f);
	}
	if (in.pressed(Action::ToggleFollow))
		followPlayer = !followPlayer;
	if (in.pressed(Action::ToggleEditor))
		mode = mode == Mode::Play ? Mode::Editor : Mode::Play;
	if (in.pressed(Action::RotateLeft))
		camera.rotate(1);
	if (in.pressed(Action::RotateRight))
		camera.rotate(-1);
	if (in.pressed(Action::Jump))
		jumpRequested = true; // consumed by the next physics step
	if (in.pressed(Action::CycleCamera) && !camLock.active)
	{
		const CameraZone* zone = registry.valid(activeZone) ? registry.try_get<CameraZone>(activeZone) : nullptr;
		if (zone && zone->force)
		{
			hudMessage = zone->firstPerson ? "It is too cramped to step back." : "You need to see what is around you.";
			hudMessageTime = 2.0f;
		}
		else
		{
			const Camera::Mode current = camFade.active ? camFade.target : camera.mode();
			const glm::vec3 facing(player().facing.x, 0.0f, player().facing.y);
			requestCameraMode(current == Camera::Mode::FirstPerson ? Camera::Mode::Isometric : Camera::Mode::FirstPerson, &facing);
			zoneSwitched = false; // the player chose: leaving the zone keeps this camera
		}
	}

	// first person: hold the right (play) or middle mouse to look. The look starts only when
	// the press is on the scene, then keeps going until release even if the hidden cursor
	// passes over the sidebar.
	if (camera.isPerspective())
	{
		if (!mouseLooking && (in.mousePressed(GLFW_MOUSE_BUTTON_MIDDLE) || (mode == Mode::Play && in.mousePressed(GLFW_MOUSE_BUTTON_RIGHT))))
			setMouseLook(true);
		else if (mouseLooking && !in.mouseHeld(GLFW_MOUSE_BUTTON_MIDDLE) && !in.mouseHeld(GLFW_MOUSE_BUTTON_RIGHT))
			setMouseLook(false);
		if (mouseLooking)
			camera.look(static_cast<float>(in.mouseDeltaX) * 0.0025f, static_cast<float>(in.mouseDeltaY) * 0.0025f);
	}


	// zoom: keys are continuous and frame-rate independent, the wheel steps
	float zoomDir = (in.down(Action::ZoomIn) ? 1.0f : 0.0f) - (in.down(Action::ZoomOut) ? 1.0f : 0.0f);
	camera.zoom *= std::exp(zoomDir * dt * 2.0f);
	if (!in.mouseBlocked && in.scroll != 0.0f && !camera.isPerspective())
		camera.zoom *= std::pow(1.15f, in.scroll);
	camera.zoom = std::clamp(camera.zoom, kMinZoom, kMaxZoom);

	// middle mouse drag pans the camera
	if (!camera.isPerspective() && in.mouseDown(GLFW_MOUSE_BUTTON_MIDDLE) && (in.mouseDeltaX != 0.0 || in.mouseDeltaY != 0.0))
	{
		int fbW, fbH;
		glfwGetFramebufferSize(window, &fbW, &fbH);
		float scale = static_cast<float>(fbW) / static_cast<float>(std::max(windowW, 1));
		float dx = static_cast<float>(in.mouseDeltaX) * scale / camera.zoom;
		float dy = static_cast<float>(in.mouseDeltaY) * scale / camera.zoom;
		followPlayer = false;
		camera.focusGoal += -camera.right() * dx + camera.forward() * (dy / std::sin(camera.elevation()));
		camera.focus = camera.focusGoal;
	}

	if (!hover.valid)
		return;
	const glm::ivec2 column(hover.block.x, hover.block.z);

	if (mode == Mode::Editor)
	{
		if (in.mousePressed(GLFW_MOUSE_BUTTON_LEFT))
		{
			level.setHeight(column.x, column.y, level.height(column.x, column.y) + 1);
			audio.play("place");
		}
		if (in.mousePressed(GLFW_MOUSE_BUTTON_RIGHT))
		{
			level.setHeight(column.x, column.y, level.height(column.x, column.y) - 1);
			audio.play("remove");
		}
	}
	else if (in.mousePressed(GLFW_MOUSE_BUTTON_LEFT))
	{
		std::vector<glm::ivec2> path = findPath(level, player().tile(), column);
		player().path.assign(path.begin(), path.end());
		audio.play(path.empty() ? "denied" : "click", 0.6f);
	}
}

void IsoMateric::setMouseLook(bool on)
{
	if (on == mouseLooking)
		return;
	mouseLooking = on;
	glfwSetInputMode(window, GLFW_CURSOR, on ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
	inputState.resetMotion();
	if (on && glfwRawMouseMotionSupported())
		glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
	ImGuiIO& io = ImGui::GetIO();
	if (on)
		io.ConfigFlags |= ImGuiConfigFlags_NoMouse; // ImGui must not react to the hidden cursor
	else
		io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
}

void IsoMateric::fixedUpdate(float dt)
{
	const Input& in = inputState;
	glm::vec3 f = camera.forward(), r = camera.right();
	float fwd = (in.down(Action::MoveForward) ? 1.0f : 0.0f) - (in.down(Action::MoveBack) ? 1.0f : 0.0f);
	float side = (in.down(Action::MoveRight) ? 1.0f : 0.0f) - (in.down(Action::MoveLeft) ? 1.0f : 0.0f);
	glm::vec2 move(f.x * fwd + r.x * side, f.z * fwd + r.z * side);
	if (glm::dot(move, move) > 0.0f)
		move = glm::normalize(move); // diagonal input is not faster
	if (camLock.active)
	{
		move = glm::vec2(0.0f);
		jumpRequested = false;
	}
	player().sprint = in.down(Action::Sprint);
	player().crouch = in.down(Action::Crouch);
	Player::Events e = player().update(dt, level, move, jumpRequested);
	jumpRequested = false;
	pendingEvents.jumped |= e.jumped;
	pendingEvents.landed |= e.landed;
	pendingEvents.respawned |= e.respawned;
}

void IsoMateric::syncPlayerEntity(float dt)
{
	const Player& p = player();
	Transform& t = registry.get<Transform>(playerEntity);
	t.position = p.feet();
	if (auto* mr = registry.try_get<MeshRenderer>(playerEntity))
	{
		// the model has no crouch animation: squat it a little instead
		const float squashTarget = p.crouching ? 0.72f : 1.0f;
		mr->squash += (squashTarget - mr->squash) * (1.0f - std::exp(-dt * 14.0f));
	}
	float target = std::atan2(p.facing.x, p.facing.y);
	t.yaw += wrapAngle(target - t.yaw) * (1.0f - std::exp(-dt * 14.0f));

	if (auto* anim = registry.try_get<Animator>(playerEntity))
	{
		anim->playing = p.horizontalSpeed > 0.3f;
		anim->speed = p.grounded ? p.horizontalSpeed / p.speed * 1.3f : 0.4f;
	}
}

void IsoMateric::updateAnimation(float dt)
{
	auto view = registry.view<MeshRenderer, Animator>();
	for (auto entity : view)
	{
		const MeshRenderer& mr = view.get<MeshRenderer>(entity);
		Animator& anim = view.get<Animator>(entity);
		if (!mr.model)
			continue;
		const Model& m = *mr.model;
		bool hasClip = anim.clip >= 0 && anim.clip < static_cast<int>(m.clips.size());
		if (hasClip && anim.playing)
		{
			float duration = m.clips[anim.clip].duration;
			anim.time += dt * anim.speed;
			if (duration > 0.0f)
				anim.time = std::fmod(std::fmod(anim.time, duration) + duration, duration);
		}
		m.pose(hasClip ? anim.clip : -1, anim.time, anim.globals, anim.joints);
	}
}

void IsoMateric::updateHover()
{
	hover.valid = false;
	const float viewW = static_cast<float>(windowW) - viewX;
	if (inputState.mouseBlocked || viewW <= 0.0f || windowH <= 0 || inputState.mouseX < viewX)
		return;

	Ray ray = camera.screenRay((static_cast<float>(inputState.mouseX) - viewX) / viewW,
	                           static_cast<float>(inputState.mouseY) / static_cast<float>(windowH));
	float best = std::numeric_limits<float>::infinity();
	for (int x = 0; x < level.sizeX; x++)
	{
		for (int z = 0; z < level.sizeZ; z++)
		{
			for (int y = 0; y < level.height(x, z); y++)
			{
				glm::vec3 c(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
				float t;
				glm::ivec3 n;
				if (rayBox(ray, c - 0.5f, c + 0.5f, t, n) && t < best)
				{
					best = t;
					hover.valid = true;
					hover.block = glm::ivec3(x, y, z);
					hover.normal = n;
				}
			}
		}
	}
}

void IsoMateric::buildFrame(FrameData& frame)
{
	// see-through window around the player, cut in the world shader
	renderSettings.cutawayCenter = player().position;
	renderSettings.cutawayFloor = player().feet().y + 0.05f;
	renderSettings.cutawayDir = camera.isPerspective() ? glm::normalize(camera.position() - player().position) : camera.toCamera();
	const bool firstPerson = camera.mode() == Camera::Mode::FirstPerson;

	const int maxH = std::max(level.maxHeight(), 2);
	frame.boundsMin = glm::vec3(-0.5f);
	frame.boundsMax = glm::vec3(static_cast<float>(level.sizeX) - 0.5f, static_cast<float>(maxH) - 0.5f,
	                            static_cast<float>(level.sizeZ) - 0.5f);

	// grass ground withers along with the blades
	auto columnColor = [&](int x, int y, int z) {
		glm::vec3 c = blockColor(level, x, y, z);
		if (level.tile(x, z) == 'g' && renderSettings.grassDry > 0.0f)
			c = glm::mix(c, glm::vec3(0.36f, 0.32f, 0.23f) * (0.9f + 0.15f * tileNoise(x, z)), glm::clamp(renderSettings.grassDry, 0.0f, 1.0f));
		return c;
	};
	for (int x = 0; x < level.sizeX; x++)
	{
		for (int z = 0; z < level.sizeZ; z++)
		{
			// floor block plus one tall box for everything stacked on it: no hidden faces
			// between stacked cubes, which would otherwise flicker through shared edges
			const int h = level.height(x, z);
			const float fx = static_cast<float>(x), fz = static_cast<float>(z);
			if (h >= 1)
				frame.opaque.push_back({glm::vec3(fx, 0.0f, fz), glm::vec3(1.0f), glm::vec4(columnColor(x, 0, z), 1.0f)});
			if (h >= 2)
				frame.opaque.push_back({glm::vec3(fx, static_cast<float>(h) * 0.5f, fz), glm::vec3(1.0f, static_cast<float>(h - 1), 1.0f),
				                        glm::vec4(columnColor(x, h - 1, z), 1.0f)});
		}
	}

	// entities with a model
	auto meshes = registry.view<Transform, MeshRenderer, Animator>();
	for (auto entity : meshes)
	{
		const Transform& t = meshes.get<Transform>(entity);
		const MeshRenderer& mr = meshes.get<MeshRenderer>(entity);
		const Animator& anim = meshes.get<Animator>(entity);
		if (!mr.model || anim.globals.empty())
			continue;
		// the camera is inside the head in first person: the body only shows in mirrors
		const bool hiddenBody = firstPerson && entity == playerEntity;
		const Model& m = *mr.model;
		float h = m.boundsMax.y - m.boundsMin.y;
		float scale = h > 0.0f ? mr.height / h : 1.0f;
		glm::vec3 c = (m.boundsMin + m.boundsMax) * 0.5f;
		glm::mat4 xf = glm::translate(glm::mat4(1.0f), t.position) *
		               glm::rotate(glm::mat4(1.0f), t.yaw + mr.yawOffset, glm::vec3(0.0f, 1.0f, 0.0f)) *
		               glm::scale(glm::mat4(1.0f), glm::vec3(scale, scale * mr.squash, scale)) *
		               glm::translate(glm::mat4(1.0f), glm::vec3(-c.x, -m.boundsMin.y, -c.z));
		frame.meshes.push_back({&m, xf, &anim.globals, &anim.joints, mr.tint, hiddenBody || mr.reflectionOnly, !mr.reflectionOnly});
	}
	if (!registry.all_of<MeshRenderer>(playerEntity) && !firstPerson)
		frame.opaque.push_back({player().position, player().size, glm::vec4(0.9f, 0.2f, 0.2f, 1.0f)});

	// endless tree trunks: bark from far below to far above, with branches and leaves
	for (const glm::ivec2& t : level.trunks)
	{
		auto hash = [](int a, int b) {
			unsigned h = static_cast<unsigned>(a) * 374761393u + static_cast<unsigned>(b) * 668265263u;
			h = (h ^ (h >> 13)) * 1274126177u;
			return static_cast<float>((h ^ (h >> 16)) & 0xffff) / 65535.0f;
		};
		const glm::vec3 base(static_cast<float>(t.x), 0.0f, static_cast<float>(t.y));
		for (int y = -45; y <= 45; y++)
		{
			float v = hash(y, t.x * 31 + t.y);
			glm::vec3 bark = glm::vec3(0.34f, 0.23f, 0.14f) * (0.8f + 0.4f * v);
			frame.opaque.push_back({base + glm::vec3(0.0f, static_cast<float>(y), 0.0f), glm::vec3(1.0f), glm::vec4(bark, 1.0f)});

			// a branch every few blocks, never right at the island (y -2 .. 4)
			if ((y < -2 || y > 4) && hash(y * 7, t.x + 3) > 0.78f)
			{
				static const glm::vec3 dirs[4] = {{1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}};
				glm::vec3 dir = dirs[static_cast<int>(hash(y, 99) * 3.99f)];
				int length = 2 + static_cast<int>(hash(y, 7) * 3.0f);
				for (int i = 1; i <= length; i++)
				{
					glm::vec3 p = base + glm::vec3(0.0f, static_cast<float>(y) + 0.25f * static_cast<float>(i), 0.0f) + dir * (0.5f + 0.55f * static_cast<float>(i));
					frame.opaque.push_back({p, glm::vec3(0.5f), glm::vec4(bark * 0.95f, 1.0f)});
				}
				glm::vec3 tip = base + glm::vec3(0.0f, static_cast<float>(y) + 0.25f * static_cast<float>(length) + 0.4f, 0.0f) +
				                dir * (0.6f + 0.55f * static_cast<float>(length));
				for (int lx = -1; lx <= 1; lx++)
					for (int lz = -1; lz <= 1; lz++)
						for (int ly = 0; ly <= 1; ly++)
						{
							if (hash(lx * 5 + lz * 11 + ly * 3, y) < 0.25f)
								continue;
							float g = 0.75f + 0.5f * hash(lx + ly, lz + y);
							frame.opaque.push_back({tip + glm::vec3(lx, ly, lz) * 0.55f, glm::vec3(0.6f),
							                        glm::vec4(glm::vec3(0.22f, 0.45f, 0.24f) * g, 1.0f)});
						}
			}
		}
	}

	// asteroids orbit the level centre
	const glm::vec3 levelCenter(static_cast<float>(level.sizeX - 1) * 0.5f, 0.0f, static_cast<float>(level.sizeZ - 1) * 0.5f);
	for (const Asteroid& a : asteroids)
	{
		glm::vec3 c = levelCenter + glm::vec3(std::cos(a.angle) * a.orbitRadius, a.height + std::sin(elapsed * 0.4f + a.bobPhase) * 0.3f,
		                                      std::sin(a.angle) * a.orbitRadius);
		for (const auto& [offset, size] : a.parts)
			frame.opaque.push_back({c + offset, glm::vec3(size), glm::vec4(a.color, 1.0f)});
	}

	// decorative boxes from scripts (props, roofs, signs); glowing ones are unlit
	for (auto [entity, t, box] : registry.view<Transform, Box>().each())
		(box.reflectionOnly ? frame.reflectionOnly
		 : box.glow         ? frame.overlay
		 : box.color.a < 1.0f ? frame.transparent
		                      : frame.opaque)
			.push_back({t.position, box.size, box.color, box.emissive});

	// items lying around: small, bobbing, with a faint glint so they can be found
	const entt::entity highlighted = mode == Mode::Play && !inventory.open ? nearestPickup() : entt::null;
	for (auto [entity, t, pick] : registry.view<Transform, Pickup>().each())
	{
		const ItemDef* d = findItem(pick.item);
		if (!d)
			continue;
		const float s = 0.38f;
		const float phase = t.position.x * 1.3f + t.position.z * 0.7f;
		const glm::vec3 base = t.position + glm::vec3(0.0f, 0.04f * std::sin(elapsed * 2.0f + phase), 0.0f);
		for (const ItemBox& b : d->boxes)
			frame.opaque.push_back({base + b.offset * s, glm::max(b.size * s, glm::vec3(0.012f)), glm::vec4(b.color, 1.0f)});
		if (entity == highlighted)
		{
			// in reach: a warm, pulsing shell around every part
			const float pulse = 0.35f + 0.25f * std::sin(static_cast<float>(glfwGetTime()) * 6.0f);
			for (const ItemBox& b : d->boxes)
				frame.particlesAdditive.push_back({base + b.offset * s, glm::max(b.size * s, glm::vec3(0.012f)) + glm::vec3(0.035f),
				                                   glm::vec4(1.0f, 0.75f, 0.4f, pulse)});
		}
		float glint = std::pow(std::max(0.0f, std::sin(elapsed * 2.6f + phase)), 12.0f);
		if (glint > 0.02f)
			frame.particlesAdditive.push_back({base + glm::vec3(0.0f, 0.12f, 0.0f), glm::vec3(0.07f * glint + 0.02f), glm::vec4(1.0f, 0.95f, 0.8f, glint)});
	}

	// mirrors: yaw 0 faces +z, 90 degrees faces +x
	for (auto [entity, t, m] : registry.view<Transform, Mirror>().each())
	{
		const glm::vec3 normal(std::sin(t.yaw), 0.0f, std::cos(t.yaw));
		const glm::vec3 right(std::cos(t.yaw), 0.0f, -std::sin(t.yaw));
		frame.mirrors.push_back({t.position, normal, right * (m.size.x * 0.5f), glm::vec3(0.0f, m.size.y * 0.5f, 0.0f), m.tint, m.tarnish});
	}

	// glass
	for (auto [entity, t, g] : registry.view<Transform, Glass>().each())
		frame.glass.push_back({t.position, g.radius, g.tint, g.ior, g.roughness, g.absorption, g.dispersion});

	// rain: thin streaks and splashes, dimmed at night so they do not glow
	if (!rain.drops.empty() || !rain.splashes.empty())
	{
		float sunLum = glm::dot(level.sunColor, glm::vec3(0.3f, 0.6f, 0.1f));
		float bright = glm::clamp(level.ambient * 1.5f + sunLum * 0.5f, 0.12f, 1.0f);
		glm::vec3 c = glm::vec3(0.75f, 0.8f, 0.92f) * bright;
		for (const Rain::Drop& d : rain.drops)
			frame.particlesAlpha.push_back({d.position, glm::vec3(0.025f, 0.45f, 0.025f), glm::vec4(c, 0.45f)});
		for (const Rain::Splash& s : rain.splashes)
		{
			float f = s.age / rain.splashLife;
			frame.particlesAlpha.push_back({s.position, glm::vec3(0.05f * (1.0f - f) + 0.01f), glm::vec4(c, 0.6f * (1.0f - f))});
		}
	}

	// particles
	auto emitters = registry.view<ParticleEmitter>();
	for (auto entity : emitters)
	{
		const ParticleEmitter& e = emitters.get<ParticleEmitter>(entity);
		auto& out = e.additive ? frame.particlesAdditive : frame.particlesAlpha;
		for (const Particle& p : e.particles)
		{
			float f = p.age / p.life;
			float size = glm::mix(e.sizeStart, e.sizeEnd, f);
			out.push_back({p.position, glm::vec3(size), glm::mix(e.colorStart, e.colorEnd, f)});
		}
	}

	for (const PointLight& l : level.lights)
		frame.overlay.push_back({l.position, glm::vec3(0.2f), glm::vec4(l.color, 1.0f)});

	for (const glm::ivec2& t : player().path)
	{
		float top = static_cast<float>(level.height(t.x, t.y)) - 0.5f;
		frame.overlay.push_back({glm::vec3(t.x, top + 0.03f, t.y), glm::vec3(0.2f, 0.04f, 0.2f), glm::vec4(0.3f, 0.9f, 1.0f, 0.9f)});
	}

	if (mode == Mode::Editor)
	{
		float top = static_cast<float>(level.height(level.spawn.x, level.spawn.y)) - 0.5f;
		frame.overlay.push_back({glm::vec3(level.spawn.x, top + 0.02f, level.spawn.y), glm::vec3(0.9f, 0.03f, 0.9f),
		                         glm::vec4(0.2f, 1.0f, 0.3f, 0.5f)});
	}

	if (hover.valid)
	{
		glm::vec3 c(hover.block);
		if (mode == Mode::Editor)
		{
			frame.overlay.push_back({c, glm::vec3(1.04f), glm::vec4(1.0f, 0.9f, 0.2f, 0.35f)});
		}
		else
		{
			bool ok = level.standable(hover.block.x, hover.block.z);
			float top = static_cast<float>(level.height(hover.block.x, hover.block.z)) - 0.5f;
			frame.overlay.push_back({glm::vec3(c.x, top + 0.02f, c.z), glm::vec3(1.0f, 0.03f, 1.0f),
			                         ok ? glm::vec4(0.3f, 0.9f, 1.0f, 0.35f) : glm::vec4(1.0f, 0.3f, 0.3f, 0.35f)});
		}
	}
}

void IsoMateric::setDayNight(bool enabled)
{
	if (enabled == dayNight.running)
		return;
	if (!enabled)
	{
		setBloodMoon(false);
		dayNight.bloodMoonFade = 0.0f;
	}
	if (enabled)
	{
		dayNight.savedSunDirection = level.sunDirection;
		dayNight.savedSunColor = level.sunColor;
		dayNight.savedAmbient = level.ambient;
		dayNight.savedClear = renderSettings.clearColor;
		dayNight.running = true;
		applyDayNight();
	}
	else
	{
		level.sunDirection = dayNight.savedSunDirection;
		level.sunColor = dayNight.savedSunColor;
		level.ambient = dayNight.savedAmbient;
		renderSettings.clearColor = dayNight.savedClear;
		dayNight.running = false;
	}
}

void IsoMateric::advanceDayNight(float dt)
{
	const float before = dayNight.hour;
	dayNight.hour = std::fmod(dayNight.hour + dt * dayNight.speed, 24.0f);
	auto crossed = [&](float mark) { // handles wrapping past midnight
		return before <= dayNight.hour ? (before < mark && dayNight.hour >= mark) : (before < mark || dayNight.hour >= mark);
	};
	if (crossed(19.0f) && std::uniform_real_distribution<float>(0.0f, 1.0f)(rng) < dayNight.bloodMoonChance)
		setBloodMoon(true);
	if (crossed(6.0f))
		setBloodMoon(false);

	float target = dayNight.bloodMoon ? 1.0f : 0.0f;
	dayNight.bloodMoonFade += (target - dayNight.bloodMoonFade) * (1.0f - std::exp(-dt * 1.5f));
	if (registry.valid(dayNight.ash))
		registry.get<Transform>(dayNight.ash).position = camera.focus + glm::vec3(0.0f, 7.0f, 0.0f);
	applyDayNight();
}

void IsoMateric::setBloodMoon(bool active)
{
	if (active == dayNight.bloodMoon)
		return;
	dayNight.bloodMoon = active;
	glm::vec3 at = player().feet();
	if (active)
	{
		LOG_INFO("blood moon rises");
		audio.play("bloodmoon");
		ParticleEmitter ash = particlePreset("ash");
		dayNight.ash = registry.create();
		registry.emplace<Transform>(dayNight.ash, Transform{camera.focus + glm::vec3(0.0f, 7.0f, 0.0f), 0.0f});
		registry.emplace<ParticleEmitter>(dayNight.ash, ash);
		registry.emplace<Name>(dayNight.ash, Name{"blood moon ash"});
		scripts->event("blood_moon_start", at);
	}
	else
	{
		if (registry.valid(dayNight.ash))
			registry.destroy(dayNight.ash);
		dayNight.ash = entt::null;
		scripts->event("blood_moon_end", at);
	}
}

void IsoMateric::applyDayNight()
{
	// sun rises at 6:00 (east, +x), peaks at 12:00, sets at 18:00; the moon is opposite
	const float angle = (dayNight.hour - 6.0f) / 12.0f * glm::pi<float>();
	const float height = std::sin(angle); // > 0 while the sun is up
	const float day = glm::smoothstep(-0.15f, 0.25f, height);
	const float twilight = 1.0f - glm::smoothstep(0.0f, 0.35f, std::abs(height));

	glm::vec3 toLight;
	glm::vec3 color;
	if (height >= 0.0f)
	{
		toLight = glm::vec3(std::cos(angle) * 0.9f, std::max(height, 0.08f), 0.4f);
		glm::vec3 low(1.0f, 0.5f, 0.25f), high(1.0f, 0.96f, 0.88f);
		color = glm::mix(low, high, glm::smoothstep(0.0f, 0.5f, height)) * glm::smoothstep(-0.02f, 0.12f, height);
	}
	else
	{
		toLight = glm::vec3(-std::cos(angle) * 0.9f, std::max(-height, 0.15f), -0.4f);
		glm::vec3 moon = glm::vec3(0.30f, 0.40f, 0.70f) * 0.55f;
		float pulse = 0.85f + 0.15f * std::sin(elapsed * 1.7f);
		glm::vec3 blood = glm::vec3(1.0f, 0.12f, 0.06f) * 0.95f * pulse;
		color = glm::mix(moon, blood, dayNight.bloodMoonFade) * glm::smoothstep(-0.02f, 0.15f, -height);
	}
	level.sunDirection = -glm::normalize(toLight); // the level stores the direction light travels
	level.sunColor = color;
	level.ambient = glm::mix(0.05f, 0.30f, day) + twilight * 0.04f;

	glm::vec3 nightSky = glm::mix(glm::vec3(0.02f, 0.03f, 0.07f), glm::vec3(0.16f, 0.015f, 0.02f), dayNight.bloodMoonFade);
	glm::vec3 daySky(0.36f, 0.50f, 0.70f), duskSky(0.45f, 0.20f, 0.12f);
	renderSettings.clearColor = glm::mix(nightSky, daySky, day) + duskSky * twilight * 0.6f;

	// sky backdrop colours
	glm::vec3 nightTop = glm::mix(glm::vec3(0.008f, 0.012f, 0.035f), glm::vec3(0.07f, 0.0f, 0.01f), dayNight.bloodMoonFade);
	renderSettings.skyTop = glm::mix(nightTop, glm::vec3(0.16f, 0.30f, 0.62f), day);
	renderSettings.skyHorizon = glm::mix(nightSky * 1.6f, glm::vec3(0.62f, 0.74f, 0.86f), day) + duskSky * twilight * 0.9f;
	renderSettings.nightAmount = 1.0f - day;
	renderSettings.celestialDir = glm::normalize(toLight);
	renderSettings.celestialColor = color;
}

void IsoMateric::updateWeather(float dt)
{
	const float target = weather.raining ? weather.rainIntensity : 0.0f;
	rain.update(dt, level, camera.focus, target, weather.wind, rng);

	// surfaces get wet within a few seconds and dry out slowly
	float& wet = renderSettings.wetness;
	float rate = target > wet ? 0.6f : 0.08f;
	wet += (target - wet) * (1.0f - std::exp(-dt * rate));
	audio.setLoop("rain", target * 0.8f);

	// lightning: a few rapid flickers, thunder a moment later
	Weather& w = weather;
	if (w.raining && w.lightning)
	{
		w.lightningTimer -= dt;
		if (w.lightningTimer <= 0.0f)
			strikeLightning();
	}
	float flash = 0.0f;
	if (w.flashAge >= 0.0f)
	{
		w.flashAge += dt;
		const float t = w.flashAge;
		// main bolt, a dimmer re-strike, a faint last flicker
		flash = std::exp(-t * 14.0f) + 0.6f * std::exp(-std::max(t - 0.14f, 0.0f) * 18.0f) * (t > 0.14f ? 1.0f : 0.0f) +
		        0.3f * std::exp(-std::max(t - 0.32f, 0.0f) * 10.0f) * (t > 0.32f ? 1.0f : 0.0f);
		if (!w.thunderPlayed && t >= w.thunderDelay)
		{
			audio.play("thunder", 0.9f - 0.25f * w.thunderDelay / 2.5f);
			w.thunderPlayed = true;
		}
		if (t > 3.0f && w.thunderPlayed)
			w.flashAge = -1.0f;
	}
	renderSettings.lightningFlash = std::min(flash, 1.0f);

	renderSettings.time = elapsed;
	renderSettings.wind = glm::vec3(weather.wind.x, 0.0f, weather.wind.y);
	renderSettings.grassPusher = player().feet();
	renderSettings.fogFocus = camera.focus;
	if (weather.fogFollowsSky)
		renderSettings.fogColor = glm::min(renderSettings.clearColor * 1.25f + glm::vec3(0.04f), glm::vec3(1.0f));
}

void IsoMateric::strikeLightning()
{
	std::uniform_real_distribution<float> u(0.0f, 1.0f);
	weather.flashAge = 0.0f;
	weather.thunderDelay = 0.2f + u(rng) * 2.3f; // how far away the bolt was
	weather.thunderPlayed = false;
	// heavier rain, more frequent strikes
	weather.lightningTimer = (5.0f + u(rng) * 14.0f) * (1.4f - weather.rainIntensity);
}

void IsoMateric::updateSky()
{
	RenderSettings& rs = renderSettings;
	rs.sky = level.sky == "galaxy" ? 2 : level.sky == "atmosphere" ? 1 : level.sky == "overcast" ? 3 : 0;
	rs.bloodMoon = dayNight.bloodMoonFade;
	if (!dayNight.running)
	{
		// no cycle: derive the backdrop from the level's own light and clear colour
		const glm::vec3 c = rs.clearColor;
		rs.skyTop = c * 0.5f;
		rs.skyHorizon = c * 1.4f + glm::vec3(0.03f);
		rs.nightAmount = 1.0f - glm::smoothstep(0.08f, 0.3f, glm::dot(c, glm::vec3(0.3f, 0.6f, 0.1f)));
		rs.celestialDir = -glm::normalize(level.sunDirection);
		rs.celestialColor = level.sunColor;
	}
	if (weather.fogFollowsSky)
	{
		if (rs.sky == 2)
			rs.fogColor = glm::vec3(0.06f, 0.04f, 0.11f);
		else if (rs.sky == 1)
			rs.fogColor = rs.skyHorizon;
		else if (rs.sky == 3)
			rs.fogColor = glm::max(rs.skyHorizon * 0.8f, glm::vec3(0.17f, 0.18f, 0.21f)); // grey murk, never black
	}
}

void IsoMateric::generateAsteroids()
{
	asteroids.clear();
	std::mt19937 gen(4242); // same field every time the level loads
	std::uniform_real_distribution<float> u(0.0f, 1.0f);
	const float levelRadius = 0.5f * std::sqrt(static_cast<float>(level.sizeX * level.sizeX + level.sizeZ * level.sizeZ));
	for (int i = 0; i < level.asteroids; i++)
	{
		Asteroid a;
		a.orbitRadius = levelRadius + 3.0f + u(gen) * 9.0f;
		a.angle = u(gen) * glm::two_pi<float>();
		a.speed = (0.02f + u(gen) * 0.05f) * (u(gen) < 0.5f ? 1.0f : -1.0f);
		a.height = -7.0f + u(gen) * 11.0f;
		a.bobPhase = u(gen) * 6.28f;
		int parts = 2 + static_cast<int>(u(gen) * 6.0f);
		float scale = 0.4f + u(gen) * 0.9f;
		for (int p = 0; p < parts; p++)
		{
			glm::vec3 offset = (glm::vec3(u(gen), u(gen), u(gen)) - 0.5f) * scale * 1.2f;
			a.parts.push_back({offset, scale * (0.45f + u(gen) * 0.55f)});
		}
		float g = 0.35f + u(gen) * 0.2f;
		a.color = glm::vec3(g, g * 0.92f, g * 0.85f);
		asteroids.push_back(std::move(a));
	}
}

void IsoMateric::drawLabels()
{
	// names of glass spheres, projected above each one, behind the ImGui windows
	ImDrawList* draw = ImGui::GetBackgroundDrawList();
	const glm::mat4 vp = camera.viewProjection();
	const float viewW = static_cast<float>(windowW) - viewX;
	for (auto [entity, t, g, name] : registry.view<Transform, Glass, Name>().each())
	{
		glm::vec4 clip = vp * glm::vec4(t.position + glm::vec3(0.0f, g.radius + 0.15f, 0.0f), 1.0f);
		if (clip.w <= 0.0f)
			continue;
		glm::vec2 ndc = glm::vec2(clip) / clip.w;
		ImVec2 size = ImGui::CalcTextSize(name.value.c_str());
		ImVec2 pos(viewX + (ndc.x * 0.5f + 0.5f) * viewW - size.x * 0.5f, (0.5f - ndc.y * 0.5f) * static_cast<float>(windowH) - size.y);
		if (pos.x + size.x < viewX)
			continue;
		draw->AddRectFilled(ImVec2(pos.x - 3, pos.y - 1), ImVec2(pos.x + size.x + 3, pos.y + size.y + 1), IM_COL32(0, 0, 0, 140), 3.0f);
		draw->AddText(pos, IM_COL32(255, 255, 255, 230), name.value.c_str());
	}
}

void IsoMateric::drawUi(float frameTime)
{
	fpsSmoothed = fpsSmoothed * 0.95f + (frameTime > 0.0f ? 1.0f / frameTime : 0.0f) * 0.05f;

	// sidebar docked to the left edge: full height, width dragged from its right edge
	const ImVec2 display = ImGui::GetIO().DisplaySize;
	const float maxWidth = std::max(240.0f, std::min(700.0f, display.x - 100.0f));
	ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(sidebarWidth, display.y), ImGuiCond_Always);
	ImGui::SetNextWindowSizeConstraints(ImVec2(240.0f, display.y), ImVec2(maxWidth, display.y));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::Begin("IsoMateric (F2)", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
	ImGui::PopStyleVar();
	sidebarWidth = ImGui::GetWindowWidth();

	ImGui::Text("%.0f fps (%.2f ms) | %d draw calls | %d instances", fpsSmoothed, frameMsSmoothed, renderer.drawCalls, renderer.instancesDrawn);
	if (ImGui::Checkbox("VSync", &vsync))
		glfwSwapInterval(vsync ? 1 : 0);
	ImGui::SameLine();
	const char* qualities[] = {"Very low", "Low", "Medium", "High", "Ultra"};
	ImGui::SetNextItemWidth(110.0f);
	int q = quality;
	if (ImGui::Combo("Graphics", &q, qualities, 5))
		applyQuality(q);
	if (renderer.mirrorsDrawn > 0)
		ImGui::Text("%d mirror reflection%s (each redraws the world)", renderer.mirrorsDrawn, renderer.mirrorsDrawn == 1 ? "" : "s");

	int modeIndex = mode == Mode::Play ? 0 : 1;
	ImGui::Text("Mode (Tab):");
	ImGui::SameLine();
	ImGui::RadioButton("Play", &modeIndex, 0);
	ImGui::SameLine();
	ImGui::RadioButton("Editor", &modeIndex, 1);
	mode = modeIndex == 0 ? Mode::Play : Mode::Editor;

	if (ImGui::CollapsingHeader("Controls"))
	{
		ImGui::TextUnformatted(
			"WASD / arrows   move\n"
			"Space           jump\n"
			"Shift           sprint (hold)\n"
			"Ctrl / C        crouch (hold)\n"
			"Left click      walk there (play) / raise column (editor)\n"
			"Right click     lower column (editor)\n"
			"Middle drag     pan camera\n"
			"Q / E           rotate view\n"
			"V               isometric / first person\n"
			"RMB / MMB drag  look around (first person)\n"
			"Wheel, + / -    zoom\n"
			"T               follow player\n"
			"F / Enter       pick up, confirm\n"
			"I               items (pauses the game)\n"
			"L               flashlight on/off (when equipped)\n"
			"F at a mirror   look into it (F / S / Esc to step back)\n"
			"F2              this window\n"
			"Esc             quit");
	}

	if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
	{
		int camMode = static_cast<int>(camera.mode());
		const char* camModes[] = {"Isometric", "First person"};
		if (ImGui::Combo("View (V)", &camMode, camModes, 2))
			requestCameraMode(static_cast<Camera::Mode>(camMode));
		if (camera.isPerspective())
		{
			ImGui::SliderFloat("Field of view", &camera.fov, 40.0f, 110.0f, "%.0f deg");
			ImGui::SliderFloat("Eye height", &eyeHeight, 0.6f, 2.2f, "%.2f blocks");
			ImGui::TextDisabled("right or middle mouse drag to look");
		}
		int style = camera.style == Camera::Style::Isometric ? 0 : 1;
		const char* styles[] = {"True isometric (35.26 deg)", "2:1 dimetric (30 deg)"};
		if (ImGui::Combo("Projection", &style, styles, 2))
			camera.style = style == 0 ? Camera::Style::Isometric : Camera::Style::Dimetric;
		ImGui::Checkbox("Follow player (T)", &followPlayer);
		ImGui::SliderFloat("Zoom", &camera.zoom, kMinZoom, kMaxZoom, "%.0f px/unit");
		if (ImGui::Button("Rotate left (Q)"))
			camera.rotate(1);
		ImGui::SameLine();
		if (ImGui::Button("Rotate right (E)"))
			camera.rotate(-1);
	}

	if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Checkbox("Shadows", &renderSettings.shadows);
		ImGui::SameLine();
		ImGui::Checkbox("Labels", &showLabels);
		ImGui::SameLine();
		ImGui::Checkbox("See-through walls", &renderSettings.cutaway);
		if (renderSettings.cutaway)
			ImGui::SliderFloat("Window radius", &renderSettings.cutawayRadius, 0.5f, 4.0f, "%.1f blocks");
		ImGui::DragFloat3("Sun direction", &level.sunDirection.x, 0.01f, -1.0f, 1.0f);
		ImGui::ColorEdit3("Sun color", &level.sunColor.x);
		ImGui::SliderFloat("Ambient", &level.ambient, 0.0f, 1.0f);
		ImGui::Checkbox("Hot-reload shaders + script", &autoReload);
		ImGui::SameLine();
		if (ImGui::Button("Reload now"))
		{
			renderer.reloadShaders();
			scripts->load(scripts->path());
		}
	}

	if (ImGui::CollapsingHeader("Day / night", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::Button(dayNight.running ? "Stop day/night cycle" : "Start day/night cycle"))
			setDayNight(!dayNight.running);
		int hh = static_cast<int>(dayNight.hour);
		int mm = static_cast<int>((dayNight.hour - static_cast<float>(hh)) * 60.0f);
		ImGui::SameLine();
		ImGui::Text("%02d:%02d", hh, mm);
		if (ImGui::SliderFloat("Time of day", &dayNight.hour, 0.0f, 24.0f, "%.1f h") && dayNight.running)
			applyDayNight();
		ImGui::SliderFloat("Speed", &dayNight.speed, 0.05f, 6.0f, "%.2f h/s", ImGuiSliderFlags_Logarithmic);
		if (ImGui::Button(dayNight.bloodMoon ? "End blood moon" : "Trigger blood moon"))
		{
			if (dayNight.bloodMoon)
			{
				setBloodMoon(false);
			}
			else
			{
				if (!dayNight.running || (dayNight.hour > 5.0f && dayNight.hour < 19.0f))
					dayNight.hour = 20.5f; // jump to the evening so it is visible
				setDayNight(true);
				setBloodMoon(true);
			}
		}
		if (dayNight.bloodMoon)
		{
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.2f, 1.0f), "the blood moon is up");
		}
		ImGui::SliderFloat("Blood moon chance", &dayNight.bloodMoonChance, 0.0f, 1.0f, "%.2f per night");
		if (!dayNight.running)
			ImGui::TextDisabled("stopping restores the level's own sun");
	}

	if (ImGui::CollapsingHeader("Sky", ImGuiTreeNodeFlags_DefaultOpen))
	{
		int skyMode = level.sky == "galaxy" ? 2 : level.sky == "atmosphere" ? 1 : level.sky == "overcast" ? 3 : 0;
		const char* skies[] = {"Plain colour", "Atmosphere (day/night)", "Galaxy", "Overcast (wind-driven)"};
		if (ImGui::Combo("Backdrop", &skyMode, skies, 4))
			level.sky = skyMode == 3 ? "overcast" : skyMode == 2 ? "galaxy" : skyMode == 1 ? "atmosphere" : "plain";
		if (skyMode != 0)
		{
			ImGui::SliderFloat("Sky brightness", &renderSettings.skyBrightness, 0.2f, 2.0f);
			ImGui::Checkbox("Comets", &renderSettings.comets);
		}
		if (ImGui::SliderInt("Asteroids", &level.asteroids, 0, 120))
			generateAsteroids();
	}

	if (ImGui::CollapsingHeader("Style presets", ImGuiTreeNodeFlags_DefaultOpen))
	{
		// quick ways to try a look; every value stays editable in the sections below
		RenderSettings& rs = renderSettings;
		auto preset = [&](int pixel, int levels, bool dither, int shading, bool lines, bool bloom) {
			rs.pixelSize = pixel;
			rs.colorLevels = levels;
			rs.dither = dither;
			rs.shadingStyle = shading;
			rs.outlines = lines;
			rs.bloom = bloom;
		};
		if (ImGui::Button("Clean 3D"))
			preset(1, 0, false, 0, false, false);
		ImGui::SameLine();
		if (ImGui::Button("Pixel art"))
			preset(4, 16, false, 0, false, false);
		ImGui::SameLine();
		if (ImGui::Button("Retro"))
			preset(5, 5, true, 1, false, false);
		if (ImGui::Button("Cel + ink"))
			preset(1, 0, false, 1, true, false);
		ImGui::SameLine();
		if (ImGui::Button("Pixel cel + ink"))
			preset(4, 0, false, 1, true, false);
		ImGui::SameLine();
		if (ImGui::Button("Moebius"))
			preset(1, 0, false, 2, true, false);
		if (ImGui::Button("Dreamy (bloom)"))
			preset(3, 0, false, 0, false, true);
		ImGui::SameLine();
		if (ImGui::Button("Gothic"))
		{
			// cold, desaturated and murky: overcast sky, withered grass, low mist
			preset(3, 0, false, 0, false, true);
			rs.bloomThreshold = 0.8f;
			rs.bloomIntensity = 0.5f;
			rs.grading = true;
			rs.saturation = 0.55f;
			rs.contrast = 1.15f;
			rs.shadowTint = glm::vec3(0.80f, 0.92f, 1.05f);
			rs.highlightTint = glm::vec3(1.10f, 0.98f, 0.82f);
			rs.vignette = 0.6f;
			rs.grain = 0.06f;
			rs.grassDry = 0.75f;
			rs.lightFlicker = 0.6f;
			rs.godRays = true;
			rs.shadows = true;
			weather.lightning = true;
			rs.fog = true;
			rs.fogDensity = 0.8f;
			rs.fogHeight = 1.2f;
			rs.fogDistance = 0.04f;
			weather.fogFollowsSky = true;
			level.sky = "overcast";
		}
		if (ImGui::Button("Reset grade"))
		{
			rs.grading = false;
			rs.saturation = rs.contrast = 1.0f;
			rs.shadowTint = rs.highlightTint = glm::vec3(1.0f);
			rs.vignette = rs.grain = rs.grassDry = rs.lightFlicker = 0.0f;
			rs.godRays = false;
		}
	}

	if (ImGui::CollapsingHeader("Shading style"))
	{
		const char* styles[] = {"Smooth", "Cel", "Moebius (flat + hatching)"};
		ImGui::Combo("Style", &renderSettings.shadingStyle, styles, 3);
		if (renderSettings.shadingStyle == 2)
			ImGui::SliderFloat("Hatch spacing", &renderSettings.hatchSpacing, 3.0f, 12.0f, "%.0f px");
		ImGui::Checkbox("Ink outlines", &renderSettings.outlines);
		if (renderSettings.outlines)
		{
			ImGui::SliderFloat("Line width", &renderSettings.outlineWidth, 1.0f, 3.0f, "%.0f px");
			ImGui::ColorEdit3("Line colour", &renderSettings.outlineColor.x);
		}
	}

	if (ImGui::CollapsingHeader("Light & atmosphere"))
	{
		ImGui::SliderFloat("Flame flicker", &renderSettings.lightFlicker, 0.0f, 1.0f);
		ImGui::Checkbox("God rays", &renderSettings.godRays);
		if (renderSettings.godRays)
		{
			ImGui::SliderFloat("Ray strength", &renderSettings.godRayStrength, 0.0f, 2.0f);
			ImGui::SliderInt("Ray quality", &renderSettings.godRaySteps, 8, 64, "%d steps");
			if (!renderSettings.shadows)
				ImGui::TextDisabled("needs shadows on for shafts");
		}
	}

	if (ImGui::CollapsingHeader("Colour grade"))
	{
		ImGui::Checkbox("Grading", &renderSettings.grading);
		if (renderSettings.grading)
		{
			ImGui::SliderFloat("Saturation", &renderSettings.saturation, 0.0f, 1.5f);
			ImGui::SliderFloat("Contrast", &renderSettings.contrast, 0.5f, 1.8f);
			ImGui::ColorEdit3("Shadow tint", &renderSettings.shadowTint.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
			ImGui::ColorEdit3("Highlight tint", &renderSettings.highlightTint.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
		}
		ImGui::SliderFloat("Vignette", &renderSettings.vignette, 0.0f, 1.0f);
		ImGui::SliderFloat("Film grain", &renderSettings.grain, 0.0f, 0.25f);
	}

	if (ImGui::CollapsingHeader("Bloom"))
	{
		ImGui::Checkbox("Enabled", &renderSettings.bloom);
		ImGui::SliderFloat("Threshold", &renderSettings.bloomThreshold, 0.2f, 1.0f);
		ImGui::SliderFloat("Strength", &renderSettings.bloomIntensity, 0.0f, 2.0f);
		ImGui::SliderInt("Spread", &renderSettings.bloomPasses, 1, 5);
	}

	if (ImGui::CollapsingHeader("Weather", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat2("Wind", &weather.wind.x, 0.05f, -8.0f, 8.0f);
		ImGui::SameLine();
		if (ImGui::SmallButton("Calm"))
			weather.wind = glm::vec2(0.0f);
		ImGui::Checkbox("Rain", &weather.raining);
		if (weather.raining)
		{
			ImGui::SliderFloat("Intensity", &weather.rainIntensity, 0.05f, 1.0f);
			ImGui::Checkbox("Lightning", &weather.lightning);
			ImGui::SameLine();
			if (ImGui::SmallButton("Strike now"))
				strikeLightning();
		}
		ImGui::Checkbox("Grass", &renderSettings.grass);
		if (renderSettings.grass)
		{
			ImGui::SliderInt("Blades per tile", &renderSettings.grassDensity, 4, 96);
			ImGui::SliderFloat("Blade height", &renderSettings.grassHeight, 0.3f, 2.5f, "%.1fx");
			ImGui::SliderFloat("Withered", &renderSettings.grassDry, 0.0f, 1.0f);
		}
		ImGui::Text("Wetness %.2f  |  %d drops", renderSettings.wetness, static_cast<int>(rain.drops.size()));
		ImGui::Checkbox("Fog / mist", &renderSettings.fog);
		if (renderSettings.fog)
		{
			ImGui::SliderFloat("Mist density", &renderSettings.fogDensity, 0.0f, 3.0f);
			ImGui::SliderFloat("Mist height", &renderSettings.fogHeight, 0.1f, 4.0f, "%.1f blocks");
			ImGui::SliderFloat("Distance haze", &renderSettings.fogDistance, 0.0f, 0.15f, "%.3f");
			ImGui::Checkbox("Fog color follows sky", &weather.fogFollowsSky);
			if (!weather.fogFollowsSky)
				ImGui::ColorEdit3("Fog color", &renderSettings.fogColor.x);
		}
	}

	if (ImGui::CollapsingHeader("Pixelizer", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::SliderInt("Pixel size", &renderSettings.pixelSize, 1, 12, renderSettings.pixelSize == 1 ? "off" : "%d px");
		ImGui::SliderInt("Color levels", &renderSettings.colorLevels, 0, 16, renderSettings.colorLevels == 0 ? "off" : "%d");
		ImGui::Checkbox("Dither", &renderSettings.dither);
		ImGui::SameLine();
		ImGui::Checkbox("Snap camera to pixels", &pixelSnap);
	}

	if (ImGui::CollapsingHeader("Script"))
	{
		ImGui::TextWrapped("%s", scripts->path().c_str());
		ImGui::TextDisabled("set per level with a 'script' line");
		if (scripts->failed())
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "error: %s", scripts->error().c_str());
		else
			ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "running");
		if (ImGui::Button("Reload script"))
			scripts->load(scripts->path());
	}

	if (ImGui::CollapsingHeader("Entities"))
	{
		ImGui::Text("%d alive", static_cast<int>(registry.storage<entt::entity>().free_list()));
		entt::entity toDestroy = entt::null;
		int boxes = 0;
		for (auto [entity, t] : registry.view<Transform>().each())
		{
			if (registry.all_of<Box>(entity))
			{
				boxes++; // scenery from scripts; too many to list one by one
				continue;
			}
			const auto* name = registry.try_get<Name>(entity);
			const auto* emitter = registry.try_get<ParticleEmitter>(entity);
			ImGui::PushID(static_cast<int>(entt::to_integral(entity)));
			ImGui::Text("#%u %s  (%.1f %.1f %.1f)%s", static_cast<unsigned>(entt::to_integral(entity)),
			            name ? name->value.c_str() : (emitter ? "emitter" : "entity"), t.position.x, t.position.y, t.position.z,
			            emitter ? (" " + std::to_string(emitter->particles.size()) + " particles").c_str() : "");
			if (entity != playerEntity)
			{
				ImGui::SameLine();
				if (ImGui::SmallButton("x"))
					toDestroy = entity;
			}
			if (auto* g = registry.try_get<Glass>(entity); g && ImGui::TreeNode("glass"))
			{
				ImGui::SliderFloat("IOR", &g->ior, 1.0f, 2.6f);
				ImGui::SliderFloat("Roughness", &g->roughness, 0.0f, 1.0f);
				ImGui::SliderFloat("Absorption", &g->absorption, 0.0f, 6.0f);
				ImGui::SliderFloat("Dispersion", &g->dispersion, 0.0f, 0.15f);
				ImGui::ColorEdit3("Tint", &g->tint.x);
				ImGui::SliderFloat("Radius", &g->radius, 0.1f, 1.5f);
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		ImGui::TextDisabled("+ %d decorative boxes", boxes);
		if (toDestroy != entt::null)
			registry.destroy(toDestroy);
	}

	if (ImGui::CollapsingHeader("Audio"))
	{
		if (!audio.available())
			ImGui::TextDisabled("no audio device");
		float volume = audio.volume;
		if (ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f))
			audio.setVolume(volume);
	}

	if (ImGui::CollapsingHeader("Profiler"))
	{
#ifdef TRACY_ENABLE
		ImGui::TextDisabled("Tracy enabled: connect the Tracy profiler GUI");
#else
		ImGui::TextDisabled("build with -DISOMATERIC_TRACY=ON for Tracy");
#endif
		for (const CpuProfiler::Entry& e : CpuProfiler::entries())
			ImGui::Text("%-12s %6.3f ms", e.name, e.ms);
	}

	if (ImGui::CollapsingHeader("Point lights"))
	{
		int removeIndex = -1;
		for (int i = 0; i < static_cast<int>(level.lights.size()); i++)
		{
			PointLight& l = level.lights[i];
			ImGui::PushID(i);
			ImGui::Text("Light %d", i);
			ImGui::DragFloat3("Position", &l.position.x, 0.05f);
			ImGui::ColorEdit3("Color", &l.color.x);
			ImGui::SliderFloat("Intensity", &l.intensity, 0.0f, 4.0f);
			ImGui::SliderFloat("Radius", &l.radius, 0.5f, 30.0f);
			if (ImGui::Button("Delete"))
				removeIndex = i;
			ImGui::Separator();
			ImGui::PopID();
		}
		// erase after the loop so no element is skipped while iterating
		if (removeIndex >= 0)
			level.lights.erase(level.lights.begin() + removeIndex);

		if (static_cast<int>(level.lights.size()) < Level::kMaxLights && ImGui::Button("Add light at player"))
		{
			PointLight l;
			l.position = player().position + glm::vec3(0.0f, 1.5f, 0.0f);
			level.lights.push_back(l);
		}
	}

	if (ImGui::CollapsingHeader("Level", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::InputText("File", levelPathBuffer, sizeof(levelPathBuffer));
		if (ImGui::Button("Save"))
		{
			// save the level's own sun, not the cycle's current one
			bool cycling = dayNight.running;
			float hour = dayNight.hour;
			setDayNight(false);
			level.save(levelPathBuffer);
			dayNight.hour = hour;
			setDayNight(cycling);
		}
		ImGui::SameLine();
		if (ImGui::Button("Load"))
			loadLevel(levelPathBuffer);
		ImGui::SameLine();
		if (ImGui::Button("Spawn = player tile"))
			level.spawn = player().tile();
		ImGui::InputInt2("New size", newLevelSize);
		if (ImGui::Button("New flat level"))
		{
			Level fresh = Level::flat(std::clamp(newLevelSize[0], 1, 256), std::clamp(newLevelSize[1], 1, 256));
			fresh.sunDirection = level.sunDirection;
			fresh.sunColor = level.sunColor;
			fresh.ambient = level.ambient;
			bool cycling = dayNight.running;
			setDayNight(false);
			level = std::move(fresh);
			setDayNight(cycling);
			generateAsteroids();
			scripts->load(kAssets + "/" + level.script);
			resetPlayer();
		}
	}

	ImGui::Separator();
	const Player& p = player();
	ImGui::Text("Player  %.2f %.2f %.2f  tile %d,%d  %s", p.position.x, p.position.y, p.position.z, p.tile().x, p.tile().y,
	            p.grounded ? "grounded" : "airborne");
	if (hover.valid)
		ImGui::Text("Hover   block %d,%d,%d", hover.block.x, hover.block.y, hover.block.z);

	ImGui::End();
}
