#pragma once
#include <cstdint>
#include <cstdio>
#include "pros/ai_vision.h"

#define PROS_ERR INT32_MAX
constexpr int TASK_PRIORITY_DEFAULT = 8;
constexpr int TASK_STACK_DEPTH_DEFAULT = 8192;

namespace camera_test {
inline std::uint32_t now = 0;
inline void (*task)(void*) = nullptr;
inline int taskCount = 0;
inline bool lockBusy = false;
}

namespace pros {
inline std::uint32_t millis() { return camera_test::now; }
struct Mutex {
	bool take(std::uint32_t) { return !camera_test::lockBusy; }
	void give() {}
};
struct Task {
	Task(void (*function)(void*), void*, int, int, const char*) {
		camera_test::task = function;
		++camera_test::taskCount;
	}
};
namespace c {
enum device_type { E_DEVICE_NONE = 0, E_DEVICE_AIVISION = 29 };
device_type get_plugged_type(std::uint8_t port);
}
}
