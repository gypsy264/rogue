
#include <cassert>
#include <string>
#include "main.h"

namespace {
std::string status;
bool alert = false;
bool screen_busy = false;
struct NextTick {};
}

bool writeScreenStatus(const char* text, bool is_alert) {
	if (screen_busy) return false;
	status = text;
	alert = is_alert;
	return true;
}

void sleep_ms(std::uint32_t) { throw NextTick{}; }

void tick() {
	try { test_runtime::monitor(nullptr); }
	catch (const NextTick&) {}
}

bool shows(const char* text) { return status.find(text) != std::string::npos; }

#include "handlers/runtimeController.hpp"

int main() {
	runtimeInit();
	runtimeInit();
	assert(test_runtime::task_count == 1);
	tick();
	assert(shows("LIVE") && !alert);

	runtimeStep("motor init");
	test_runtime::now += 1600;
	tick();
	assert(shows("STALL") && shows("motor init") && alert);
	runtimeHeartbeat();
	tick();
	assert(shows("LIVE") && !alert);

	for (unsigned mode : {1u, 5u, 6u, 4u, 0u}) {
		test_runtime::mode = mode;
		runtimeIdle("waiting for mode");
		test_runtime::now += 10000;
		tick();
		assert(shows("LIVE") && !alert);
		assert(shows(mode & 1 ? "DISABLED" : mode & 2 ? "AUTO" : "DRIVER"));
	}

	runtimeStep("cal: measuring 2s");
	for (int i = 0; i < 150; ++i) {
		test_runtime::now += 20;
		runtimeHeartbeat();
		tick();
		assert(!alert && shows("cal:"));
	}

	runtimeError("check drive motors");
	test_runtime::mode = 1;
	tick();
	assert(shows("ERR:") && shows("DISABLED") && alert);
	test_runtime::now += 1600;
	tick();
	assert(shows("STALL") && alert);
	runtimeError(nullptr);
	runtimeHeartbeat();
	tick();
	assert(!alert);

	screen_busy = true;
	test_runtime::now += 1600;
	tick();
	screen_busy = false;
	tick();
	assert(shows("STALL") && alert);

	test_runtime::now = UINT32_MAX - 500;
	runtimeStep("opcontrol");
	test_runtime::now = 100;
	tick();
	assert(!alert);
	test_runtime::now = 1100;
	tick();
	assert(alert);
	puts("runtime monitor tests passed");
}
