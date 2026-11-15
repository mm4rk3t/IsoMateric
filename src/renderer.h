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
	float emissive = 0.0f; // self-lit: adds color * emissive after lighting
};

struct MeshDraw
{
	const Model* model;
	glm::mat4 transform;                    // entity transform incl. model fit/scale
	const std::vector<glm::mat4>* globals;  // node matrices (non-skinned primitives)
	const std::vector<glm::mat4>* joints;   // skinning palette
	glm::vec4 tint;
	bool reflectionOnly = false; // drawn only in mirror reflections
	bool castShadow = true;
};

struct GlassDraw
{
	glm::vec3 center;
	float radius;
	glm::vec3 tint;
	float ior, roughness, absorption, dispersion;
};

struct MirrorDraw
{
	glm::vec3 center;
	glm::vec3 normal;  // the side that reflects
	glm::vec3 right;   // half width along the surface
	glm::vec3 up;      // half height
	glm::vec3 tint;
	float tarnish;
};

struct FrameData
{
	std::vector<MirrorDraw> mirrors;
	std::vector<GlassDraw> glass; // drawn last, refracting everything before it
	std::vector<MeshDraw> meshes;
	std::vector<Instance> particlesAlpha;    // sorted back-to-front by the renderer
	std::vector<Instance> particlesAdditive;
	std::vector<Instance> opaque;
	std::vector<Instance> reflectionOnly; // opaque, but only drawn in mirror reflections
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
	// sky backdrop: 0 plain clear colour, 1 atmosphere, 2 galaxy, 3 overcast
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

	// flashlight: one spotlight, no shadows
	bool spotOn = false;
	glm::vec3 spotPos{0.0f};
	glm::vec3 spotDir{0.0f, -1.0f, 0.0f};
	glm::vec3 spotColor{1.0f, 0.93f, 0.78f};
	float spotIntensity = 2.4f;
	float spotRange = 11.0f;
	float spotInner = 0.93f; // cos of the bright core's half angle
	float spotOuter = 0.82f; // cos of the edge's half angle

	int maxMirrors = 4;     // mirror reflections drawn per frame (each redraws the world)

	// lights and atmosphere
	float lightFlicker = 0.0f;    // 0 steady .. 1 candle-like flicker of point lights
	float lightningFlash = 0.0f;  // 0..1 this frame, driven by the weather
	bool godRays = false;         // sun/moon shafts ray-marched through the shadow map
	float godRayStrength = 0.6f;
	int godRaySteps = 32;

	// colour grading, vignette and film grain (final pass)
	bool grading = false;
	float saturation = 1.0f;
	float contrast = 1.0f;
	glm::vec3 shadowTint{1.0f};
	glm::vec3 highlightTint{1.0f};
	float vignette = 0.0f;
	float grain = 0.0f;

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
	float grassDry = 0.0f;  // 0 lush green .. 1 withered
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
	float cutawayRadius = 1.2f;
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
	static constexpr int kMaxMirrors = 4; // each one renders the opaque world again
	int mirrorsDrawn = 0;

private:
	// what a scene pass is drawn from: the camera, or the camera mirrored in a mirror
	struct ViewParams
	{
		glm::mat4 view{1.0f}, projection{1.0f};
		glm::vec3 toCamera{0.0f, 1.0f, 0.0f};
		glm::vec3 position{0.0f};
		bool perspective = false;
		bool reflected = false;
		glm::vec4 clipPlane{0.0f, 0.0f, 0.0f, 1.0f}; // keeps dot(clipPlane, (p, 1)) >= 0
	};
	static ViewParams viewOf(const Camera& camera);
	void drawOpaqueScene(const ViewParams& v, const Level& level, const FrameData& frame, const glm::mat4& lightSpace, int lightCount,
	                     const RenderSettings& settings);
	void renderReflections(const Camera& camera, const Level& level, FrameData& frame, const glm::mat4& lightSpace, int lightCount,
	                       const RenderSettings& settings, int targetW, int targetH);
	void drawMirrors(const Camera& camera, bool pixelate);
	void draw(const std::vector<Instance>& instances);
	void draw(const Instance* instances, size_t count);
	void grabFrame(bool pixelate);
	void drawGlassSphere(const Camera& camera, const Level& level, const GlassDraw& g, int lightCount, bool pixelate,
	                     const RenderSettings& settings);
	void setWeather(Shader& shader, const RenderSettings& settings);
	// mirrorNormal: draw the sky as seen in a mirror with that normal
	void drawSky(const Camera& camera, const RenderSettings& settings, int originX, int w, int h, const glm::vec3* mirrorNormal = nullptr);
	void drawReflectedEffects(const ViewParams& v, const FrameData& frame);
	void applyBloom(const RenderSettings& settings, bool pixelate);
	void buildSphere();
	void drawGrass(const ViewParams& v, const Level& level, const glm::mat4& lightSpace, int lightCount, const RenderSettings& settings);
	void rebuildGrass(const Level& level, const RenderSettings& settings);
	void drawMesh(Shader& shader, const MeshDraw& mesh, bool materials);
	void setLighting(Shader& shader, const ViewParams& v, const Level& level, const glm::mat4& lightSpace, int lightCount,
	                 const RenderSettings& settings);
	glm::mat4 sunMatrix(const Level& level, const FrameData& frame);
	glm::vec3 flashedSun(const Level& level, const RenderSettings& settings) const;
	float shadowTexelWorld = 0.01f;
	glm::vec3 raysMin{0.0f}, raysMax{0.0f}; // volume the god rays are marched through // world size of one shadow map texel
	void ensureSceneTarget(int w, int h);

	Shader world, depth, unlit, mesh, meshDepth, post;
	GLuint sceneFbo = 0, sceneColor = 0, sceneDepth = 0, sceneNormal = 0, emptyVao = 0;
	Shader grass, mirror;
	GLuint mirrorFbo[kMaxMirrors] = {}, mirrorTex[kMaxMirrors] = {}, mirrorDepth[kMaxMirrors] = {};
	int mirrorW = 0, mirrorH = 0;
	std::vector<MirrorDraw> activeMirrors; // the ones with a reflection this frame, same order as mirrorTex
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
