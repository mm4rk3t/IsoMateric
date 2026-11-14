#ifndef LEVEL_H
#define LEVEL_H

#include <glm/glm.hpp>

#include <string>
#include <vector>

struct PointLight
{
	glm::vec3 position{0.0f, 3.0f, 0.0f};
	glm::vec3 color{1.0f};
	float intensity = 1.0f;
	float radius = 8.0f;
};

// A height-map of unit blocks. Column (x, z) holds `height` blocks stacked from y = 0;
// 0 is a hole, 1 is floor, every extra block is one step up. The top surface of a
// column is at y = height - 0.5.
// Block (x, y, z) is centred at world (x, y, z) and spans +-0.5 on every axis.
class Level
{
public:
	static constexpr int kMaxHeight = 8;
	static constexpr int kMaxLights = 32;

	static Level flat(int sizeX, int sizeZ);

	bool load(const std::string& path);
	bool save(const std::string& path) const;

	bool inBounds(int x, int z) const { return x >= 0 && z >= 0 && x < sizeX && z < sizeZ; }
	int height(int x, int z) const { return inBounds(x, z) ? heights[index(x, z)] : 0; }
	void setHeight(int x, int z, int h);
	bool standable(int x, int z) const { return height(x, z) >= 1; }
	float surface(int x, int z) const { return static_cast<float>(height(x, z)) - 0.5f; }
	// solid block test used by collision; outside the level counts as an endless wall
	bool solid(int x, int y, int z) const { return !inBounds(x, z) || (y >= 0 && y < height(x, z)); }
	int maxHeight() const;

	int sizeX = 0, sizeZ = 0;
	std::vector<int> heights;
	glm::ivec2 spawn{0, 0};

	glm::vec3 sunDirection{-0.43f, -0.33f, -0.38f}; // direction the light travels
	glm::vec3 sunColor{0.992f, 0.239f, 0.251f};
	float ambient = 0.058f;
	std::vector<PointLight> lights;

private:
	int index(int x, int z) const { return x * sizeZ + z; }
};

#endif
