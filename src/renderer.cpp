#include "renderer.h"
#include "log.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>

namespace
{
	const std::string kShaderDir = std::string(ISOMATERIC_ASSETS_DIR) + "/shaders/";

	// std140 layout of the "Lights" uniform block in world.fs
	struct LightBlock
	{
		glm::vec4 positionRadius[Level::kMaxLights];
		glm::vec4 colorIntensity[Level::kMaxLights];
	};

	// Unit cube, 36 vertices (position + normal), counter-clockwise when seen from outside,
	// so back-face culling works. Each face is built from its normal n and two tangents
	// u, v with u x v = n.
	std::vector<float> buildCube()
	{
		std::vector<float> v;
		v.reserve(36 * 6);
		for (int axis = 0; axis < 3; axis++)
		{
			for (int sign = -1; sign <= 1; sign += 2)
			{
				glm::vec3 n(0.0f), u(0.0f), t(0.0f);
				n[axis] = static_cast<float>(sign);
				u[(axis + 1) % 3] = 1.0f;
				t[(axis + 2) % 3] = 1.0f;
				if (sign < 0)
					std::swap(u, t);
				glm::vec3 c = n * 0.5f;
				u *= 0.5f;
				t *= 0.5f;
				const glm::vec3 corners[6] = {c - u - t, c + u - t, c + u + t, c - u - t, c + u + t, c - u + t};
				for (const glm::vec3& p : corners)
				{
					v.insert(v.end(), {p.x, p.y, p.z, n.x, n.y, n.z});
				}
			}
		}
		return v;
	}
}

Renderer::~Renderer()
{
	glDeleteVertexArrays(1, &cubeVao);
	glDeleteBuffers(1, &cubeVbo);
	glDeleteBuffers(1, &instanceVbo);
	glDeleteBuffers(1, &lightUbo);
	glDeleteFramebuffers(1, &shadowFbo);
	glDeleteTextures(1, &shadowTex);
	glDeleteFramebuffers(1, &sceneFbo);
	glDeleteTextures(1, &sceneColor);
	glDeleteRenderbuffers(1, &sceneDepth);
	glDeleteVertexArrays(1, &emptyVao);
}

void Renderer::ensureSceneTarget(int w, int h)
{
	if (sceneFbo && w == sceneW && h == sceneH)
		return;
	sceneW = w;
	sceneH = h;
	if (!sceneFbo)
	{
		glGenFramebuffers(1, &sceneFbo);
		glGenTextures(1, &sceneColor);
		glGenRenderbuffers(1, &sceneDepth);
	}
	glBindTexture(GL_TEXTURE_2D, sceneColor);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindRenderbuffer(GL_RENDERBUFFER, sceneDepth);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
	glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneColor, 0);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, sceneDepth);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		LOG_ERROR("renderer: pixelizer framebuffer is incomplete (%dx%d)", w, h);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool Renderer::init()
{
	world = Shader(kShaderDir + "world.vs", kShaderDir + "world.fs");
	depth = Shader(kShaderDir + "depth.vs", kShaderDir + "depth.fs");
	unlit = Shader(kShaderDir + "unlit.vs", kShaderDir + "unlit.fs");
	mesh = Shader(kShaderDir + "mesh.vs", kShaderDir + "mesh.fs");
	meshDepth = Shader(kShaderDir + "mesh_depth.vs", kShaderDir + "depth.fs");
	post = Shader(kShaderDir + "post.vs", kShaderDir + "post.fs");
	glGenVertexArrays(1, &emptyVao);
	if (!reloadShaders())
		return false;

	// cube geometry + per-instance attributes
	std::vector<float> cube = buildCube();
	glGenVertexArrays(1, &cubeVao);
	glGenBuffers(1, &cubeVbo);
	glGenBuffers(1, &instanceVbo);

	glBindVertexArray(cubeVao);
	glBindBuffer(GL_ARRAY_BUFFER, cubeVbo);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(cube.size() * sizeof(float)), cube.data(), GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(0));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));

	glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Instance), reinterpret_cast<void*>(offsetof(Instance, offset)));
	glVertexAttribDivisor(2, 1);
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Instance), reinterpret_cast<void*>(offsetof(Instance, scale)));
	glVertexAttribDivisor(3, 1);
	glEnableVertexAttribArray(4);
	glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Instance), reinterpret_cast<void*>(offsetof(Instance, color)));
	glVertexAttribDivisor(4, 1);
	glBindVertexArray(0);

	// point lights uniform buffer
	glGenBuffers(1, &lightUbo);
	glBindBuffer(GL_UNIFORM_BUFFER, lightUbo);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(LightBlock), nullptr, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER, 0, lightUbo);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	// directional shadow map
	glGenTextures(1, &shadowTex);
	glBindTexture(GL_TEXTURE_2D, shadowTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, kShadowSize, kShadowSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	const float border[] = {1.0f, 1.0f, 1.0f, 1.0f};
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);

	glGenFramebuffers(1, &shadowFbo);
	glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTex, 0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	if (!complete)
	{
		LOG_ERROR("renderer: shadow framebuffer is incomplete");
		return false;
	}
	return true;
}

bool Renderer::reloadShaders()
{
	bool ok = world.load() & depth.load() & unlit.load() & mesh.load() & meshDepth.load() & post.load();
	world.bindBlock("Lights", 0);
	mesh.bindBlock("Lights", 0);
	return ok && world.valid() && depth.valid() && unlit.valid() && mesh.valid() && meshDepth.valid() && post.valid();
}

void Renderer::reloadShadersIfChanged()
{
	if (world.reloadIfChanged())
		world.bindBlock("Lights", 0);
	if (mesh.reloadIfChanged())
		mesh.bindBlock("Lights", 0);
	depth.reloadIfChanged();
	unlit.reloadIfChanged();
	meshDepth.reloadIfChanged();
	post.reloadIfChanged();
}

void Renderer::drawMesh(Shader& shader, const MeshDraw& d, bool materials)
{
	for (const Model::Primitive& p : d.model->primitives)
	{
		if (p.skinned && d.joints && !d.joints->empty())
		{
			shader.set("skinned", 1);
			shader.set("joints", *d.joints);
			shader.set("model", d.transform);
		}
		else
		{
			shader.set("skinned", 0);
			glm::mat4 node = d.globals && p.node >= 0 && p.node < static_cast<int>(d.globals->size()) ? (*d.globals)[p.node] : glm::mat4(1.0f);
			shader.set("model", d.transform * node);
		}
		if (materials)
		{
			const Model::Material* m = p.material >= 0 ? &d.model->materials[p.material] : nullptr;
			shader.set("baseColor", (m ? m->baseColor : glm::vec4(1.0f)) * d.tint);
			bool textured = m && m->texture >= 0;
			shader.set("hasTexture", textured ? 1 : 0);
			if (textured)
				d.model->textures[m->texture].bind(1);
		}
		glBindVertexArray(p.vao);
		glDrawElements(GL_TRIANGLES, p.indexCount, GL_UNSIGNED_INT, nullptr);
		drawCalls++;
	}
}

void Renderer::setLighting(Shader& shader, const Camera& camera, const Level& level, const glm::mat4& lightSpace, int lightCount,
                           const RenderSettings& settings)
{
	shader.set("view", camera.view());
	shader.set("projection", camera.projection());
	shader.set("lightSpace", lightSpace);
	shader.set("viewDir", camera.toCamera());
	shader.set("sunDir", glm::normalize(level.sunDirection));
	shader.set("sunColor", level.sunColor);
	shader.set("ambient", level.ambient);
	shader.set("numLights", lightCount);
	shader.set("shadowsEnabled", settings.shadows ? 1 : 0);
	shader.set("shadowMap", 0);
	shader.set("shadowNormalOffset", shadowTexelWorld * 1.5f);
}

void Renderer::resize(int w, int h)
{
	width = std::max(1, w);
	height = std::max(1, h);
	setViewRect(viewX, viewW);
}

void Renderer::setViewRect(int x, int w)
{
	viewX = std::clamp(x, 0, width - 1);
	viewW = std::clamp(w, 1, width - viewX);
}

void Renderer::draw(const std::vector<Instance>& instances)
{
	if (instances.empty())
		return;
	glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
	// orphan + upload: lets the driver hand out fresh storage instead of stalling
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(instances.size() * sizeof(Instance)), nullptr, GL_STREAM_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(instances.size() * sizeof(Instance)), instances.data());
	glBindVertexArray(cubeVao);
	glDrawArraysInstanced(GL_TRIANGLES, 0, 36, static_cast<GLsizei>(instances.size()));
	drawCalls++;
	instancesDrawn += static_cast<int>(instances.size());
}

glm::mat4 Renderer::sunMatrix(const Level& level, const FrameData& frame)
{
	glm::vec3 center = (frame.boundsMin + frame.boundsMax) * 0.5f;
	float radius = glm::length(frame.boundsMax - frame.boundsMin) * 0.5f + 1.0f;
	shadowTexelWorld = 2.0f * radius / static_cast<float>(kShadowSize);
	glm::vec3 dir = glm::normalize(level.sunDirection);
	glm::vec3 up = std::abs(dir.y) > 0.99f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
	glm::mat4 view = glm::lookAt(center - dir * radius * 2.0f, center, up);
	glm::mat4 proj = glm::ortho(-radius, radius, -radius, radius, 0.1f, radius * 4.0f);
	return proj * view;
}

void Renderer::render(const Camera& camera, const Level& level, FrameData& frame, const RenderSettings& settings)
{
	drawCalls = 0;
	instancesDrawn = 0;

	const glm::mat4 lightSpace = sunMatrix(level, frame);
	const glm::mat4 view = camera.view();
	const glm::mat4 projection = camera.projection();

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_CULL_FACE);
	glEnable(GL_MULTISAMPLE);

	// 1. shadow pass
	if (settings.shadows)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
		glViewport(0, 0, kShadowSize, kShadowSize);
		glClear(GL_DEPTH_BUFFER_BIT);
		// front faces into the map; acne is handled by the normal offset in the shaders
		// (culling front faces instead leaks light under walls where they meet the floor)
		depth.use();
		depth.set("lightSpace", lightSpace);
		draw(frame.opaque);
		draw(frame.transparent);
		if (!frame.meshes.empty())
		{
			glDisable(GL_CULL_FACE); // character meshes are not always closed
			meshDepth.use();
			meshDepth.set("lightSpace", lightSpace);
			for (const MeshDraw& m : frame.meshes)
				drawMesh(meshDepth, m, false);
			glEnable(GL_CULL_FACE);
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	// pixelizer: draw the scene into a low-res target, upscale at the end
	const bool pixelate = settings.pixelSize > 1;
	if (pixelate)
	{
		ensureSceneTarget(std::max(1, viewW / settings.pixelSize), std::max(1, height / settings.pixelSize));
		glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo);
		glViewport(0, 0, sceneW, sceneH);
	}
	else
	{
		glViewport(viewX, 0, viewW, height);
	}
	glClearColor(settings.clearColor.r, settings.clearColor.g, settings.clearColor.b, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// 2. lights uniform block
	LightBlock block{};
	int lightCount = std::min(static_cast<int>(level.lights.size()), Level::kMaxLights);
	for (int i = 0; i < lightCount; i++)
	{
		const PointLight& l = level.lights[i];
		block.positionRadius[i] = glm::vec4(l.position, std::max(l.radius, 0.01f));
		block.colorIntensity[i] = glm::vec4(l.color, l.intensity);
	}
	glBindBuffer(GL_UNIFORM_BUFFER, lightUbo);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(LightBlock), &block);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	// 3. opaque world
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, shadowTex);
	world.use();
	setLighting(world, camera, level, lightSpace, lightCount, settings);
	world.set("cutaway", settings.cutaway ? 1 : 0);
	world.set("cutawayCenter", settings.cutawayCenter);
	world.set("cutawayRadius", settings.cutawayRadius);
	world.set("cutawayFloor", settings.cutawayFloor);
	draw(frame.opaque);

	// 3b. skinned / static meshes
	if (!frame.meshes.empty())
	{
		mesh.use();
		setLighting(mesh, camera, level, lightSpace, lightCount, settings);
		mesh.set("albedo", 1);
		for (const MeshDraw& m : frame.meshes)
			drawMesh(mesh, m, true);
		glActiveTexture(GL_TEXTURE0);
	}

	// 4. transparent world (faded occluders), far to near
	if (!frame.transparent.empty())
	{
		const glm::vec3 toCam = camera.toCamera();
		std::sort(frame.transparent.begin(), frame.transparent.end(), [&](const Instance& a, const Instance& b) {
			return glm::dot(a.offset, toCam) < glm::dot(b.offset, toCam);
		});
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask(GL_FALSE);
		draw(frame.transparent);
		glDepthMask(GL_TRUE);
		glDisable(GL_BLEND);
	}

	// 5. unlit overlay
	if (!frame.overlay.empty())
	{
		unlit.use();
		unlit.set("view", view);
		unlit.set("projection", projection);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask(GL_FALSE);
		draw(frame.overlay);
		glDepthMask(GL_TRUE);
		glDisable(GL_BLEND);
	}

	// 6. particles: alpha-blended (sorted) then additive (order independent)
	if (!frame.particlesAlpha.empty() || !frame.particlesAdditive.empty())
	{
		const glm::vec3 toCam = camera.toCamera();
		std::sort(frame.particlesAlpha.begin(), frame.particlesAlpha.end(), [&](const Instance& a, const Instance& b) {
			return glm::dot(a.offset, toCam) < glm::dot(b.offset, toCam);
		});
		unlit.use();
		unlit.set("view", view);
		unlit.set("projection", projection);
		glEnable(GL_BLEND);
		glDepthMask(GL_FALSE);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		draw(frame.particlesAlpha);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		draw(frame.particlesAdditive);
		glDepthMask(GL_TRUE);
		glDisable(GL_BLEND);
	}

	// 7. pixelizer upscale + posterize
	if (pixelate)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(viewX, 0, viewW, height);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		post.use();
		post.set("scene", 0);
		post.set("colorLevels", settings.colorLevels);
		post.set("dither", settings.dither ? 1 : 0);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, sceneColor);
		glBindVertexArray(emptyVao);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		drawCalls++;
		glEnable(GL_DEPTH_TEST);
	}

	glBindVertexArray(0);
}
