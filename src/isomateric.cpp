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

	glm::vec3 blockColor(int x, int y, int z)
	{
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
	ctx.assetsDir = kAssets;
	scripts = std::make_unique<ScriptHost>(ctx);
	scripts->load(kAssets + "/scripts/game.lua");
	return true;
}

void IsoMateric::createPlayer()
{
	playerEntity = registry.create();
	registry.emplace<Transform>(playerEntity);
	registry.emplace<Player>(playerEntity);
	registry.emplace<Name>(playerEntity, Name{"player"});
	if (auto model = models.get(kAssets + "/" + kPlayerModel))
	{
		registry.emplace<MeshRenderer>(playerEntity, MeshRenderer{model, 1.1f, kPlayerModelYaw, glm::vec4(1.0f)});
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
	level = std::move(loaded);
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
		elapsed += dt;

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

		{
			PROFILE_SCOPE("physics");
			accumulator += frameTime;
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
			scripts->update(dt);
		}

		{
			PROFILE_SCOPE("animation");
			syncPlayerEntity(dt);
			updateAnimation(dt);
		}
		{
			PROFILE_SCOPE("particles");
			updateParticles(registry, dt, rng);
		}

		// camera after movement and collision, so it never trails a frame behind
		if (followPlayer)
			camera.focusGoal = player().position;
		camera.update(dt);
		camera.snapUnits = pixelSnap && renderSettings.pixelSize > 1 ? static_cast<float>(renderSettings.pixelSize) / camera.zoom : 0.0f;

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

	// zoom: keys are continuous and frame-rate independent, the wheel steps
	float zoomDir = (in.down(Action::ZoomIn) ? 1.0f : 0.0f) - (in.down(Action::ZoomOut) ? 1.0f : 0.0f);
	camera.zoom *= std::exp(zoomDir * dt * 2.0f);
	if (!in.mouseBlocked && in.scroll != 0.0f)
		camera.zoom *= std::pow(1.15f, in.scroll);
	camera.zoom = std::clamp(camera.zoom, kMinZoom, kMaxZoom);

	// middle mouse drag pans the camera
	if (in.mouseDown(GLFW_MOUSE_BUTTON_MIDDLE) && (in.mouseDeltaX != 0.0 || in.mouseDeltaY != 0.0))
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

void IsoMateric::fixedUpdate(float dt)
{
	const Input& in = inputState;
	glm::vec3 f = camera.forward(), r = camera.right();
	float fwd = (in.down(Action::MoveForward) ? 1.0f : 0.0f) - (in.down(Action::MoveBack) ? 1.0f : 0.0f);
	float side = (in.down(Action::MoveRight) ? 1.0f : 0.0f) - (in.down(Action::MoveLeft) ? 1.0f : 0.0f);
	glm::vec2 move(f.x * fwd + r.x * side, f.z * fwd + r.z * side);
	if (glm::dot(move, move) > 0.0f)
		move = glm::normalize(move); // diagonal input is not faster
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

	const int maxH = std::max(level.maxHeight(), 2);
	frame.boundsMin = glm::vec3(-0.5f);
	frame.boundsMax = glm::vec3(static_cast<float>(level.sizeX) - 0.5f, static_cast<float>(maxH) - 0.5f,
	                            static_cast<float>(level.sizeZ) - 0.5f);

	for (int x = 0; x < level.sizeX; x++)
	{
		for (int z = 0; z < level.sizeZ; z++)
		{
			// floor block plus one tall box for everything stacked on it: no hidden faces
			// between stacked cubes, which would otherwise flicker through shared edges
			const int h = level.height(x, z);
			const float fx = static_cast<float>(x), fz = static_cast<float>(z);
			if (h >= 1)
				frame.opaque.push_back({glm::vec3(fx, 0.0f, fz), glm::vec3(1.0f), glm::vec4(blockColor(x, 0, z), 1.0f)});
			if (h >= 2)
				frame.opaque.push_back({glm::vec3(fx, static_cast<float>(h) * 0.5f, fz), glm::vec3(1.0f, static_cast<float>(h - 1), 1.0f),
				                        glm::vec4(blockColor(x, h - 1, z), 1.0f)});
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
		const Model& m = *mr.model;
		float h = m.boundsMax.y - m.boundsMin.y;
		float scale = h > 0.0f ? mr.height / h : 1.0f;
		glm::vec3 c = (m.boundsMin + m.boundsMax) * 0.5f;
		glm::mat4 xf = glm::translate(glm::mat4(1.0f), t.position) *
		               glm::rotate(glm::mat4(1.0f), t.yaw + mr.yawOffset, glm::vec3(0.0f, 1.0f, 0.0f)) *
		               glm::scale(glm::mat4(1.0f), glm::vec3(scale)) * glm::translate(glm::mat4(1.0f), glm::vec3(-c.x, -m.boundsMin.y, -c.z));
		frame.meshes.push_back({&m, xf, &anim.globals, &anim.joints, mr.tint});
	}
	if (!registry.all_of<MeshRenderer>(playerEntity))
		frame.opaque.push_back({player().position, player().size, glm::vec4(0.9f, 0.2f, 0.2f, 1.0f)});

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

	ImGui::Text("%.0f fps | %d draw calls | %d instances", fpsSmoothed, renderer.drawCalls, renderer.instancesDrawn);

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
			"Left click      walk there (play) / raise column (editor)\n"
			"Right click     lower column (editor)\n"
			"Middle drag     pan camera\n"
			"Q / E           rotate view\n"
			"Wheel, + / -    zoom\n"
			"T               follow player\n"
			"F2              this window\n"
			"Esc             quit");
	}

	if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
	{
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
		for (auto [entity, t] : registry.view<Transform>().each())
		{
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
			ImGui::PopID();
		}
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
			level.save(levelPathBuffer);
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
			level = std::move(fresh);
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
