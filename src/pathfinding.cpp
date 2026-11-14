#include "pathfinding.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <limits>
#include <queue>
#include <utility>

std::vector<glm::ivec2> findPath(const Level& level, glm::ivec2 start, glm::ivec2 goal)
{
	if (!level.inBounds(start.x, start.y) || !level.standable(goal.x, goal.y) || start == goal)
		return {};

	const int sizeZ = level.sizeZ;
	const int cells = level.sizeX * level.sizeZ;
	const float kDiag = std::sqrt(2.0f);
	auto idx = [sizeZ](int x, int z) { return x * sizeZ + z; };
	auto heuristic = [&](int x, int z) {
		int dx = std::abs(x - goal.x), dz = std::abs(z - goal.y);
		return static_cast<float>(dx + dz) + (kDiag - 2.0f) * static_cast<float>(std::min(dx, dz));
	};

	std::vector<float> cost(cells, std::numeric_limits<float>::infinity());
	std::vector<int> parent(cells, -1);
	std::vector<char> closed(cells, 0);

	using Node = std::pair<float, int>;
	std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;

	const int startIdx = idx(start.x, start.y);
	const int goalIdx = idx(goal.x, goal.y);
	cost[startIdx] = 0.0f;
	open.push({heuristic(start.x, start.y), startIdx});

	static const int dirs[8][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

	while (!open.empty())
	{
		int current = open.top().second;
		open.pop();
		if (closed[current])
			continue;
		closed[current] = 1;
		if (current == goalIdx)
			break;

		int cx = current / sizeZ, cz = current % sizeZ;
		int h = level.height(cx, cz);
		for (const auto& d : dirs)
		{
			int nx = cx + d[0], nz = cz + d[1];
			if (!level.standable(nx, nz) || level.height(nx, nz) - h > 1)
				continue;
			bool diagonal = d[0] != 0 && d[1] != 0;
			if (diagonal && (level.height(nx, nz) != h || level.height(cx + d[0], cz) != h || level.height(cx, cz + d[1]) != h))
				continue;

			int next = idx(nx, nz);
			if (closed[next])
				continue;
			float climb = level.height(nx, nz) > h ? 0.5f : 0.0f; // prefer flat routes
			float newCost = cost[current] + (diagonal ? kDiag : 1.0f) + climb;
			if (newCost < cost[next])
			{
				cost[next] = newCost;
				parent[next] = current;
				open.push({newCost + heuristic(nx, nz), next});
			}
		}
	}

	if (parent[goalIdx] == -1)
		return {};

	std::vector<glm::ivec2> path;
	for (int at = goalIdx; at != startIdx; at = parent[at])
		path.emplace_back(at / sizeZ, at % sizeZ);
	std::reverse(path.begin(), path.end());
	return path;
}
