#include "handlers/runtimeController.hpp"

#include <atomic>
#include <cstdio>

#include "handlers/screenController.hpp"
#include "handlers/supportController.hpp"
#include "main.h"

namespace {
constexpr std::uint32_t STALL_MS = 1500;
std::atomic<const char*> stage{"startup"};
std::atomic<const char*> startup_error{nullptr};
std::atomic<std::uint32_t> last_beat{0};
std::atomic<bool> watching{false};

void display_status() {
	const auto now = pros::millis();
	const auto bits = pros::competition::get_status();
	const auto label = stage.load();
	const auto error = startup_error.load();
	const auto age = now - last_beat.load();
	const bool stalled = watching.load() && age > STALL_MS;
	const char* mode = bits & COMPETITION_DISABLED ? "DISABLED"
	                   : bits & COMPETITION_AUTONOMOUS ? "AUTO" : "DRIVER";
	char line[59];
	if (stalled) {
		std::snprintf(line, sizeof line, "STALL %.1fs | %s | %.25s", age / 1000.0, mode, label);
	} else if (error) {
		std::snprintf(line, sizeof line, "LIVE %lus | %s | ERR: %.25s", (unsigned long)(now / 1000), mode, error);
	} else {
		std::snprintf(line, sizeof line, "LIVE %lus | %s | %.25s", (unsigned long)(now / 1000), mode, label);
	}

	const bool drawn = writeScreenStatus(line, stalled || error);
	static bool was_stalled = false;
	static bool was_drawn = true;
	static unsigned last_bits = ~0u;
	static const char* last_stage = nullptr;
	static const char* last_error = nullptr;
	if (stalled != was_stalled || drawn != was_drawn || bits != last_bits || label != last_stage || error != last_error) {
		printf("runtime: %s | bits=0x%02x%s\n", line, bits, drawn ? "" : " | screen lock busy");
	}
	was_stalled = stalled;
	was_drawn = drawn;
	last_bits = bits;
	last_stage = label;
	last_error = error;
}

void monitor(void*) {
	while (true) {
		display_status();
		sleep_ms(100);
	}
}
}

void runtimeHeartbeat() { last_beat.store(pros::millis()); }

void runtimeStep(const char* label) {
	runtimeHeartbeat();
	stage.store(label);
	watching.store(true);
}

void runtimeIdle(const char* label) {
	watching.store(false);
	stage.store(label);
	runtimeHeartbeat();
}

void runtimeError(const char* label) { startup_error.store(label); }

void runtimeInit() {
	runtimeStep("startup");
	writeScreenStatus("LIVE | startup", false);
#ifndef ROGUE_SIM
	static pros::Task task(monitor, nullptr, TASK_PRIORITY_DEFAULT + 1,
	                       TASK_STACK_DEPTH_DEFAULT, "rogue diagnostics");
#else
	// QEMU timer workaround.
	writeScreenStatus("SIM | runtime watchdog unavailable", true);
#endif
}
