#ifndef LOG_H
#define LOG_H

#include <cstdio>

// Minimal logging to stderr. Kept as macros so call sites stay printf-style.
#define LOG_INFO(...)  do { std::fprintf(stderr, "[info]  "); std::fprintf(stderr, __VA_ARGS__); std::fputc('\n', stderr); } while (0)
#define LOG_WARN(...)  do { std::fprintf(stderr, "[warn]  "); std::fprintf(stderr, __VA_ARGS__); std::fputc('\n', stderr); } while (0)
#define LOG_ERROR(...) do { std::fprintf(stderr, "[error] "); std::fprintf(stderr, __VA_ARGS__); std::fputc('\n', stderr); } while (0)

#endif
