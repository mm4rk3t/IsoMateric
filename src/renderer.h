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

struct FrameData
{
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
	int pixelSize = 1;      // pixelizer: >1 renders at window/pixelSize and upscales
	int colorLevels = 0;    // pixelizer posterize: 0 = off, else levels per channel
	bool dither = true;
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
	void render(const Camera& camera, const Level& level, FrameData& frame, const RenderSettings& settings);
	void reloadShadersIfChanged();
	bool reloadShaders();

	int drawCalls = 0;
	int instancesDrawn = 0;

	static constexpr int kShadowSize = 2048;

private:
	void draw(const std::vector<Instance>& instances);
	void drawMesh(Shader& shader, const MeshDraw& mesh, bool materials);
	void setLighting(Shader& shader, const Camera& camera, const Level& level, const glm::mat4& lightSpace, int lightCount,
	                 const RenderSettings& settings);
	glm::mat4 sunMatrix(const Level& level, const FrameData& frame) const;
	void ensureSceneTarget(int w, int h);

	Shader world, depth, unlit, mesh, meshDepth, post;
	GLuint sceneFbo = 0, sceneColor = 0, sceneDepth = 0, emptyVao = 0;
	int sceneW = 0, sceneH = 0;
	GLuint cubeVao = 0, cubeVbo = 0, instanceVbo = 0;
	GLuint lightUbo = 0;
	GLuint shadowFbo = 0, shadowTex = 0;
	int width = 1, height = 1;
};

#endif
