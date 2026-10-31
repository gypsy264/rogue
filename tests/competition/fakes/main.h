#pragma once

#include "../../mechanisms/fakes/main.h"
#include <cassert>
#include <cstring>
#include <functional>
#include <vector>

constexpr unsigned COMPETITION_DISABLED = 1, COMPETITION_AUTONOMOUS = 2, COMPETITION_CONNECTED = 4;
namespace match_test {
inline std::uint32_t now = 0;
inline unsigned mode = 0;
inline int connected = 1;
inline std::array<int, 4> analog{};
inline std::array<int, 12> buttons{};
inline std::array<bool, 12> previous{};
inline std::array<double, 22> measured{};
inline std::array<int, 22> commands{};
inline std::function<void(std::uint32_t)> onDelay;
}

namespace pros {
inline std::uint32_t millis() { return match_test::now; }
inline void delay(std::uint32_t ms) {
	match_test::now += ms;
	if (match_test::onDelay) match_test::onDelay(ms);
}
namespace competition {
inline unsigned get_status() { return match_test::mode; }
inline bool is_disabled() { return match_test::mode & COMPETITION_DISABLED; }
inline bool is_autonomous() { return match_test::mode & COMPETITION_AUTONOMOUS; }
inline bool is_field_control() { return match_test::mode & COMPETITION_CONNECTED; }
inline bool is_competition_switch() { return false; }
}
enum controller_analog_e_t { E_CONTROLLER_ANALOG_LEFT_X, E_CONTROLLER_ANALOG_LEFT_Y, E_CONTROLLER_ANALOG_RIGHT_X, E_CONTROLLER_ANALOG_RIGHT_Y };
enum controller_digital_e_t {
	E_CONTROLLER_DIGITAL_A, E_CONTROLLER_DIGITAL_B, E_CONTROLLER_DIGITAL_X, E_CONTROLLER_DIGITAL_Y,
	E_CONTROLLER_DIGITAL_UP, E_CONTROLLER_DIGITAL_DOWN, E_CONTROLLER_DIGITAL_LEFT, E_CONTROLLER_DIGITAL_RIGHT,
	E_CONTROLLER_DIGITAL_L1, E_CONTROLLER_DIGITAL_L2, E_CONTROLLER_DIGITAL_R1, E_CONTROLLER_DIGITAL_R2
};
constexpr int E_CONTROLLER_MASTER = 0;
struct Controller {
	explicit Controller(int) {}
	int is_connected() { return match_test::connected; }
	int get_analog(controller_analog_e_t axis) { return match_test::analog[axis]; }
	int get_digital(controller_digital_e_t button) { return match_test::buttons[button]; }
	int get_digital_new_press(controller_digital_e_t button) {
		bool held = match_test::buttons[button] == 1;
		bool pressed = held && !match_test::previous[button];
		match_test::previous[button] = held;
		return pressed;
	}
	void rumble(const char*) {}
	void set_text(std::uint8_t, int, const char*) {}
};
struct MotorGroup {
	std::vector<std::int8_t> ports;
	MotorGroup(const std::vector<std::int8_t>& ports, MotorGears) : ports(ports) {}
	void set_gearing_all(MotorGears) {}
	void set_brake_mode_all(MotorBrake) {}
	void move_velocity(int velocity) {
		if (velocity) assert(!competition::is_disabled());
		for (const auto port : ports) match_test::commands[std::abs(port)] = port < 0 ? -velocity : velocity;
	}
	std::vector<double> get_actual_velocity_all() {
		std::vector<double> result;
		for (const auto port : ports) result.push_back(match_test::measured[std::abs(port)]);
		return result;
	}
};
}

void initialize();
void opcontrol();
void disabled();
void autonomous();
void competition_initialize();
