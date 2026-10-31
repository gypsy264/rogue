#include "handlers/driverController.hpp"

#include "handlers/cameraController.hpp"
#include "handlers/gamepadController.hpp"
#include "handlers/mechanismController.hpp"
#include "handlers/motorController.hpp"
#include "main.h"

namespace {
bool limited = true;
int percent = 80;
}

void driverReset() {
	limited = true;
	percent = 80;
	gamepadSetAxisPercent(percent);
	driveStop();
	mechanismStop();
}

void driverUpdateOptions() {
	if (gamepadConnected() && gamepadPressed(GamepadButton::L1)) limited = !limited;
	percent = gamepadConnected() && gamepadHeld(GamepadButton::R1) ? 20 : limited ? 80 : 100;
	gamepadSetAxisPercent(percent);
}

void driverUpdate() {
	if (pros::competition::is_disabled() || pros::competition::is_autonomous() || !gamepadConnected()) {
		driveStop();
		mechanismStop();
		return;
	}
	driveArcade(gamepadLeftY(), gamepadRightX(), 127 * percent / 100);
	mechanismDriverUpdate();
	cameraSetMoving(driveLeftPower() != 0 || driveRightPower() != 0 || mechanismMoving());
}

int driverSpeedPercent() { return percent; }
bool driverSpeedLimited() { return limited; }
