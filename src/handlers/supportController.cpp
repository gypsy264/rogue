#include "handlers/supportController.hpp"

#include "handlers/gamepadController.hpp"
#include "handlers/driverController.hpp"
#include "handlers/motorController.hpp"
#include "handlers/runtimeController.hpp"
#include "handlers/screenController.hpp"
#include "main.h"

void sleep_ms(std::uint32_t ms) {
#ifdef ROGUE_SIM
	const std::uint32_t start = pros::millis();
	while (pros::millis() - start < ms) {}
#else
	pros::delay(ms);
#endif
}

const char* competitionText() {
	static char text[20];
	const char* mode = pros::competition::is_disabled() ? "off"
	                   : pros::competition::is_autonomous() ? "auto"
	                                                        : "drive";
	const char* source = pros::competition::is_field_control()       ? "field"
	                     : pros::competition::is_competition_switch() ? "switch"
	                                                                  : "none";
	std::snprintf(text, sizeof text, "%s %s", mode, source);
	return text;
}

unsigned competitionBits() { return pros::competition::get_status(); }

void debugStep(const char* step, std::uint32_t hold_ms) {
	runtimeStep(step);
	printf("rogue: step %s (%lu ms)\n", step, (unsigned long)pros::millis());

	if (hold_ms) sleep_ms(hold_ms);
}

namespace {

constexpr GamepadButton ALL_BUTTONS[] = {
	GamepadButton::L1, GamepadButton::L2, GamepadButton::R1,    GamepadButton::R2,
	GamepadButton::Up, GamepadButton::Down, GamepadButton::Left, GamepadButton::Right,
	GamepadButton::X,  GamepadButton::Y,  GamepadButton::A,     GamepadButton::B,
};

struct InputState {
	bool held[12];
	int lx, ly, rx, ry;
};

InputState read_inputs() {
	InputState in{};
	for (int i = 0; i < 12; i++) in.held[i] = gamepadHeld(ALL_BUTTONS[i]);
	in.lx = gamepadRawAxis(GamepadAxis::LeftX);
	in.ly = gamepadRawAxis(GamepadAxis::LeftY);
	in.rx = gamepadRawAxis(GamepadAxis::RightX);
	in.ry = gamepadRawAxis(GamepadAxis::RightY);
	return in;
}

}

void drawInputScreen(bool full) {

	constexpr int BTN_X = 10, BTN_Y = 30, BTN_W = 50, BTN_H = 30, BTN_GAP = 6;
	constexpr int STICK_R = 42, LSTICK_X = 300, RSTICK_X = 420, STICK_Y = 90;

	static InputState last{};
	static bool drawn = false;
	const InputState in = read_inputs();

	if (full) {
		clearScreen();
		drawn = false;
	}

	for (int i = 0; i < 12; i++) {
		if (drawn && in.held[i] == last.held[i]) continue;
		const int x = BTN_X + (i % 4) * (BTN_W + BTN_GAP);
		const int y = BTN_Y + (i / 4) * (BTN_H + BTN_GAP);
		drawButton(gamepadButtonName(ALL_BUTTONS[i]), x, y, BTN_W, BTN_H, in.held[i]);
	}
	if (!drawn || in.lx != last.lx || in.ly != last.ly) drawStick(LSTICK_X, STICK_Y, STICK_R, in.lx, in.ly);
	if (!drawn || in.rx != last.rx || in.ry != last.ry) drawStick(RSTICK_X, STICK_Y, STICK_R, in.rx, in.ry);

	writeScreenSmall("INPUT  pad %-5s  mode %-10s  tap: camera", 10, 5, gamepadLinkText(), competitionText());
	writeScreenSmall("L stick %4d %4d    R stick %4d %4d", 10, 150, in.lx, in.ly, in.rx, in.ry);
	writeScreenSmall("motor cmd L %4d R %4d  cap %d%%", 10, 175, driveLeftPower(), driveRightPower(), driverSpeedPercent());
	writeScreenSmall("motor real L %4.0f  R %4.0f rpm", 10, 200, motorMeasuredRpm(DriveSide::Left),
	                 motorMeasuredRpm(DriveSide::Right));

	last = in;
	drawn = true;
}

void logInputChanges() {
	static InputState last{};
	static bool first = true;
	const InputState in = read_inputs();

	bool changed = first || in.lx != last.lx || in.ly != last.ly || in.rx != last.rx || in.ry != last.ry;
	for (int i = 0; i < 12 && !changed; i++) changed = in.held[i] != last.held[i];
	first = false;
	last = in;
	if (!changed) return;

	char buttons[64] = "";
	for (int i = 0; i < 12; i++) {
		if (!in.held[i]) continue;
		const size_t used = std::strlen(buttons);
		std::snprintf(buttons + used, sizeof buttons - used, "%s ", gamepadButtonName(ALL_BUTTONS[i]));
	}
	printf("rogue: input %s| LX %d LY %d RX %d RY %d\n", buttons[0] ? buttons : "- ", in.lx, in.ly, in.rx, in.ry);
}
