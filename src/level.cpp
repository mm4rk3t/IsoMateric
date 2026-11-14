#include "level.h"
#include "log.h"

#include <algorithm>
#include <fstream>
#include <sstream>

// File format (plain text, '#' starts a comment):
//
//   size <sizeX> <sizeZ>
//   spawn <x> <z>
//   sun <dirX> <dirY> <dirZ> <r> <g> <b>
//   ambient <value>
//   sky plain|atmosphere|galaxy
//   script <path relative to assets/>
//   asteroids <count>
//   trunk <x> <z>           an endless tree trunk through that column (any number)
//   light <x> <y> <z> <r> <g> <b> <intensity> <radius>   (any number of lines)
//   heights
//   <sizeZ numbers>   one row per x, sizeX rows

Level Level::flat(int sizeX, int sizeZ)
{
	Level level;
	level.sizeX = std::max(1, sizeX);
	level.sizeZ = std::max(1, sizeZ);
	level.heights.assign(static_cast<size_t>(level.sizeX) * level.sizeZ, 1);
	level.spawn = glm::ivec2(level.sizeX / 2, level.sizeZ / 2);
	return level;
}

void Level::setHeight(int x, int z, int h)
{
	if (inBounds(x, z))
		heights[index(x, z)] = std::clamp(h, 0, kMaxHeight);
}

int Level::maxHeight() const
{
	return heights.empty() ? 0 : *std::max_element(heights.begin(), heights.end());
}

bool Level::load(const std::string& path)
{
	std::ifstream file(path);
	if (!file)
	{
		LOG_ERROR("level: cannot open '%s'", path.c_str());
		return false;
	}

	Level result;
	std::string line;
	int lineNo = 0;
	bool readingHeights = false;
	int row = 0;

	auto fail = [&](const char* what) {
		LOG_ERROR("level: %s:%d: %s", path.c_str(), lineNo, what);
		return false;
	};

	while (std::getline(file, line))
	{
		lineNo++;
		line = line.substr(0, line.find('#'));
		std::istringstream in(line);
		std::string key;
		if (!(in >> key))
			continue;

		if (readingHeights)
		{
			in.clear();
			in.str(line);
			for (int z = 0; z < result.sizeZ; z++)
			{
				int h;
				if (!(in >> h))
					return fail("height row is too short");
				result.setHeight(row, z, h);
			}
			if (++row == result.sizeX)
				readingHeights = false;
			continue;
		}

		if (key == "size")
		{
			int sx, sz;
			if (!(in >> sx >> sz) || sx <= 0 || sz <= 0 || sx > 1024 || sz > 1024)
				return fail("bad size");
			result = Level::flat(sx, sz);
		}
		else if (key == "spawn")
		{
			if (!(in >> result.spawn.x >> result.spawn.y))
				return fail("bad spawn");
		}
		else if (key == "sun")
		{
			glm::vec3& d = result.sunDirection;
			glm::vec3& c = result.sunColor;
			if (!(in >> d.x >> d.y >> d.z >> c.r >> c.g >> c.b))
				return fail("bad sun");
		}
		else if (key == "sky")
		{
			if (!(in >> result.sky) || (result.sky != "plain" && result.sky != "atmosphere" && result.sky != "galaxy"))
				return fail("sky must be plain, atmosphere or galaxy");
		}
		else if (key == "script")
		{
			if (!(in >> result.script))
				return fail("bad script");
		}
		else if (key == "trunk")
		{
			glm::ivec2 t;
			if (!(in >> t.x >> t.y))
				return fail("bad trunk");
			result.trunks.push_back(t);
		}
		else if (key == "asteroids")
		{
			if (!(in >> result.asteroids) || result.asteroids < 0 || result.asteroids > 500)
				return fail("asteroids must be 0..500");
		}
		else if (key == "ambient")
		{
			if (!(in >> result.ambient))
				return fail("bad ambient");
		}
		else if (key == "light")
		{
			PointLight l;
			if (!(in >> l.position.x >> l.position.y >> l.position.z >> l.color.r >> l.color.g >> l.color.b >> l.intensity >> l.radius))
				return fail("bad light");
			if (static_cast<int>(result.lights.size()) < kMaxLights)
				result.lights.push_back(l);
			else
				LOG_WARN("level: %s:%d: more than %d lights, ignoring", path.c_str(), lineNo, kMaxLights);
		}
		else if (key == "heights")
		{
			if (result.sizeX == 0)
				return fail("'heights' before 'size'");
			readingHeights = true;
			row = 0;
		}
		else
		{
			return fail("unknown keyword");
		}
	}

	if (result.sizeX == 0)
		return fail("missing 'size'");
	if (readingHeights)
		return fail("not enough height rows");
	if (!result.inBounds(result.spawn.x, result.spawn.y))
		result.spawn = glm::ivec2(result.sizeX / 2, result.sizeZ / 2);

	*this = std::move(result);
	LOG_INFO("level: loaded '%s' (%dx%d)", path.c_str(), sizeX, sizeZ);
	return true;
}

bool Level::save(const std::string& path) const
{
	std::ofstream file(path);
	if (!file)
	{
		LOG_ERROR("level: cannot write '%s'", path.c_str());
		return false;
	}

	file << "# IsoMateric level\n";
	file << "size " << sizeX << ' ' << sizeZ << '\n';
	file << "spawn " << spawn.x << ' ' << spawn.y << '\n';
	file << "sun " << sunDirection.x << ' ' << sunDirection.y << ' ' << sunDirection.z << "  "
	     << sunColor.r << ' ' << sunColor.g << ' ' << sunColor.b << '\n';
	file << "ambient " << ambient << '\n';
	file << "sky " << sky << '\n';
	file << "script " << script << '\n';
	if (asteroids > 0)
		file << "asteroids " << asteroids << '\n';
	for (const glm::ivec2& t : trunks)
		file << "trunk " << t.x << ' ' << t.y << '\n';
	for (const PointLight& l : lights)
		file << "light " << l.position.x << ' ' << l.position.y << ' ' << l.position.z << "  "
		     << l.color.r << ' ' << l.color.g << ' ' << l.color.b << "  " << l.intensity << ' ' << l.radius << '\n';
	file << "heights\n";
	for (int x = 0; x < sizeX; x++)
	{
		for (int z = 0; z < sizeZ; z++)
			file << height(x, z) << (z + 1 < sizeZ ? " " : "\n");
	}

	if (!file)
	{
		LOG_ERROR("level: failed while writing '%s'", path.c_str());
		return false;
	}
	LOG_INFO("level: saved '%s'", path.c_str());
	return true;
}
