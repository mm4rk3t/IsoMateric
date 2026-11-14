#ifndef MODEL_H
#define MODEL_H

#include "texture.h"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// A glTF 2.0 model (.glb / .gltf): meshes, one skin and its animations.
// Skinning happens on the GPU; pose() fills the joint matrix palette on the CPU.
class Model
{
public:
	static constexpr int kMaxJoints = 32; // must match MAX_JOINTS in mesh.vs

	struct Primitive
	{
		GLuint vao = 0, vbo = 0, ebo = 0;
		GLsizei indexCount = 0;
		int node = -1;      // node that instantiates the mesh (used when not skinned)
		int material = -1;
		bool skinned = false;
	};

	struct Material
	{
		glm::vec4 baseColor{1.0f};
		int texture = -1; // index into textures
	};

	struct Node
	{
		int parent = -1;
		glm::vec3 translation{0.0f};
		glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
		glm::vec3 scale{1.0f};
	};

	struct Channel
	{
		enum class Path { Translation, Rotation, Scale };
		enum class Interpolation { Linear, Step, CubicSpline };
		int node = -1;
		Path path = Path::Translation;
		Interpolation interpolation = Interpolation::Linear;
		std::vector<float> times;
		std::vector<glm::vec4> values; // xyz for T/S, xyzw quaternion for R
	};

	struct Clip
	{
		std::string name;
		float duration = 0.0f;
		std::vector<Channel> channels;
	};

	Model() = default;
	~Model();
	Model(const Model&) = delete;
	Model& operator=(const Model&) = delete;

	bool load(const std::string& path);

	// Evaluates clip `clip` at `time` (clip < 0: rest pose). nodeGlobals gets every node's
	// model-space matrix, joints the skinning palette (global * inverse bind).
	void pose(int clip, float time, std::vector<glm::mat4>& nodeGlobals, std::vector<glm::mat4>& joints) const;

	std::vector<Primitive> primitives;
	std::vector<Material> materials;
	std::vector<Texture> textures;
	std::vector<Node> nodes;
	std::vector<int> nodeOrder; // parents before children
	std::vector<int> skinJoints;
	std::vector<glm::mat4> inverseBind;
	std::vector<Clip> clips;
	glm::vec3 boundsMin{0.0f}, boundsMax{0.0f}; // rest pose, model space
	std::string path;
};

// Loads each model file once and shares it between entities.
class ModelCache
{
public:
	std::shared_ptr<Model> get(const std::string& path);
	void clear() { models.clear(); }

private:
	std::unordered_map<std::string, std::shared_ptr<Model>> models;
};

#endif
