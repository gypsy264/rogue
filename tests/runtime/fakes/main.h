#pragma once
#include <cstdint>

constexpr int TASK_PRIORITY_DEFAULT = 8;
constexpr int TASK_STACK_DEPTH_DEFAULT = 8192;
constexpr unsigned COMPETITION_DISABLED = 1;
constexpr unsigned COMPETITION_AUTONOMOUS = 2;
constexpr unsigned COMPETITION_CONNECTED = 4;

namespace test_runtime {
inline std::uint32_t now = 0;
inline unsigned mode = 0;
inline unsigned task_count = 0;
inline void (*monitor)(void*) = nullptr;
}

namespace pros {
inline std::uint32_t millis() { return test_runtime::now; }
namespace competition {
inline unsigned get_status() { return test_runtime::mode; }
}
struct Task {
	Task(void (*function)(void*), void*, int priority, int, const char*) {
		if (priority <= TASK_PRIORITY_DEFAULT) throw "watchdog must preempt driver";
		test_runtime::monitor = function;
		++test_runtime::task_count;
	}
};
}
