#include "handlers/gamepadController.hpp"

#include "main.h"

namespace {

pros::Controller master(pros::E_CONTROLLER_MASTER);
int deadzone_value = 5;

int read_stick(pros::controller_analog_e_t channel) {
	const int value = master.get_analog(channel);
	if (value > -deadzone_value && value < deadzone_value) return 0;
	return value;
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

}  // namespace

void gamepadInit(int deadzone) {
	deadzone_value = deadzone < 0 ? 0 : deadzone;
	printf("gamepad: %s, deadzone %d\n", gamepadConnected() ? "connected" : "not connected", deadzone_value);
}

bool gamepadConnected() { return master.is_connected() == 1; }

int gamepadLeftX()  { return read_stick(pros::E_CONTROLLER_ANALOG_LEFT_X); }
int gamepadLeftY()  { return read_stick(pros::E_CONTROLLER_ANALOG_LEFT_Y); }
int gamepadRightX() { return read_stick(pros::E_CONTROLLER_ANALOG_RIGHT_X); }
int gamepadRightY() { return read_stick(pros::E_CONTROLLER_ANALOG_RIGHT_Y); }

bool gamepadHeld(GamepadButton button) { return master.get_digital(to_pros(button)) == 1; }

bool gamepadPressed(GamepadButton button) { return master.get_digital_new_press(to_pros(button)) == 1; }

void gamepadRumble(const char* pattern) { master.rumble(pattern); }
