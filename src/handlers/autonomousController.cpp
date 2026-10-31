#include "handlers/autonomousController.hpp"
#include "handlers/cameraController.hpp"
#include "handlers/calibrationController.hpp"
#include "handlers/detail/loopTiming.hpp"
#include "handlers/gamepadController.hpp"

#include "handlers/motorController.hpp"
#include "handlers/mechanismController.hpp"
#include "handlers/runtimeController.hpp"
#include "handlers/supportController.hpp"
#include "main.h"

void autonomousRun() {
	runtimeStep("autonomous");
	mechanismStop();
	driveStop();
	cameraSetMoving(false);
	calibrationStart(CalibrationOrigin::Autonomous);
	drawCalibrationScreen(true, true);
	while (!pros::competition::is_disabled() && pros::competition::is_autonomous()) {
		const auto start = pros::millis();
		runtimeHeartbeat();
		if (gamepadPressed(GamepadButton::B) && calibrationActive()) calibrationCancel();
		calibrationUpdate();
		drawCalibrationScreen(false, true);
		sleep_ms(handler_detail::remainingDelay(start, pros::millis(), 20));
	}
	calibrationCancel();
}
