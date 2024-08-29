#pragma once
#include "Containers/ConcurrentObject.h"
#include <cstdint>
#include <cstdio>
#include <exception>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#ifdef __GNUG__
#define FUNCTION_NAME __PRETTY_FUNCTION__
#else
#define FUNCTION_NAME __func__
#endif
#define PANIC(...)            \
	{                         \
		SDL_Log(__VA_ARGS__); \
		std::abort();         \
	}

// Languages:
// 1 - English
// 2 - Chinese

#define LANGUAGE 1
// #define LANGUAGE 2

#define LOCK(x) \
	std::lock_guard<std::mutex> lock_##x{x};