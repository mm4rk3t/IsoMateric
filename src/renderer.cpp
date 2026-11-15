#include "renderer.h"
#include "log.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <random>
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

	// one grass blade, as stored in the instance buffer (matches grass.vs)
	struct GrassBlade
	{
		glm::vec3 base;
		glm::vec4 shape; // height, half width, facing angle, phase
		glm::vec3 color;
	};
	constexpr int kBladeSegments = 4;
	constexpr GLsizei kBladeVertices = kBladeSegments * 2 + 1; // triangle strip ending in the tip

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
	glDeleteTextures(1, &sceneDepth);
	glDeleteTextures(1, &sceneNormal);
	glDeleteVertexArrays(1, &emptyVao);
	glDeleteVertexArrays(1, &sphereVao);
	glDeleteBuffers(1, &sphereVbo);
	glDeleteBuffers(1, &sphereEbo);
	glDeleteFramebuffers(1, &grabFbo);
	glDeleteTextures(1, &grabTex);
	glDeleteVertexArrays(1, &grassVao);
	glDeleteBuffers(1, &grassMeshVbo);
	glDeleteBuffers(1, &grassInstanceVbo);
	glDeleteFramebuffers(kMaxMirrors, mirrorFbo);
	glDeleteTextures(kMaxMirrors, mirrorTex);
	glDeleteRenderbuffers(kMaxMirrors, mirrorDepth);
	glDeleteFramebuffers(2, bloomFbo);
	glDeleteTextures(2, bloomTex);
}

void Renderer::drawSky(const Camera& camera, const RenderSettings& s, int originX, int w, int h, const glm::vec3* mirrorNormal)
{
	// in a mirror every view direction is reflected; the shader rebuilds its up/right
	// vectors with cross products, which flip sign under a reflection, so it is told
	auto m = [&](const glm::vec3& d) { return mirrorNormal ? glm::reflect(d, *mirrorNormal) : d; };
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	sky.use();
	sky.set("mode", s.sky);
	sky.set("viewOrigin", glm::vec3(static_cast<float>(originX), 0.0f, 0.0f));
	sky.set("viewSize", glm::vec3(static_cast<float>(w), static_cast<float>(h), 0.0f));
	sky.set("camForward", m(camera.forward()));
	sky.set("camRight", m(camera.right()));
	sky.set("mirrored", mirrorNormal ? 1 : 0);
	sky.set("time", s.time);
	sky.set("focus", camera.focus);
	sky.set("brightness", s.skyBrightness * (1.0f + s.lightningFlash * 1.5f));
	sky.set("skyTop", s.skyTop);
	sky.set("skyHorizon", s.skyHorizon);
	sky.set("nightAmount", s.nightAmount);
	sky.set("bloodMoon", s.bloodMoon);
	sky.set("celestialDir", s.celestialDir);
	sky.set("celestialColor", s.celestialColor);
	sky.set("comets", s.comets ? 1 : 0);
	sky.set("realView", camera.isPerspective() ? 1 : 0);
	const glm::vec3 viewForward = -camera.toCamera();
	const glm::vec3 viewRight = glm::normalize(glm::cross(viewForward, glm::vec3(0.0f, 1.0f, 0.0f)));
	sky.set("viewForward", m(viewForward));
	sky.set("viewUp", m(glm::cross(viewRight, viewForward)));
	sky.set("tanHalfFov", std::tan(camera.fieldOfView() * 0.5f));
	sky.set("wind", s.wind);
	glBindVertexArray(emptyVao);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	drawCalls++;
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
}

void Renderer::applyBloom(const RenderSettings& s, bool pixelate)
{
	grabFrame(pixelate); // resolved copy of the scene with mipmaps

	const int w = std::max(1, grabW / 4), h = std::max(1, grabH / 4);
	if (!bloomFbo[0] || w != bloomW || h != bloomH)
	{
		if (!bloomFbo[0])
		{
			glGenFramebuffers(2, bloomFbo);
			glGenTextures(2, bloomTex);
		}
		bloomW = w;
		bloomH = h;
		for (int i = 0; i < 2; i++)
		{
			glBindTexture(GL_TEXTURE_2D, bloomTex[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindFramebuffer(GL_FRAMEBUFFER, bloomFbo[i]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bloomTex[i], 0);
		}
	}

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glBindVertexArray(emptyVao);
	glViewport(0, 0, w, h);

	// bright pass into [0]
	glBindFramebuffer(GL_FRAMEBUFFER, bloomFbo[0]);
	bloomExtract.use();
	bloomExtract.set("source", 0);
	bloomExtract.set("threshold", s.bloomThreshold);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, grabTex);
	glDrawArrays(GL_TRIANGLES, 0, 3);

	// blur: [0] -> [1] horizontally, [1] -> [0] vertically
	bloomBlur.use();
	bloomBlur.set("source", 0);
	for (int pass = 0; pass < std::max(1, s.bloomPasses); pass++)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, bloomFbo[1]);
		bloomBlur.set("direction", glm::vec3(1.0f / static_cast<float>(w), 0.0f, 0.0f));
		glBindTexture(GL_TEXTURE_2D, bloomTex[0]);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		glBindFramebuffer(GL_FRAMEBUFFER, bloomFbo[0]);
		bloomBlur.set("direction", glm::vec3(0.0f, 1.0f / static_cast<float>(h), 0.0f));
		glBindTexture(GL_TEXTURE_2D, bloomTex[1]);
		glDrawArrays(GL_TRIANGLES, 0, 3);
	}

	// add back onto the scene
	glBindFramebuffer(GL_FRAMEBUFFER, pixelate ? sceneFbo : 0);
	if (pixelate)
		glViewport(0, 0, sceneW, sceneH);
	else
		glViewport(viewX, 0, viewW, height);
	bloomComposite.use();
	bloomComposite.set("bloom", 0);
	bloomComposite.set("intensity", s.bloomIntensity);
	bloomComposite.set("viewOrigin", glm::vec3(static_cast<float>(pixelate ? 0 : viewX), 0.0f, 0.0f));
	bloomComposite.set("viewSize", glm::vec3(static_cast<float>(grabW), static_cast<float>(grabH), 0.0f));
	glBindTexture(GL_TEXTURE_2D, bloomTex[0]);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glDisable(GL_BLEND);
	drawCalls += 3 + 2 * s.bloomPasses;

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glBindVertexArray(0);
}

void Renderer::buildSphere()
{
	// unit UV sphere: position doubles as the normal
	const int rings = 24, segments = 48;
	std::vector<float> v;
	std::vector<unsigned int> idx;
	for (int r = 0; r <= rings; r++)
	{
		float phi = glm::pi<float>() * static_cast<float>(r) / rings;
		for (int sgm = 0; sgm <= segments; sgm++)
		{
			float theta = glm::two_pi<float>() * static_cast<float>(sgm) / segments;
			v.insert(v.end(), {std::sin(phi) * std::cos(theta), std::cos(phi), std::sin(phi) * std::sin(theta)});
		}
	}
	for (int r = 0; r < rings; r++)
		for (int sgm = 0; sgm < segments; sgm++)
		{
			unsigned int a = static_cast<unsigned int>(r * (segments + 1) + sgm), b = a + segments + 1;
			idx.insert(idx.end(), {a, a + 1, b, b, a + 1, b + 1}); // counter-clockwise from outside
		}
	sphereIndexCount = static_cast<GLsizei>(idx.size());
	glGenVertexArrays(1, &sphereVao);
	glGenBuffers(1, &sphereVbo);
	glGenBuffers(1, &sphereEbo);
	glBindVertexArray(sphereVao);
	glBindBuffer(GL_ARRAY_BUFFER, sphereVbo);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(v.size() * sizeof(float)), v.data(), GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphereEbo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(idx.size() * sizeof(unsigned int)), idx.data(), GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
	glBindVertexArray(0);
}

void Renderer::grabFrame(bool pixelate)
{
	// copy (and resolve MSAA of) the frame drawn so far into a mipmapped texture
	const int srcX = pixelate ? 0 : viewX;
	const int w = pixelate ? sceneW : viewW;
	const int h = pixelate ? sceneH : height;
	const GLuint target = pixelate ? sceneFbo : 0;
	if (!grabFbo || w != grabW || h != grabH)
	{
		if (!grabFbo)
		{
			glGenFramebuffers(1, &grabFbo);
			glGenTextures(1, &grabTex);
		}
		grabW = w;
		grabH = h;
		glBindTexture(GL_TEXTURE_2D, grabTex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glGenerateMipmap(GL_TEXTURE_2D);
		glBindFramebuffer(GL_FRAMEBUFFER, grabFbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, grabTex, 0);
	}
	glBindFramebuffer(GL_READ_FRAMEBUFFER, target);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, grabFbo);
	glBlitFramebuffer(srcX, 0, srcX + w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_FRAMEBUFFER, target);
	glBindTexture(GL_TEXTURE_2D, grabTex);
	glGenerateMipmap(GL_TEXTURE_2D); // rough glass samples blurrier mips
}

void Renderer::drawGlassSphere(const Camera& camera, const Level& level, const GlassDraw& g, int lightCount, bool pixelate,
                               const RenderSettings& settings)
{
	grabFrame(pixelate); // per sphere, so a nearer sphere refracts the farther ones

	glass.use();
	glass.set("view", camera.view());
	glass.set("projection", camera.projection());
	glass.set("viewDir", camera.toCamera());
	glass.set("cameraPos", camera.position());
	glass.set("perspective", camera.isPerspective() ? 1 : 0);
	glass.set("sunDir", glm::normalize(level.sunDirection));
	glass.set("sunColor", flashedSun(level, settings));
	glass.set("ambient", level.ambient + settings.lightningFlash * 0.35f);
	glass.set("numLights", lightCount);
	glass.set("grab", 2);
	glass.set("viewOrigin", glm::vec3(static_cast<float>(pixelate ? 0 : viewX), 0.0f, 0.0f));
	glass.set("viewSize", glm::vec3(static_cast<float>(grabW), static_cast<float>(grabH), 0.0f));
	glass.set("center", g.center);
	glass.set("radius", g.radius);
	glass.set("tint", g.tint);
	glass.set("ior", std::max(g.ior, 1.0f));
	glass.set("roughness", glm::clamp(g.roughness, 0.0f, 1.0f));
	glass.set("absorption", std::max(g.absorption, 0.0f));
	glass.set("dispersion", g.dispersion);
	setWeather(glass, settings);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, grabTex);
	glActiveTexture(GL_TEXTURE0);
	glBindVertexArray(sphereVao);
	glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, nullptr);
	glBindVertexArray(0);
	drawCalls++;
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
		glGenTextures(1, &sceneDepth);
		glGenTextures(1, &sceneNormal);
	}
	glBindTexture(GL_TEXTURE_2D, sceneColor);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	// depth as a texture so the outline pass can read it
	glBindTexture(GL_TEXTURE_2D, sceneDepth);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneColor, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, sceneDepth, 0);
	// world-space normals of opaque geometry (alpha 1 = geometry), read by the outline pass
	glBindTexture(GL_TEXTURE_2D, sceneNormal);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, sceneNormal, 0);
	const GLenum buffers[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
	glDrawBuffers(2, buffers);
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
	glass = Shader(kShaderDir + "glass.vs", kShaderDir + "glass.fs");
	sky = Shader(kShaderDir + "post.vs", kShaderDir + "sky.fs");
	bloomExtract = Shader(kShaderDir + "post.vs", kShaderDir + "bloom_extract.fs");
	bloomBlur = Shader(kShaderDir + "post.vs", kShaderDir + "bloom_blur.fs");
	bloomComposite = Shader(kShaderDir + "post.vs", kShaderDir + "bloom_composite.fs");
	grass = Shader(kShaderDir + "grass.vs", kShaderDir + "world.fs");
	mirror = Shader(kShaderDir + "mirror.vs", kShaderDir + "mirror.fs");
	buildSphere();
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
	glEnableVertexAttribArray(5);
	glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(Instance), reinterpret_cast<void*>(offsetof(Instance, emissive)));
	glVertexAttribDivisor(5, 1);
	glBindVertexArray(0);

	// grass: one blade strip, instanced per blade
	{
		std::vector<float> strip;
		for (int i = 0; i < kBladeSegments; i++)
		{
			float t = static_cast<float>(i) / kBladeSegments;
			strip.insert(strip.end(), {-1.0f, t, 1.0f, t});
		}
		strip.insert(strip.end(), {0.0f, 1.0f});
		glGenVertexArrays(1, &grassVao);
		glGenBuffers(1, &grassMeshVbo);
		glGenBuffers(1, &grassInstanceVbo);
		glBindVertexArray(grassVao);
		glBindBuffer(GL_ARRAY_BUFFER, grassMeshVbo);
		glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(strip.size() * sizeof(float)), strip.data(), GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
		glBindBuffer(GL_ARRAY_BUFFER, grassInstanceVbo);
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(GrassBlade), reinterpret_cast<void*>(offsetof(GrassBlade, base)));
		glVertexAttribDivisor(2, 1);
		glEnableVertexAttribArray(3);
		glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(GrassBlade), reinterpret_cast<void*>(offsetof(GrassBlade, shape)));
		glVertexAttribDivisor(3, 1);
		glEnableVertexAttribArray(4);
		glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(GrassBlade), reinterpret_cast<void*>(offsetof(GrassBlade, color)));
		glVertexAttribDivisor(4, 1);
		glBindVertexArray(0);
	}

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
	bool ok = world.load() & depth.load() & unlit.load() & mesh.load() & meshDepth.load() & post.load() & glass.load() & sky.load() & bloomExtract.load() & bloomBlur.load() &
	          bloomComposite.load() & grass.load() & mirror.load();
	glass.bindBlock("Lights", 0);
	grass.bindBlock("Lights", 0);
	world.bindBlock("Lights", 0);
	mesh.bindBlock("Lights", 0);
	return ok && world.valid() && depth.valid() && unlit.valid() && mesh.valid() && meshDepth.valid() && post.valid() && glass.valid() && sky.valid() &&
	       bloomExtract.valid() && bloomBlur.valid() && bloomComposite.valid() && grass.valid() && mirror.valid();
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
	if (glass.reloadIfChanged())
		glass.bindBlock("Lights", 0);
	if (grass.reloadIfChanged())
		grass.bindBlock("Lights", 0);
	mirror.reloadIfChanged();
	sky.reloadIfChanged();
	bloomExtract.reloadIfChanged();
	bloomBlur.reloadIfChanged();
	bloomComposite.reloadIfChanged();
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

Renderer::ViewParams Renderer::viewOf(const Camera& camera)
{
	ViewParams v;
	v.view = camera.view();
	v.projection = camera.projection();
	v.toCamera = camera.toCamera();
	v.position = camera.position();
	v.perspective = camera.isPerspective();
	return v;
}

void Renderer::setLighting(Shader& shader, const ViewParams& v, const Level& level, const glm::mat4& lightSpace, int lightCount,
                           const RenderSettings& settings)
{
	shader.set("view", v.view);
	shader.set("projection", v.projection);
	shader.set("clipPlane", v.clipPlane);
	shader.set("lightSpace", lightSpace);
	shader.set("viewDir", v.toCamera);
	shader.set("cameraPos", v.position);
	shader.set("perspective", v.perspective ? 1 : 0);
	shader.set("sunDir", glm::normalize(level.sunDirection));
	shader.set("sunColor", flashedSun(level, settings));
	shader.set("ambient", level.ambient + settings.lightningFlash * 0.35f);
	shader.set("numLights", lightCount);
	shader.set("shadowsEnabled", settings.shadows ? 1 : 0);
	shader.set("shadowMap", 0);
	shader.set("shadowNormalOffset", shadowTexelWorld * 1.5f);
	shader.set("spotOn", settings.spotOn ? 1 : 0);
	shader.set("spotPos", settings.spotPos);
	shader.set("spotDir", settings.spotDir);
	shader.set("spotColor", settings.spotColor * settings.spotIntensity);
	shader.set("spotRange", settings.spotRange);
	shader.set("spotInner", settings.spotInner);
	shader.set("spotOuter", settings.spotOuter);
	setWeather(shader, settings);
}

glm::vec3 Renderer::flashedSun(const Level& level, const RenderSettings& settings) const
{
	// lightning lights the world from the sun's direction for a moment, cold and bright
	return level.sunColor + glm::vec3(0.75f, 0.82f, 1.0f) * settings.lightningFlash * 1.6f;
}

void Renderer::setWeather(Shader& shader, const RenderSettings& settings)
{
	shader.set("wetness", settings.wetness);
	shader.set("fogEnabled", settings.fog ? 1 : 0);
	shader.set("fogColor", settings.fogColor);
	shader.set("fogDensity", settings.fogDensity);
	shader.set("fogHeight", settings.fogHeight);
	shader.set("fogBase", settings.fogBase);
	shader.set("fogDistance", settings.fogDistance);
	shader.set("fogFocus", settings.fogFocus);
	shader.set("time", settings.time);
	shader.set("shadingStyle", settings.shadingStyle);
	shader.set("hatchSpacing", settings.hatchSpacing);
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

void Renderer::rebuildGrass(const Level& level, const RenderSettings& settings)
{
	// rebuild only when the grass tiles, their heights or the settings changed (editor)
	unsigned long long key = 1469598103934665603ull;
	auto mix = [&](unsigned long long v) { key = (key ^ v) * 1099511628211ull; };
	mix(static_cast<unsigned long long>(level.sizeX));
	mix(static_cast<unsigned long long>(level.sizeZ));
	mix(static_cast<unsigned long long>(settings.grassDensity));
	mix(static_cast<unsigned long long>(settings.grassHeight * 1000.0f));
	mix(static_cast<unsigned long long>(settings.grassDry * 1000.0f));
	for (size_t i = 0; i < level.tiles.size(); i++)
		mix(level.tiles[i] == 'g' ? static_cast<unsigned long long>(level.heights[i]) + 1 : 0);
	for (const glm::ivec2& t : level.trunks)
		mix(static_cast<unsigned long long>(t.x * 7919 + t.y));
	if (key == grassKey)
		return;
	grassKey = key;

	std::vector<GrassBlade> blades;
	std::uniform_real_distribution<float> u(0.0f, 1.0f);
	// lush palette, withered towards straw and grey-brown as grassDry rises
	const float wither = glm::clamp(settings.grassDry, 0.0f, 1.0f);
	const glm::vec3 dark = glm::mix(glm::vec3(0.15f, 0.30f, 0.12f), glm::vec3(0.20f, 0.18f, 0.12f), wither);
	const glm::vec3 light = glm::mix(glm::vec3(0.40f, 0.60f, 0.26f), glm::vec3(0.48f, 0.44f, 0.30f), wither);
	const glm::vec3 dry = glm::mix(glm::vec3(0.55f, 0.55f, 0.28f), glm::vec3(0.30f, 0.29f, 0.27f), wither);
	for (int x = 0; x < level.sizeX; x++)
	{
		for (int z = 0; z < level.sizeZ; z++)
		{
			if (level.tile(x, z) != 'g' || !level.standable(x, z))
				continue;
			std::mt19937 rng(static_cast<unsigned>(x * 73856093) ^ static_cast<unsigned>(z * 19349663)); // stable per tile
			const float top = level.surface(x, z);
			for (int i = 0; i < settings.grassDensity; i++)
			{
				GrassBlade b;
				b.base = glm::vec3(static_cast<float>(x) + (u(rng) - 0.5f) * 0.96f, top, static_cast<float>(z) + (u(rng) - 0.5f) * 0.96f);
				float tall = u(rng);
				b.shape = glm::vec4((0.16f + 0.26f * tall * tall) * settings.grassHeight, 0.035f + 0.03f * u(rng), u(rng) * glm::two_pi<float>(),
				                    u(rng) * glm::two_pi<float>());
				glm::vec3 c = glm::mix(dark, light, u(rng));
				if (u(rng) < 0.08f + 0.2f * wither)
					c = glm::mix(c, dry, 0.7f); // the odd dry blade
				b.color = c;
				blades.push_back(b);
			}
		}
	}
	grassBlades = static_cast<GLsizei>(blades.size());
	glBindBuffer(GL_ARRAY_BUFFER, grassInstanceVbo);
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(blades.size() * sizeof(GrassBlade)), blades.data(), GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Renderer::drawGrass(const ViewParams& v, const Level& level, const glm::mat4& lightSpace, int lightCount, const RenderSettings& settings)
{
	rebuildGrass(level, settings);
	if (grassBlades == 0)
		return;
	grass.use();
	setLighting(grass, v, level, lightSpace, lightCount, settings);
	grass.set("wind", settings.wind);
	grass.set("windMax", 8.0f);
	grass.set("pusher", settings.grassPusher);
	grass.set("pushRadius", 0.55f);
	grass.set("cutaway", settings.cutaway && !v.perspective && !v.reflected ? 1 : 0);
	grass.set("cutawayCenter", settings.cutawayCenter);
	grass.set("cutawayRadius", settings.cutawayRadius);
	grass.set("cutawayFloor", settings.cutawayFloor);
	grass.set("cutawayDir", settings.cutawayDir);
	// blades are flat: both sides visible; they stay out of the outline normals, or
	// every blade would get an ink line
	glDisable(GL_CULL_FACE);
	glColorMaski(1, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glBindVertexArray(grassVao);
	glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, kBladeVertices, grassBlades);
	glBindVertexArray(0);
	glColorMaski(1, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glEnable(GL_CULL_FACE);
	drawCalls++;
	instancesDrawn += grassBlades;
	world.use(); // callers continue with the world shader bound
}

void Renderer::drawOpaqueScene(const ViewParams& v, const Level& level, const FrameData& frame, const glm::mat4& lightSpace, int lightCount,
                               const RenderSettings& settings)
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, shadowTex);
	world.use();
	setLighting(world, v, level, lightSpace, lightCount, settings);
	// nothing to see through in first person or in a reflection
	world.set("cutaway", settings.cutaway && !v.perspective && !v.reflected ? 1 : 0);
	world.set("cutawayCenter", settings.cutawayCenter);
	world.set("cutawayRadius", settings.cutawayRadius);
	world.set("cutawayFloor", settings.cutawayFloor);
	world.set("cutawayDir", settings.cutawayDir);
	draw(frame.opaque);

	// grass, swaying in the wind
	if (settings.grass)
		drawGrass(v, level, lightSpace, lightCount, settings);

	// skinned / static meshes
	if (!frame.meshes.empty())
	{
		mesh.use();
		setLighting(mesh, v, level, lightSpace, lightCount, settings);
		mesh.set("albedo", 1);
		for (const MeshDraw& m : frame.meshes)
			drawMesh(mesh, m, true);
		glActiveTexture(GL_TEXTURE0);
	}
}

void Renderer::renderReflections(const Camera& camera, const Level& level, FrameData& frame, const glm::mat4& lightSpace, int lightCount,
                                 const RenderSettings& settings, int targetW, int targetH)
{
	activeMirrors.clear();
	mirrorsDrawn = 0;
	const ViewParams cam = viewOf(camera);

	// only mirrors that face the camera, nearest first, up to kMaxMirrors
	std::vector<MirrorDraw> facing;
	for (const MirrorDraw& m : frame.mirrors)
	{
		bool front = cam.perspective ? glm::dot(cam.position - m.center, m.normal) > 0.0f : glm::dot(cam.toCamera, m.normal) > 0.0f;
		if (front)
			facing.push_back(m);
	}
	std::sort(facing.begin(), facing.end(), [&](const MirrorDraw& a, const MirrorDraw& b) {
		return glm::length(a.center - camera.focus) < glm::length(b.center - camera.focus);
	});
	const size_t cap = static_cast<size_t>(std::clamp(settings.maxMirrors, 0, kMaxMirrors));
	if (facing.size() > cap)
		facing.resize(cap);
	if (facing.empty())
		return;

	if (!mirrorFbo[0] || targetW != mirrorW || targetH != mirrorH)
	{
		if (!mirrorFbo[0])
		{
			glGenFramebuffers(kMaxMirrors, mirrorFbo);
			glGenTextures(kMaxMirrors, mirrorTex);
			glGenRenderbuffers(kMaxMirrors, mirrorDepth);
		}
		mirrorW = targetW;
		mirrorH = targetH;
		for (int i = 0; i < kMaxMirrors; i++)
		{
			glBindTexture(GL_TEXTURE_2D, mirrorTex[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, mirrorW, mirrorH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindRenderbuffer(GL_RENDERBUFFER, mirrorDepth[i]);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mirrorW, mirrorH);
			glBindFramebuffer(GL_FRAMEBUFFER, mirrorFbo[i]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mirrorTex[i], 0);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mirrorDepth[i]);
			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
				LOG_ERROR("renderer: mirror framebuffer is incomplete (%dx%d)", mirrorW, mirrorH);
		}
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
	}

	glEnable(GL_CLIP_DISTANCE0);
	glFrontFace(GL_CW); // the reflection flips the winding
	for (size_t i = 0; i < facing.size(); i++)
	{
		const MirrorDraw& m = facing[i];
		const glm::vec3 n = glm::normalize(m.normal);
		const float d = glm::dot(n, m.center);
		// reflection across the plane dot(n, p) = d
		glm::mat4 r(1.0f);
		for (int c = 0; c < 3; c++)
			for (int row = 0; row < 3; row++)
				r[c][row] -= 2.0f * n[c] * n[row];
		r[3] = glm::vec4(2.0f * d * n, 1.0f);

		ViewParams v = cam;
		v.view = cam.view * r;
		v.position = glm::vec3(r * glm::vec4(cam.position, 1.0f));
		v.toCamera = glm::reflect(cam.toCamera, n);
		v.reflected = true;
		v.clipPlane = glm::vec4(n, -d - 0.01f); // drop whatever is behind the glass

		glBindFramebuffer(GL_FRAMEBUFFER, mirrorFbo[i]);
		glViewport(0, 0, mirrorW, mirrorH);
		glClearColor(settings.clearColor.r, settings.clearColor.g, settings.clearColor.b, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		if (settings.sky != 0)
		{
			glFrontFace(GL_CCW);
			drawSky(camera, settings, 0, mirrorW, mirrorH, &n);
			glFrontFace(GL_CW);
		}
		drawOpaqueScene(v, level, frame, lightSpace, lightCount, settings);
		drawReflectedEffects(v, frame);
		activeMirrors.push_back(m);
		mirrorsDrawn++;
	}
	glFrontFace(GL_CCW);
	glDisable(GL_CLIP_DISTANCE0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::drawReflectedEffects(const ViewParams& v, const FrameData& frame)
{
	// glowing boxes, particles and rain, far to near as seen in the mirror
	auto key = [&](const Instance& i) { return glm::dot(i.offset, v.toCamera); };
	auto sorted = [&](const std::vector<Instance>& src) {
		std::vector<Instance> out(src);
		std::sort(out.begin(), out.end(), [&](const Instance& a, const Instance& b) { return key(a) < key(b); });
		return out;
	};
	if (frame.overlay.empty() && frame.particlesAlpha.empty() && frame.particlesAdditive.empty())
		return;
	unlit.use();
	unlit.set("view", v.view);
	unlit.set("projection", v.projection);
	unlit.set("clipPlane", v.clipPlane);
	glEnable(GL_BLEND);
	glDepthMask(GL_FALSE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	draw(sorted(frame.overlay));
	draw(sorted(frame.particlesAlpha));
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	draw(frame.particlesAdditive); // additive: order does not matter
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
}

void Renderer::drawMirrors(const Camera& camera, bool pixelate)
{
	if (activeMirrors.empty())
		return;
	mirror.use();
	mirror.set("view", camera.view());
	mirror.set("projection", camera.projection());
	mirror.set("reflection", 5);
	mirror.set("viewOrigin", glm::vec3(static_cast<float>(pixelate ? 0 : viewX), 0.0f, 0.0f));
	mirror.set("viewSize", glm::vec3(static_cast<float>(mirrorW), static_cast<float>(mirrorH), 0.0f));
	glBindVertexArray(emptyVao);
	for (size_t i = 0; i < activeMirrors.size(); i++)
	{
		const MirrorDraw& m = activeMirrors[i];
		mirror.set("center", m.center);
		mirror.set("halfRight", m.right);
		mirror.set("halfUp", m.up);
		mirror.set("normal", glm::normalize(m.normal));
		mirror.set("tint", m.tint);
		mirror.set("tarnish", m.tarnish);
		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_2D, mirrorTex[i]);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		drawCalls++;
	}
	glActiveTexture(GL_TEXTURE0);
	glBindVertexArray(0);
}

void Renderer::draw(const std::vector<Instance>& instances)
{
	draw(instances.data(), instances.size());
}

void Renderer::draw(const Instance* instances, size_t count)
{
	if (count == 0)
		return;
	const GLsizeiptr bytes = static_cast<GLsizeiptr>(count * sizeof(Instance));
	glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
	// orphan + upload: lets the driver hand out fresh storage instead of stalling
	glBufferData(GL_ARRAY_BUFFER, bytes, nullptr, GL_STREAM_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, bytes, instances);
	glBindVertexArray(cubeVao);
	glDrawArraysInstanced(GL_TRIANGLES, 0, 36, static_cast<GLsizei>(count));
	drawCalls++;
	instancesDrawn += static_cast<int>(count);
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
		draw(frame.opaque); // glass (frame.transparent) lets the light through
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
	// offscreen when pixelating or drawing outlines (they need the depth as a texture)
	const bool finalPass = settings.grading || settings.vignette > 0.0f || settings.grain > 0.0f || settings.godRays;
	raysMin = frame.boundsMin - glm::vec3(2.0f, 0.0f, 2.0f);
	raysMax = frame.boundsMax + glm::vec3(2.0f, 3.0f, 2.0f); // some air above the tallest column
	const bool pixelate = settings.pixelSize > 1 || settings.outlines || finalPass;
	// 2. lights uniform block
	LightBlock block{};
	int lightCount = std::min(static_cast<int>(level.lights.size()), Level::kMaxLights);
	for (int i = 0; i < lightCount; i++)
	{
		const PointLight& l = level.lights[i];
		block.positionRadius[i] = glm::vec4(l.position, std::max(l.radius, 0.01f));
		float flicker = 1.0f;
		if (settings.lightFlicker > 0.0f)
		{
			// a few unrelated sines per light: reads as a flame, never as a pulse
			const float t = settings.time, k = static_cast<float>(i);
			float n = 0.5f * std::sin(t * 9.1f + k * 7.3f) + 0.3f * std::sin(t * 17.7f + k * 2.1f) + 0.2f * std::sin(t * 31.3f + k * 5.7f);
			float dip = std::max(0.0f, std::sin(t * 2.3f + k * 11.0f) - 0.92f) * 6.0f; // the odd gutter
			flicker = std::max(0.05f, 1.0f + settings.lightFlicker * (0.3f * n - 0.5f * dip));
		}
		block.colorIntensity[i] = glm::vec4(l.color, l.intensity * flicker);
	}
	glBindBuffer(GL_UNIFORM_BUFFER, lightUbo);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(LightBlock), &block);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	const int pixelSize = std::max(1, settings.pixelSize);

	// 2b. mirrors: the opaque world again, reflected, into one texture per mirror
	renderReflections(camera, level, frame, lightSpace, lightCount, settings, pixelate ? std::max(1, viewW / pixelSize) : viewW,
	                  pixelate ? std::max(1, height / pixelSize) : height);

	if (pixelate)
	{
		ensureSceneTarget(std::max(1, viewW / pixelSize), std::max(1, height / pixelSize));
		glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo);
		glViewport(0, 0, sceneW, sceneH);
	}
	else
	{
		glViewport(viewX, 0, viewW, height);
	}
	glClearColor(settings.clearColor.r, settings.clearColor.g, settings.clearColor.b, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	if (pixelate)
	{
		const GLfloat noNormal[4] = {0.0f, 0.0f, 0.0f, 0.0f};
		glClearBufferfv(GL_COLOR, 1, noNormal);
	}
	// only opaque geometry writes normals; everything else leaves attachment 1 alone
	glColorMaski(1, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	if (settings.sky != 0)
		drawSky(camera, settings, pixelate ? 0 : viewX, pixelate ? sceneW : viewW, pixelate ? sceneH : height);
	glColorMaski(1, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	// 3. opaque world: blocks, grass, models; then the mirrors showing their reflections
	drawOpaqueScene(viewOf(camera), level, frame, lightSpace, lightCount, settings);
	drawMirrors(camera, pixelate);

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

	glColorMaski(1, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	// 5. transparent queue: overlay, particles and glass, far to near.
	// Glass writes opaque colour over a copy of the frame, so everything behind a sphere
	// must be drawn before it (to be refracted) and everything in front after it.
	{
		const glm::vec3 toCam = camera.toCamera();
		auto key = [&](const glm::vec3& p) { return glm::dot(p, toCam); }; // larger = nearer
		auto sortFarToNear = [&](std::vector<Instance>& v) {
			std::sort(v.begin(), v.end(), [&](const Instance& a, const Instance& b) { return key(a.offset) < key(b.offset); });
		};
		sortFarToNear(frame.overlay);
		sortFarToNear(frame.particlesAlpha);
		sortFarToNear(frame.particlesAdditive);
		std::sort(frame.glass.begin(), frame.glass.end(),
		          [&](const GlassDraw& a, const GlassDraw& b) { return key(a.center) < key(b.center); });

		size_t nextOverlay = 0, nextAlpha = 0, nextAdditive = 0;
		auto drawUntil = [&](float limit) {
			auto take = [&](const std::vector<Instance>& v, size_t& i) {
				size_t from = i;
				while (i < v.size() && key(v[i].offset) < limit)
					i++;
				return std::make_pair(v.data() + from, i - from);
			};
			auto overlay = take(frame.overlay, nextOverlay);
			auto alpha = take(frame.particlesAlpha, nextAlpha);
			auto additive = take(frame.particlesAdditive, nextAdditive);
			if (!overlay.second && !alpha.second && !additive.second)
				return;
			unlit.use();
			unlit.set("view", view);
			unlit.set("projection", projection);
			unlit.set("clipPlane", glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
			glEnable(GL_BLEND);
			glDepthMask(GL_FALSE);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			draw(overlay.first, overlay.second);
			draw(alpha.first, alpha.second);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			draw(additive.first, additive.second);
			glDepthMask(GL_TRUE);
			glDisable(GL_BLEND);
		};

		for (const GlassDraw& g : frame.glass)
		{
			drawUntil(key(g.center));
			drawGlassSphere(camera, level, g, lightCount, pixelate, settings);
		}
		drawUntil(std::numeric_limits<float>::infinity());
	}

	// 6. bloom (before the pixelizer, so the glow is pixelated too)
	if (settings.bloom)
		applyBloom(settings, pixelate);

	// 7. pixelizer upscale + posterize
	glColorMaski(1, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
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
		post.set("sceneDepth", 1);
		post.set("sceneNormal", 3);
		post.set("outlines", settings.outlines ? 1 : 0);
		post.set("outlineColor", settings.outlineColor);
		post.set("outlineWidth", settings.outlineWidth);
		post.set("nearPlane", camera.nearPlane());
		post.set("farPlane", camera.farPlane());
		post.set("perspective", camera.isPerspective() ? 1 : 0);
		post.set("grading", settings.grading ? 1 : 0);
		post.set("saturation", settings.saturation);
		post.set("contrast", settings.contrast);
		post.set("shadowTint", settings.shadowTint);
		post.set("highlightTint", settings.highlightTint);
		post.set("vignette", settings.vignette);
		post.set("grain", settings.grain);
		post.set("time", settings.time);
		post.set("godRays", settings.godRays ? 1 : 0);
		if (settings.godRays)
		{
			post.set("invViewProj", glm::inverse(projection * view));
			post.set("lightSpace", lightSpace);
			post.set("shadowMap", 4);
			post.set("shadowsEnabled", settings.shadows ? 1 : 0);
			post.set("sunDir", glm::normalize(level.sunDirection));
			post.set("sunColor", flashedSun(level, settings));
			post.set("raysMin", raysMin);
			post.set("raysMax", raysMax);
			post.set("rayBase", settings.fogBase);
			post.set("rayFalloff", 1.5f + settings.fogHeight);
			post.set("godRayStrength", settings.godRayStrength);
			post.set("godRaySteps", std::clamp(settings.godRaySteps, 4, 64));
			glActiveTexture(GL_TEXTURE4);
			glBindTexture(GL_TEXTURE_2D, shadowTex);
		}
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, sceneNormal);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, sceneDepth);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, sceneColor);
		glBindVertexArray(emptyVao);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		drawCalls++;
		glEnable(GL_DEPTH_TEST);
	}

	glBindVertexArray(0);
}
