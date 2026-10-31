#include "main.h"

#include "handlers/autonomousController.hpp"
#include "handlers/cameraController.hpp"
#include "handlers/calibrationController.hpp"
#include "handlers/detail/loopTiming.hpp"
#include "handlers/driverController.hpp"
#include "handlers/gamepadController.hpp"
#include "handlers/mechanismController.hpp"
#include "handlers/motorController.hpp"
#include "handlers/runtimeController.hpp"
#include "handlers/screenController.hpp"
#include "handlers/supportController.hpp"

static void driverLoop();
static void idleLoop();

constexpr int CAMERA_REFERENCE_TAG = 0;
constexpr float CAMERA_REFERENCE_WIDTH_MM = 0; // Measure before enabling depth.
constexpr float CAMERA_REFERENCE_Z_MM = 0;
constexpr int CLAW_PORT = -11, ELEVATOR_PORT = -5;
constexpr int CLAW_GEAR_RATIO = 18;
constexpr int CLAW_TEST_POWER = 45, ELEVATOR_TEST_POWER = 50;
constexpr double CLAW_DEGREES_PER_100 = 360, ELEVATOR_DEGREES_PER_100 = 360;

void initialize() {
	runtimeInit();
	clearScreen();
	writeScreenLarge("rogue", 10, 10);

	debugStep("start");

	debugStep("motor ports");
	motorDefinePort(DriveSide::Left, 10);
	motorDefinePort(DriveSide::Right, -9);
	motorSetMaxRpm(200);

	debugStep("motor init");
	if (motorInit() == 0) {
		writeScreen("drive ok  %d + %d motors", 10, 70, motorCount(DriveSide::Left), motorCount(DriveSide::Right));
	} else {
		runtimeError("check drive motors");
	}

	debugStep("mechanism init");
	mechanismDefinePort(Mechanism::Claw, CLAW_PORT);
	mechanismDefinePort(Mechanism::Elevator, ELEVATOR_PORT);
	mechanismSetGearRatio(Mechanism::Claw, CLAW_GEAR_RATIO);
	mechanismSetScale(Mechanism::Claw, CLAW_DEGREES_PER_100);
	mechanismSetScale(Mechanism::Elevator, ELEVATOR_DEGREES_PER_100);
	mechanismSetTestPower(Mechanism::Claw, CLAW_TEST_POWER);
	mechanismSetTestPower(Mechanism::Elevator, ELEVATOR_TEST_POWER);
	printf("mechanism: %d missing\n", mechanismInit());

	debugStep("pad port");
	controllerDefinePort(1);
	debugStep("pad init");
	gamepadInit();

	debugStep("camera init");
	cameraDefinePort(CAMERA_AUTO_PORT);
	cameraSetTagFamily(CameraFamily::Tag16H5);
	if (CAMERA_REFERENCE_WIDTH_MM > 0) {
		cameraDefineTarget(CameraKind::Tag, CAMERA_REFERENCE_TAG, CAMERA_REFERENCE_WIDTH_MM);
		if (CAMERA_REFERENCE_Z_MM > 0)
			cameraSetCalibrationReference(CameraKind::Tag, CAMERA_REFERENCE_TAG, CAMERA_REFERENCE_Z_MM);
	}
	cameraInit();

	runtimeIdle("ready; waiting for mode");
}

void disabled() {
	debugStep("disabled", 0);
	calibrationCancel();
	mechanismStop();
	driveStop();
	idleLoop();
}

void competition_initialize() {
	debugStep("comp init", 0);
	calibrationCancel();
	mechanismStop();
	driveStop();
	idleLoop();
}

void autonomous() {
	autonomousRun();
}

static void drawStatusPage() {
	clearScreen();
	writeScreenLarge("rogue", 10, 10);
	writeScreen("tap screen: mechanisms", 10, 70);
}

static int debug_page = 3; // Inputs, camera, status, mechanisms.

static void drawDebugPage(bool full) {
	static handler_detail::FrameClock frame_clock;
	if (debug_page == 1) { drawCameraScreen(full); return; }
	if (!frame_clock.due(pros::millis(), SCREEN_FRAME_MS, full)) return;
	if (debug_page == 0) drawInputScreen(full);
	else if (debug_page == 3) drawMechanismScreen(full);
	else {
		if (full) drawStatusPage();
		writeScreen("mode: %s", 250, 190, competitionText());
		writeScreen("L %4d   R %4d", 10, 130, driveLeftPower(), driveRightPower());
		writeScreen("cap %3d%%", 250, 130, driverSpeedPercent());
		writeScreen("pad %s", 10, 165, gamepadLinkText());
		writeScreen("trim %.2f %.2f", 250, 165, motorTrim(DriveSide::Left), motorTrim(DriveSide::Right));
		writeScreen("up %.1f s", 10, 190, pros::millis() / 1000.0);
	}
}

static void nextDebugPage() {
	debug_page = (debug_page + 1) % 4;
	drawDebugPage(true);
}

static void idleLoop() {
	cameraSetMoving(false);
	runtimeStep("waiting for enable");
	drawDebugPage(true);
	while (true) {
		const auto start = pros::millis();
		runtimeHeartbeat();
		if (screenTapped()) nextDebugPage();
		mechanismRefresh();
		logInputChanges();
		drawDebugPage(false);
		sleep_ms(handler_detail::remainingDelay(start, pros::millis(), 20));
	}
}

static void driverLoop() {
	calibrationCancel();
	driverReset();
	bool calibration_page = false;
	int loops = 0;
	drawDebugPage(true);

	while (true) {
		const auto start = pros::millis();
		runtimeHeartbeat();

		if (screenTapped() && !calibrationActive()) {
			if (calibration_page) { calibration_page = false; drawDebugPage(true); }
			else nextDebugPage();
		}
		logInputChanges();
		driverUpdateOptions();

		if (gamepadPressed(GamepadButton::B)) {
			if (calibrationActive()) calibrationCancel();
			else calibrationStart(CalibrationOrigin::Driver);
			calibration_page = true;
			drawCalibrationScreen(true);
		}

		calibrationUpdate();
		if (!calibrationActive()) driverUpdate();
		if (calibration_page) drawCalibrationScreen(false);
		else drawDebugPage(false);

		if (loops % 50 == 0) {
			printf("rogue: loop %d, %lu ms, mode %s, bits 0x%02x, pad %s\n", loops, (unsigned long)pros::millis(),
			       competitionText(), competitionBits(), gamepadLinkText());
		}
		loops++;
		sleep_ms(handler_detail::remainingDelay(start, pros::millis(), 20));
	}
}

void opcontrol() {
	debugStep("opcontrol", 0);
	driverLoop();
}
