#ifndef PROFILER_H
#define PROFILER_H

// Two profilers behind one macro:
//  * Tracy (https://github.com/wolfpld/tracy) when built with -DISOMATERIC_TRACY=ON;
//    connect the Tracy GUI to the running game for a full timeline.
//  * A tiny built-in one that is always on and shows smoothed CPU times in the debug UI.

#include <tracy/Tracy.hpp>

#include <chrono>
#include <utility>
#include <vector>

class CpuProfiler
{
public:
	struct Entry
	{
		const char* name;
		double ms;
	};

	static void record(const char* name, double ms);
	static const std::vector<Entry>& entries();
};

struct ProfileScope
{
	explicit ProfileScope(const char* n) : name(n), start(std::chrono::steady_clock::now()) { }
	~ProfileScope()
	{
		std::chrono::duration<double, std::milli> d = std::chrono::steady_clock::now() - start;
		CpuProfiler::record(name, d.count());
	}
	const char* name;
	std::chrono::steady_clock::time_point start;
};

#define PROFILE_CONCAT_(a, b) a##b
#define PROFILE_CONCAT(a, b) PROFILE_CONCAT_(a, b)
// name must be a string literal
#define PROFILE_SCOPE(name) \
	ZoneScopedN(name);      \
	ProfileScope PROFILE_CONCAT(profileScope_, __LINE__)(name)
#define PROFILE_FRAME() FrameMark

#endif
