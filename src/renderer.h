#ifndef RENDERER_H
#define RENDERER_H

#include "camera.h"
#include "level.h"
#include "model.h"
#include "shader.h"

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <vector>

// One axis-aligned box. Everything in the world is drawn as instanced unit cubes.
struct Instance
{
	glm::vec3 offset;
	glm::vec3 scale;
	glm::vec4 color; // alpha < 1 goes through the transparent pass
};

struct MeshDraw
{
	const Model* model;
	glm::mat4 transform;                    // entity transform incl. model fit/scale
	const std::vector<glm::mat4>* globals;  // node matrices (non-skinned primitives)
	const std::vector<glm::mat4>* joints;   // skinning palette
	glm::vec4 tint;
};

struct GlassDraw
{
	glm::vec3 center;
	float radius;
	glm::vec3 tint;
	float ior, roughness, absorption, dispersion;
};

struct FrameData
{
	std::vector<GlassDraw> glass; // drawn last, refracting everything before it
	std::vector<MeshDraw> meshes;
	std::vector<Instance> particlesAlpha;    // sorted back-to-front by the renderer
	std::vector<Instance> particlesAdditive;
	std::vector<Instance> opaque;
	std::vector<Instance> transparent; // sorted back-to-front by the renderer
	std::vector<Instance> overlay;     // unlit: hover highlight, path, light markers
	glm::vec3 boundsMin{0.0f}, boundsMax{0.0f};
};

struct RenderSettings
{
	bool shadows = true;
	int pixelSize = 4;      // pixelizer: >1 renders at window/pixelSize and upscales
	int colorLevels = 16;    // pixelizer posterize: 0 = off, else levels per channel
	bool dither = false;
	// see-through window: block pixels between the camera and cutawayCenter, within
	// cutawayRadius of the player on screen and above cutawayFloor, are discarded
	// sky backdrop: 0 plain clear colour, 1 atmosphere, 2 galaxy
	int sky = 0;
	float skyBrightness = 1.0f;
	glm::vec3 skyTop{0.2f, 0.3f, 0.6f};
	glm::vec3 skyHorizon{0.6f, 0.7f, 0.8f};
	float nightAmount = 0.0f;
	float bloodMoon = 0.0f;
	glm::vec3 celestialDir{0.0f, 1.0f, 0.0f};
	glm::vec3 celestialColor{1.0f};
	bool comets = true;

	// bloom: bright pixels bleed light, applied before the pixelizer
	bool bloom = false;
	float bloomThreshold = 0.72f;
	float bloomIntensity = 0.7f;
	int bloomPasses = 2;

	// stylised shading: 0 smooth, 1 cel, 2 Moebius; ink outlines from the depth buffer
	int shadingStyle = 0;
	float hatchSpacing = 5.0f;
	bool outlines = false;
	glm::vec3 outlineColor{0.05f, 0.04f, 0.06f};
	float outlineWidth = 1.0f;

	// weather
	glm::vec3 wind{0.0f};   // (x, 0, z) units per second; shared by rain and grass
	float wetness = 0.0f;

	// grass: blades on 'g' tiles, swayed by the wind and parted by the player
	bool grass = true;
	int grassDensity = 24;  // blades per tile
	float grassHeight = 1.0f;
	glm::vec3 grassPusher{0.0f, -1000.0f, 0.0f};
	bool fog = false;
	glm::vec3 fogColor{0.5f, 0.55f, 0.6f};
	float fogDensity = 0.8f;
	float fogHeight = 0.8f;
	float fogBase = 0.5f;
	float fogDistance = 0.0f;
	glm::vec3 fogFocus{0.0f};
	float time = 0.0f;

	bool cutaway = true;
	float cutawayRadius = 0.9f;
	glm::vec3 cutawayCenter{0.0f};
	float cutawayFloor = 0.0f;
	glm::vec3 cutawayDir{0.0f, 1.0f, 0.0f}; // from the player towards the camera
	glm::vec3 clearColor{0.1f, 0.1f, 0.12f};
};

class Renderer
{
public:
	Renderer() = default;
	~Renderer();
	Renderer(const Renderer&) = delete;
	Renderer& operator=(const Renderer&) = delete;

	bool init();
	void resize(int width, int height);
	// horizontal slice of the framebuffer the scene is drawn into (the rest is UI)
	void setViewRect(int x, int w);
	void render(const Camera& camera, const Level& level, FrameData& frame, const RenderSettings& settings);
	void reloadShadersIfChanged();
	bool reloadShaders();

	int drawCalls = 0;
	int instancesDrawn = 0;

	static constexpr int kShadowSize = 2048;

private:
	void draw(const std::vector<Instance>& instances);
	void draw(const Instance* instances, size_t count);
	void grabFrame(bool pixelate);
	void drawGlassSphere(const Camera& camera, const Level& level, const GlassDraw& g, int lightCount, bool pixelate,
	                     const RenderSettings& settings);
	void setWeather(Shader& shader, const RenderSettings& settings);
	void drawSky(const Camera& camera, const RenderSettings& settings, bool pixelate);
	void applyBloom(const RenderSettings& settings, bool pixelate);
	void buildSphere();
	void drawGrass(const Camera& camera, const Level& level, const glm::mat4& lightSpace, int lightCount, const RenderSettings& settings);
	void rebuildGrass(const Level& level, const RenderSettings& settings);
	void drawMesh(Shader& shader, const MeshDraw& mesh, bool materials);
	void setLighting(Shader& shader, const Camera& camera, const Level& level, const glm::mat4& lightSpace, int lightCount,
	                 const RenderSettings& settings);
	glm::mat4 sunMatrix(const Level& level, const FrameData& frame);
	float shadowTexelWorld = 0.01f; // world size of one shadow map texel
	void ensureSceneTarget(int w, int h);

	Shader world, depth, unlit, mesh, meshDepth, post;
	GLuint sceneFbo = 0, sceneColor = 0, sceneDepth = 0, sceneNormal = 0, emptyVao = 0;
	Shader grass;
	GLuint grassVao = 0, grassMeshVbo = 0, grassInstanceVbo = 0;
	GLsizei grassBlades = 0;
	unsigned long long grassKey = 0; // hash of what the blades were built from
	Shader glass, sky, bloomExtract, bloomBlur, bloomComposite;
	GLuint bloomFbo[2] = {0, 0}, bloomTex[2] = {0, 0};
	int bloomW = 0, bloomH = 0;
	GLuint sphereVao = 0, sphereVbo = 0, sphereEbo = 0;
	GLsizei sphereIndexCount = 0;
	GLuint grabFbo = 0, grabTex = 0; // copy of the frame so far, sampled by the glass shader
	int grabW = 0, grabH = 0;
	int sceneW = 0, sceneH = 0;
	GLuint cubeVao = 0, cubeVbo = 0, instanceVbo = 0;
	GLuint lightUbo = 0;
	GLuint shadowFbo = 0, shadowTex = 0;
	int width = 1, height = 1;
	int viewX = 0, viewW = 1;
};

#endif
