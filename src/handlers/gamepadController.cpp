#include "handlers/gamepadController.hpp"

#include <memory>
#include <cerrno>
#include <algorithm>

#include "main.h"

namespace {

std::unique_ptr<pros::Controller> master;
int defined_port = CONTROLLER_WIRELESS;
int deadzone_value = 5;
int axis_percent = 100;

int checked_read(int value, const char* operation) {
	if (value != PROS_ERR) return value;
	const int error = errno;
	static bool reported = false;
	static std::uint32_t last_report = 0;
	const auto now = pros::millis();
	if (!reported || now - last_report >= 1000) {
		printf("gamepad: %s failed, errno=%d (input suppressed)\n", operation, error);
		reported = true;
		last_report = now;
	}
	return 0;
}

int read_stick(pros::controller_analog_e_t channel) {
	if (!master) return 0;
	const int value = checked_read(master->get_analog(channel), "analog");
	if (value > -deadzone_value && value < deadzone_value) return 0;
	return std::clamp(value, -127, 127) * axis_percent / 100;
}

pros::controller_digital_e_t to_pros(GamepadButton button) {
	switch (button) {
		case GamepadButton::A:     return pros::E_CONTROLLER_DIGITAL_A;
		case GamepadButton::B:     return pros::E_CONTROLLER_DIGITAL_B;
		case GamepadButton::X:     return pros::E_CONTROLLER_DIGITAL_X;
		case GamepadButton::Y:     return pros::E_CONTROLLER_DIGITAL_Y;
		case GamepadButton::Up:    return pros::E_CONTROLLER_DIGITAL_UP;
		case GamepadButton::Down:  return pros::E_CONTROLLER_DIGITAL_DOWN;
		case GamepadButton::Left:  return pros::E_CONTROLLER_DIGITAL_LEFT;
		case GamepadButton::Right: return pros::E_CONTROLLER_DIGITAL_RIGHT;
		case GamepadButton::L1:    return pros::E_CONTROLLER_DIGITAL_L1;
		case GamepadButton::L2:    return pros::E_CONTROLLER_DIGITAL_L2;
		case GamepadButton::R1:    return pros::E_CONTROLLER_DIGITAL_R1;
		default:                   return pros::E_CONTROLLER_DIGITAL_R2;
	}
}

}

void controllerDefinePort(int port) {
	if (master) {
		printf("gamepad: port %d ignored, define it before gamepadInit()\n", port);
		return;
	}
	if (port != CONTROLLER_WIRELESS && (port < 1 || port > 21)) {
		printf("gamepad: port %d is not valid, use 1 to 21 or CONTROLLER_WIRELESS\n", port);
		return;
	}
	if (port != CONTROLLER_WIRELESS && pros::c::get_plugged_type(static_cast<std::uint8_t>(port)) == pros::c::E_DEVICE_MOTOR) {
		printf("gamepad: port %d has a motor plugged in, check the cable\n", port);
	}
	defined_port = port;
}

void gamepadInit(int deadzone) {
	deadzone_value = deadzone < 0 ? 0 : deadzone;
	if (!master) master = std::make_unique<pros::Controller>(pros::E_CONTROLLER_MASTER);
	printf("gamepad: MASTER, wiring note %s", defined_port == CONTROLLER_WIRELESS ? "wireless" : "port ");
	if (defined_port != CONTROLLER_WIRELESS) printf("%d", defined_port);
	printf(", link now %s, deadzone %d\n", gamepadLinkText(), deadzone_value);
}

int link_state() {
	if (!master) return 0;
	return checked_read(master->is_connected(), "connection");
}

void gamepadSetAxisPercent(int percent) { axis_percent = std::clamp(percent, 0, 100); }

bool gamepadConnected() { return link_state() > 0; }

const char* gamepadLinkText() {
	switch (link_state()) {
		case 1:  return "cable";
		case 2:  return "radio";
		default: return "off";
	}
}

int gamepadRawAxis(GamepadAxis axis) {
	if (!master) return 0;
	pros::controller_analog_e_t channel = pros::E_CONTROLLER_ANALOG_LEFT_X;
	switch (axis) {
		case GamepadAxis::LeftX:  channel = pros::E_CONTROLLER_ANALOG_LEFT_X; break;
		case GamepadAxis::LeftY:  channel = pros::E_CONTROLLER_ANALOG_LEFT_Y; break;
		case GamepadAxis::RightX: channel = pros::E_CONTROLLER_ANALOG_RIGHT_X; break;
		case GamepadAxis::RightY: channel = pros::E_CONTROLLER_ANALOG_RIGHT_Y; break;
	}
	return checked_read(master->get_analog(channel), "raw analog");
}

const char* gamepadButtonName(GamepadButton button) {
	static const char* const names[] = {"A", "B", "X", "Y", "Up", "Dn", "Lt", "Rt", "L1", "L2", "R1", "R2"};
	return names[static_cast<int>(button)];
}

int gamepadLeftX()  { return read_stick(pros::E_CONTROLLER_ANALOG_LEFT_X); }
int gamepadLeftY()  { return read_stick(pros::E_CONTROLLER_ANALOG_LEFT_Y); }
int gamepadRightX() { return read_stick(pros::E_CONTROLLER_ANALOG_RIGHT_X); }
int gamepadRightY() { return read_stick(pros::E_CONTROLLER_ANALOG_RIGHT_Y); }

bool gamepadHeld(GamepadButton button) {
	return master && checked_read(master->get_digital(to_pros(button)), "digital") == 1;
}

bool gamepadPressed(GamepadButton button) { return master && master->get_digital_new_press(to_pros(button)) == 1; }

void gamepadRumble(const char* pattern) {
	if (master) master->rumble(pattern);
}

void gamepadPrint(int line, const char* text) {
	if (!master || line < 0 || line > 2) return;
	master->set_text(static_cast<std::uint8_t>(line), 0, text);
}
