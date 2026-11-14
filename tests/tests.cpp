// Dependency-free tests for the GL-independent core (level, collision, pathfinding).
// Run with `ctest --test-dir build` or ./build/IsoMateric/bin/IsoMateric_tests.

#include "components.h"
#include "level.h"
#include "particles.h"
#include "pathfinding.h"
#include "player.h"
#include "weather.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

static int failures = 0;

#define CHECK(cond)                                                           \
	do {                                                                      \
		if (!(cond)) {                                                        \
			std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
			failures++;                                                       \
		}                                                                     \
	} while (0)

static void testLevelRoundTrip()
{
	Level a = Level::flat(5, 7);
	a.setHeight(1, 2, 3);
	a.setHeight(4, 6, 0);
	a.spawn = {2, 3};
	a.setTile(1, 2, 's');
	a.setTile(4, 6, 'w');
	PointLight l;
	l.position = {1.5f, 2.0f, 3.0f};
	l.radius = 4.0f;
	a.lights.push_back(l);

	std::string path = (std::filesystem::temp_directory_path() / "isomateric_test.level").string();
	CHECK(a.save(path));
	Level b;
	CHECK(b.load(path));
	std::filesystem::remove(path);

	CHECK(b.sizeX == 5 && b.sizeZ == 7);
	CHECK(b.height(1, 2) == 3);
	CHECK(b.tile(1, 2) == 's' && b.tile(4, 6) == 'w' && b.tile(0, 0) == '.');
	CHECK(b.height(4, 6) == 0);
	CHECK(b.spawn == glm::ivec2(2, 3));
	CHECK(b.lights.size() == 1 && std::abs(b.lights[0].radius - 4.0f) < 1e-5f);
	CHECK(b.height(-1, 0) == 0 && !b.standable(5, 0) && b.solid(5, 3, 0)); // outside is a wall
}

static void testLevelRejectsGarbage()
{
	std::string path = (std::filesystem::temp_directory_path() / "isomateric_bad.level").string();
	{
		FILE* f = std::fopen(path.c_str(), "w");
		std::fputs("size 2 2\nheights\n1 1\n", f); // one row missing
		std::fclose(f);
	}
	Level level = Level::flat(3, 3);
	CHECK(!level.load(path));
	CHECK(level.sizeX == 3); // a failed load leaves the level untouched
	std::filesystem::remove(path);
}

static void testWallStopsPlayer()
{
	Level level = Level::flat(10, 10);
	level.setHeight(5, 4, 2); // wall at x = 5 spanning 4.5 .. 5.5
	level.setHeight(5, 5, 2);
	Player p;
	p.placeAt(level, {2, 5});
	for (int i = 0; i < 600; i++) // 5 seconds pushing +x
		p.update(1.0f / 120.0f, level, {1.0f, 0.0f}, false);
	CHECK(p.position.x + p.size.x * 0.5f <= 4.5f + 1e-3f);
	CHECK(p.position.x > 4.0f); // and it actually reached the wall
}

static void testLevelEdgeIsAWall()
{
	Level level = Level::flat(4, 4);
	Player p;
	p.placeAt(level, {1, 1});
	for (int i = 0; i < 600; i++)
		p.update(1.0f / 120.0f, level, glm::normalize(glm::vec2(-1.0f, -1.0f)), false);
	CHECK(p.position.x - p.size.x * 0.5f >= -0.5f - 1e-3f);
	CHECK(p.position.z - p.size.z * 0.5f >= -0.5f - 1e-3f);
}

static void testSlidesAlongWall()
{
	Level level = Level::flat(10, 10);
	for (int z = 0; z < 10; z++)
		level.setHeight(6, z, 2);
	Player p;
	p.placeAt(level, {5, 2});
	float zBefore = p.position.z;
	glm::vec2 dir = glm::normalize(glm::vec2(1.0f, 1.0f));
	for (int i = 0; i < 120; i++)
		p.update(1.0f / 120.0f, level, dir, false);
	CHECK(p.position.x + p.size.x * 0.5f <= 5.5f + 1e-3f);
	CHECK(p.position.z > zBefore + 1.0f); // kept moving along the wall
}

static void testPathAroundWall()
{
	Level level = Level::flat(7, 7);
	for (int z = 0; z < 6; z++) // wall too high to climb, gap at z = 6
		level.setHeight(3, z, 3);
	auto path = findPath(level, {0, 0}, {6, 0});
	CHECK(!path.empty());
	CHECK(!path.empty() && path.back() == glm::ivec2(6, 0));
	for (const auto& t : path)
		CHECK(level.standable(t.x, t.y));
	CHECK(path.size() >= 12); // must detour through the gap
}

static void testNoCornerCutting()
{
	Level level = Level::flat(3, 3);
	level.setHeight(1, 0, 3);
	level.setHeight(0, 1, 3);
	// (0,0) is boxed in diagonally: (1,1) is only reachable by cutting a corner
	CHECK(findPath(level, {0, 0}, {1, 1}).empty());
}

static void testUnreachableGoal()
{
	Level level = Level::flat(5, 5);
	level.setHeight(2, 2, 3);
	CHECK(findPath(level, {0, 0}, {2, 2}).empty()); // goal is a wall
}

static void stepFor(Player& p, const Level& level, float seconds, glm::vec2 dir, bool jumpFirst = false)
{
	int steps = static_cast<int>(seconds * 120.0f);
	for (int i = 0; i < steps; i++)
		p.update(1.0f / 120.0f, level, dir, jumpFirst && i == 0);
}

static void testGravityLands()
{
	Level level = Level::flat(5, 5);
	Player p;
	p.placeAt(level, {2, 2});
	p.position.y += 3.0f; // drop from above
	p.grounded = false;
	stepFor(p, level, 2.0f, {0.0f, 0.0f});
	CHECK(p.grounded);
	CHECK(std::abs(p.feet().y - level.surface(2, 2)) < 0.01f);
}

static void testJumpOntoBlock()
{
	Level level = Level::flat(8, 3);
	for (int x = 4; x < 8; x++)
		for (int z = 0; z < 3; z++)
			level.setHeight(x, z, 2); // a one block step
	Player p;
	p.placeAt(level, {2, 1});
	stepFor(p, level, 0.5f, {1.0f, 0.0f});           // walk into the step
	CHECK(p.position.x < 3.6f);                        // blocked without jumping
	stepFor(p, level, 1.0f, {1.0f, 0.0f}, true);      // jump while pushing forward
	CHECK(p.position.x > 4.0f);
	CHECK(p.grounded && std::abs(p.feet().y - level.surface(5, 1)) < 0.01f);
}

static void testCannotJumpTwoBlocks()
{
	Level level = Level::flat(8, 3);
	for (int z = 0; z < 3; z++)
		level.setHeight(4, z, 3);
	Player p;
	p.placeAt(level, {2, 1});
	stepFor(p, level, 1.5f, {1.0f, 0.0f}, true);
	CHECK(p.position.x < 3.6f);
}

static void testFallIntoHoleRespawns()
{
	Level level = Level::flat(5, 1);
	level.setHeight(3, 0, 0); // hole
	Player p;
	p.placeAt(level, {1, 0});
	bool respawned = false;
	for (int i = 0; i < 600 && !respawned; i++)
		respawned = p.update(1.0f / 120.0f, level, {1.0f, 0.0f}, false).respawned;
	CHECK(respawned);
	CHECK(p.tile() == glm::ivec2(1, 0));
}

static void testPathClimbsOneBlock()
{
	Level level = Level::flat(5, 1);
	level.setHeight(2, 0, 2);
	level.setHeight(3, 0, 3);
	level.setHeight(4, 0, 4); // a staircase
	auto path = findPath(level, {0, 0}, {4, 0});
	CHECK(path.size() == 4);

	level.setHeight(3, 0, 4); // now a two block jump
	CHECK(findPath(level, {0, 0}, {4, 0}).empty());
	CHECK(!findPath(level, {4, 0}, {0, 0}).empty()); // dropping down is fine
}

static void testParticlesBurstAndCleanup()
{
	entt::registry reg;
	std::mt19937 rng(7);
	entt::entity e = reg.create();
	reg.emplace<Transform>(e);
	reg.emplace<ParticleEmitter>(e, particlePreset("dust"));
	updateParticles(reg, 0.01f, rng);
	CHECK(reg.get<ParticleEmitter>(e).particles.size() == 18);
	for (int i = 0; i < 200 && reg.valid(e); i++)
		updateParticles(reg, 0.02f, rng);
	CHECK(!reg.valid(e)); // one-shot emitter destroyed itself
}

static void testParticlesRate()
{
	entt::registry reg;
	std::mt19937 rng(7);
	entt::entity e = reg.create();
	reg.emplace<Transform>(e);
	ParticleEmitter em = particlePreset("fire");
	em.lifetime = 100.0f;
	em.lifetimeJitter = 0.0f;
	reg.emplace<ParticleEmitter>(e, em);
	for (int i = 0; i < 100; i++)
		updateParticles(reg, 0.01f, rng); // 1 second at 60/s
	size_t n = reg.get<ParticleEmitter>(e).particles.size();
	CHECK(n >= 59 && n <= 61);
}

static void testRainDensityAndSplashes()
{
	Level level = Level::flat(20, 20);
	Rain rain;
	std::mt19937 rng(5);
	glm::vec3 center(10.0f, 1.0f, 10.0f);
	rain.update(0.016f, level, center, 0.5f, {0.0f, 0.0f}, rng);
	CHECK(rain.drops.size() == static_cast<size_t>(rain.maxDrops / 2));

	int hits = 0;
	for (int i = 0; i < 120; i++)
	{
		rain.update(1.0f / 60.0f, level, center, 0.5f, {1.0f, 0.0f}, rng);
		hits += rain.hitsLastUpdate;
		for (const Rain::Drop& d : rain.drops) // never below a block top it is over
		{
			int tx = static_cast<int>(std::floor(d.position.x + 0.5f)), tz = static_cast<int>(std::floor(d.position.z + 0.5f));
			if (level.standable(tx, tz))
				CHECK(d.position.y > level.surface(tx, tz));
		}
	}
	CHECK(hits > 0);
	CHECK(!rain.splashes.empty());

	rain.update(0.016f, level, center, 0.0f, {0.0f, 0.0f}, rng); // rain stops
	CHECK(rain.drops.empty());
}

static void testTrunkIsSolidEverywhere()
{
	Level level = Level::flat(9, 3);
	level.trunks.push_back({4, 1});
	CHECK(level.solid(4, -30, 1) && level.solid(4, 0, 1) && level.solid(4, 40, 1));
	CHECK(!level.standable(4, 1));
	Player p;
	p.placeAt(level, {1, 1});
	stepFor(p, level, 2.0f, {1.0f, 0.0f}, true); // even jumping does not get through
	CHECK(p.position.x + p.size.x * 0.5f <= 3.5f + 1e-3f);
	auto path = findPath(level, {1, 1}, {7, 1});
	CHECK(!path.empty());
	for (const auto& t : path)
		CHECK(!(t.x == 4 && t.y == 1)); // walks around it
}

int main()
{
	testTrunkIsSolidEverywhere();
	testRainDensityAndSplashes();
	testGravityLands();
	testJumpOntoBlock();
	testCannotJumpTwoBlocks();
	testFallIntoHoleRespawns();
	testPathClimbsOneBlock();
	testParticlesBurstAndCleanup();
	testParticlesRate();
	testLevelRoundTrip();
	testLevelRejectsGarbage();
	testWallStopsPlayer();
	testLevelEdgeIsAWall();
	testSlidesAlongWall();
	testPathAroundWall();
	testNoCornerCutting();
	testUnreachableGoal();

	if (failures == 0)
		std::printf("all tests passed\n");
	return failures == 0 ? 0 : 1;
}
