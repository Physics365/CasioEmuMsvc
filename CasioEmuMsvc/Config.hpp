#pragma once
#include "ConcurrentObject.h"
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
#define ENABLE_CRASH_CHECK
#ifdef ENABLE_CRASH_CHECK
#define PANIC(...)           \
	{                        \
		printf(__VA_ARGS__); \
		__debugbreak();      \
	}
#else
#define PANIC(...) 0;
#endif

// Languages:
// 1 - English
// 2 - Chinese

#define LANGUAGE 1
// #define LANGUAGE 2

#define LOCK(x) \
	std::lock_guard<std::mutex> lock_##x{x};

// Enable debug feature

#define DBG