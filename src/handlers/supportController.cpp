#include "handlers/supportController.hpp"

#include "handlers/gamepadController.hpp"
#include "handlers/motorController.hpp"
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
	printf("rogue: step %s (%lu ms)\n", step, (unsigned long)pros::millis());
	writeScreen("step: %s", 250, 200, step);
	char line[16];
	std::snprintf(line, sizeof line, "%-15s", step);  // pad so old text is overwritten
	gamepadPrint(0, line);
	sleep_ms(hold_ms);
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

}  // namespace

void drawInputScreen(bool full) {
	// Layout: 3 rows of 4 buttons on the left, two sticks on the right.
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

	writeScreenSmall("INPUT  pad %-5s  mode %-10s  tap: status", 10, 5, gamepadLinkText(), competitionText());
	writeScreenSmall("L stick %4d %4d    R stick %4d %4d", 10, 150, in.lx, in.ly, in.rx, in.ry);
	writeScreenSmall("motor cmd  L %4d  R %4d", 10, 175, driveLeftPower(), driveRightPower());
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

bool autoCalibrate(int power) {
	constexpr std::uint32_t SETTLE_MS = 1000;   // let the motors reach speed
	constexpr std::uint32_t MEASURE_MS = 2000;  // average over this window
	constexpr std::uint32_t STEP_MS = 20;

	printf("calibrate: start, power %d\n", power);
	writeScreen("calibrating... B cancels", 10, 70);

	// Measure with no correction, otherwise old trims hide the real difference.
	motorSetTrim(DriveSide::Left, 1.0);
	motorSetTrim(DriveSide::Right, 1.0);
	driveTank(power, -power);

	double left_sum = 0.0;
	double right_sum = 0.0;
	int samples = 0;
	const std::uint32_t start = pros::millis();
	while (pros::millis() - start < SETTLE_MS + MEASURE_MS) {
		if (gamepadPressed(GamepadButton::B)) {
			driveStop();
			printf("calibrate: cancelled\n");
			writeScreen("calibrate cancelled", 10, 70);
			return false;
		}
		if (pros::millis() - start >= SETTLE_MS) {
			left_sum += motorMeasuredRpm(DriveSide::Left);
			right_sum += motorMeasuredRpm(DriveSide::Right);
			samples++;
		}
		sleep_ms(STEP_MS);
	}
	driveStop();

	const double left_rpm = samples > 0 ? left_sum / samples : 0.0;
	const double right_rpm = samples > 0 ? right_sum / samples : 0.0;
	printf("calibrate: left %.1f rpm, right %.1f rpm, %d samples\n", left_rpm, right_rpm, samples);

	if (left_rpm < 5.0 || right_rpm < 5.0) {
		showError("calibrate: a side did not move");
		return false;
	}

	// Slow the faster side down to match the slower one.
	const double slower = left_rpm < right_rpm ? left_rpm : right_rpm;
	motorSetTrim(DriveSide::Left, slower / left_rpm);
	motorSetTrim(DriveSide::Right, slower / right_rpm);

	printf("calibrate: trims left %.3f right %.3f\n", motorTrim(DriveSide::Left), motorTrim(DriveSide::Right));
	printf("calibrate: to keep them, add in initialize():\n");
	printf("  motorSetTrim(DriveSide::Left, %.3f);\n", motorTrim(DriveSide::Left));
	printf("  motorSetTrim(DriveSide::Right, %.3f);\n", motorTrim(DriveSide::Right));
	writeScreen("cal L %.0f R %.0f rpm", 10, 70, left_rpm, right_rpm);
	return true;
}
