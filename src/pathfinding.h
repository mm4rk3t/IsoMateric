#ifndef PATHFINDING_H
#define PATHFINDING_H

#include "level.h"

#include <glm/glm.hpp>

#include <vector>

// A* over standable columns. A step may go up at most one block (the player jumps)
// and down any amount. Diagonals only on flat ground and never through corners.
// Returns the tiles to visit after `start`, ending at `goal`; empty if unreachable.
std::vector<glm::ivec2> findPath(const Level& level, glm::ivec2 start, glm::ivec2 goal);

#endif
