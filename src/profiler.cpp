#include "profiler.h"

#include <cstring>

namespace
{
	std::vector<CpuProfiler::Entry>& storage()
	{
		static std::vector<CpuProfiler::Entry> e;
		return e;
	}
}

void CpuProfiler::record(const char* name, double ms)
{
	for (Entry& e : storage())
	{
		if (e.name == name || std::strcmp(e.name, name) == 0)
		{
			e.ms = e.ms * 0.95 + ms * 0.05; // exponential moving average
			return;
		}
	}
	storage().push_back({name, ms});
}

const std::vector<CpuProfiler::Entry>& CpuProfiler::entries()
{
	return storage();
}
