#include "main.h"
#include "handlers/calibrationController.hpp"
#include "handlers/cameraController.hpp"
#include "handlers/driverController.hpp"
#include "handlers/gamepadController.hpp"
#include "handlers/mechanismController.hpp"
#include "handlers/motorController.hpp"
#include "handlers/screenController.hpp"

#include <limits>
#include <string>

namespace {
CameraSnapshot camera;
bool cameraReference = false, cameraBusy = false, tap = false;
std::uint32_t request = 0, nextRequest = 0;
unsigned cameraStarts = 0, beats = 0;
std::string runtimeLabel;
struct StopLoop {};
}

void runtimeInit() {}
void runtimeStep(const char* label) { runtimeLabel = label; }
void runtimeIdle(const char* label) { runtimeLabel = label; }
void runtimeError(const char*) {}
void runtimeHeartbeat() { ++beats; }
void clearScreen() {}
void showError(const char*) {}
bool screenTapped() { bool value = tap; tap = false; return value; }
void drawScreenBox(int, int, int, int) {}
void drawScreenPreview(int, int, int, int, const ScreenBox*, size_t) {}
void drawButton(const char*, int, int, int, int, bool) {}
void drawStick(int, int, int, int, int) {}
void writeScreenText(const char* text, int x, int y, ScreenFont font) {
	const int w = font == ScreenFont::Small ? 8 : font == ScreenFont::Large ? 24 : 12;
	const int h = font == ScreenFont::Small ? 16 : font == ScreenFont::Large ? 40 : 22;
	assert(x >= 0 && x + int(std::strlen(text)) * w <= 480 && y >= 0 && y + h < 218);
}
void cameraInit() {}
bool cameraDefinePort(int port) { assert(port == CAMERA_AUTO_PORT); return true; }
bool cameraSetTagFamily(CameraFamily) { return true; }
bool cameraDefineTarget(CameraKind, int, float) { return true; }
bool cameraSetCalibrationReference(CameraKind, int, float) { return true; }
void cameraSetMoving(bool) {}
std::uint32_t cameraCalibrate() { ++cameraStarts; return request = cameraReference ? ++nextRequest : 0; }
void cameraCancelCalibration() { request = 0; }
bool cameraSnapshot(CameraSnapshot& result) { result = camera; return !cameraBusy; }

void neutral() {
	match_test::analog.fill(0);
	match_test::buttons.fill(0);
	match_test::previous.fill(false);
	match_test::connected = 1;
}
void stopped() {
	assert(driveLeftPower() == 0 && driveRightPower() == 0);
	assert(mechanism_test::motors[11].power == 0 && mechanism_test::motors[5].power == 0);
}
void tick(std::uint32_t step = 20) {
	match_test::now += step;
	camera.updatedMs = match_test::now;
	calibrationUpdate();
}
void runCalibration() {
	for (unsigned n = 0; calibrationActive() && n < 800; ++n) {
		tick();
		const auto phase = calibrationState().phase;
		if (phase == CalibrationPhase::Settling || phase == CalibrationPhase::Measuring) {
			assert(driveLeftPower() == 30 && driveRightPower() == -30);
			assert(std::abs(match_test::commands[10]) == 47 && std::abs(match_test::commands[9]) == 47);
		}
	}
	assert(!calibrationActive());
	assert(calibrationState().elapsedMs <= CALIBRATION_BUDGET_MS);
	stopped();
}
void seedTrims() { motorSetTrim(DriveSide::Left, 0.7); motorSetTrim(DriveSide::Right, 0.8); }
void oldTrims() { assert(motorTrim(DriveSide::Left) == 0.7 && motorTrim(DriveSide::Right) == 0.8); }
void oneLoop(void (*callback)()) {
	match_test::onDelay = [](std::uint32_t) { throw StopLoop{}; };
	try { callback(); } catch (const StopLoop&) {}
	match_test::onDelay = {};
}

int main() {
	using namespace match_test;
	using namespace pros;
	measured[10] = 45;
	measured[9] = 48;
	camera.status = CameraStatus::Ready;
	initialize();
	assert(mechanismState(Mechanism::Claw).port == -11);
	assert(mechanismState(Mechanism::Elevator).port == -5);
	driverReset();
	analog.fill(127);
	driverUpdateOptions();
	assert(gamepadLeftX() == 101 && gamepadLeftY() == 101 && gamepadRightX() == 101 && gamepadRightY() == 101);
	driverUpdate();
	assert(driveLeftPower() == 101 && driveRightPower() == 0 && std::abs(commands[10]) <= 160);
	buttons[E_CONTROLLER_DIGITAL_L1] = 1;
	driverUpdateOptions();
	assert(!driverSpeedLimited() && driverSpeedPercent() == 100);
	for (int n = 0; n < 20; ++n) driverUpdateOptions();
	assert(!driverSpeedLimited());
	driverUpdate();
	assert(driveLeftPower() == 127);
	buttons[E_CONTROLLER_DIGITAL_R1] = 1;
	driverUpdateOptions();
	assert(gamepadLeftX() == 25 && gamepadLeftY() == 25 && gamepadRightX() == 25 && gamepadRightY() == 25);
	driverUpdate();
	assert(driveLeftPower() == 25 && std::abs(commands[10]) <= 40);
	buttons[E_CONTROLLER_DIGITAL_UP] = buttons[E_CONTROLLER_DIGITAL_LEFT] = 1;
	driverUpdate();
	assert(mechanism_test::motors[11].power == -45 && mechanism_test::motors[5].power == -50);
	buttons[E_CONTROLLER_DIGITAL_L1] = 0;
	driverUpdateOptions();
	buttons[E_CONTROLLER_DIGITAL_L1] = 1;
	driverUpdateOptions();
	assert(driverSpeedLimited() && driverSpeedPercent() == 20);
	buttons[E_CONTROLLER_DIGITAL_R1] = 0;
	driverUpdateOptions();
	assert(driverSpeedPercent() == 80);
	for (bool slow : {false, true}) {
		buttons[E_CONTROLLER_DIGITAL_R1] = slow;
		driverUpdateOptions();
		const int cap = 127 * driverSpeedPercent() / 100;
		for (int forward = -127; forward <= 127; ++forward) {
			for (int turn = -127; turn <= 127; ++turn) {
				analog[E_CONTROLLER_ANALOG_LEFT_Y] = forward;
				analog[E_CONTROLLER_ANALOG_RIGHT_X] = turn;
				driverUpdate();
				assert(std::abs(driveLeftPower()) <= cap && std::abs(driveRightPower()) <= cap);
			}
		}
	}
	connected = 0;
	driverUpdate();
	stopped();
	connected = PROS_ERR;
	driverUpdate();
	stopped();
	neutral();
	analog.fill(PROS_ERR);
	driverUpdate();
	stopped();
	neutral();
	mode = COMPETITION_DISABLED;
	assert(!calibrationStart(CalibrationOrigin::Driver));
	stopped();
	mode = 0;
	seedTrims();
	assert(calibrationStart(CalibrationOrigin::Driver));
	tick();
	assert(driveLeftPower() == 30 && driveRightPower() == -30);
	assert(std::abs(commands[10]) == 47 && std::abs(commands[9]) == 47);
	calibrationCancel();
	stopped(); oldTrims();
	assert(calibrationStart(CalibrationOrigin::Driver));
	connected = 0;
	tick();
	assert(calibrationState().phase == CalibrationPhase::Cancelled);
	stopped(); oldTrims();
	neutral();
	assert(calibrationStart(CalibrationOrigin::Driver));
	mode = COMPETITION_AUTONOMOUS;
	tick();
	assert(calibrationState().phase == CalibrationPhase::Cancelled);
	oldTrims();
	connected = 0;
	assert(calibrationStart(CalibrationOrigin::Autonomous));
	runCalibration();
	assert(calibrationState().phase == CalibrationPhase::Done);
	assert(calibrationState().camera == CalibrationPart::Skipped);
	assert(motorTrim(DriveSide::Left) == 1 && motorTrim(DriveSide::Right) == 45.0 / 48);
	seedTrims();
	assert(calibrationStart(CalibrationOrigin::Autonomous));
	measured[9] = NAN;
	runCalibration();
	assert(calibrationState().drive == CalibrationPart::Failed);
	oldTrims();
	measured[9] = 48;
	cameraReference = true;
	camera.calibration = CameraCalibration::Ready;
	camera.calibrationRequest = 0;
	assert(calibrationStart(CalibrationOrigin::Autonomous));
	while (calibrationState().phase != CalibrationPhase::Camera) tick();
	assert(calibrationActive());
	const auto pending = request;
	tick();
	assert(calibrationActive());
	camera.calibrationRequest = pending;
	tick();
	assert(calibrationState().camera == CalibrationPart::Ready);
	assert(!calibrationActive() && request == 0);
	seedTrims();
	camera.calibration = CameraCalibration::Waiting;
	assert(calibrationStart(CalibrationOrigin::Autonomous));
	runCalibration();
	assert(calibrationState().phase == CalibrationPhase::TimedOut && request == 0);
	oldTrims();
	for (const auto origin : {CalibrationOrigin::Driver, CalibrationOrigin::Autonomous}) {
		mode = origin == CalibrationOrigin::Autonomous ? COMPETITION_AUTONOMOUS : 0;
		connected = 1;
		assert(calibrationStart(origin));
		for (unsigned n = 0; n < 499; ++n) tick();
		tick(19);
		assert(calibrationActive() && request != 0 && calibrationState().elapsedMs == 9999);
		tick(1);
		assert(calibrationState().phase == CalibrationPhase::TimedOut && request == 0);
		assert(calibrationState().elapsedMs == 10000);
		stopped(); oldTrims();
	}
	connected = 0;
	assert(calibrationStart(CalibrationOrigin::Autonomous));
	tick(15000);
	assert(calibrationState().phase == CalibrationPhase::TimedOut);
	stopped(); oldTrims();
	cameraBusy = true;
	assert(calibrationStart(CalibrationOrigin::Autonomous));
	runCalibration();
	assert(calibrationState().phase == CalibrationPhase::TimedOut);
	cameraBusy = false;
	camera.status = CameraStatus::Offline;
	assert(calibrationStart(CalibrationOrigin::Autonomous));
	runCalibration();
	assert(calibrationState().phase == CalibrationPhase::Partial);
	assert(calibrationState().camera == CalibrationPart::Failed);
	camera.status = CameraStatus::Ready;
	cameraReference = false;
	now = UINT32_MAX - 1000;
	assert(calibrationStart(CalibrationOrigin::Autonomous));
	runCalibration();
	assert(calibrationState().phase == CalibrationPhase::Done);
	neutral();
	mode = 0;
	buttons[E_CONTROLLER_DIGITAL_A] = buttons[E_CONTROLLER_DIGITAL_X] = buttons[E_CONTROLLER_DIGITAL_Y] = 1;
	const auto before = cameraStarts;
	oneLoop(opcontrol);
	assert(!calibrationActive() && cameraStarts == before);
	stopped();
	neutral();
	seedTrims();
	buttons[E_CONTROLLER_DIGITAL_B] = 1;
	buttons[E_CONTROLLER_DIGITAL_UP] = buttons[E_CONTROLLER_DIGITAL_LEFT] = 1;
	const auto cancel_start = now;
	match_test::onDelay = [&](std::uint32_t) {
		const auto elapsed = now - cancel_start;
		assert(mechanism_test::motors[11].power == 0 && mechanism_test::motors[5].power == 0);
		if (elapsed < 1000) assert(calibrationActive());
		if (elapsed == 500) buttons[E_CONTROLLER_DIGITAL_B] = 0;
		if (elapsed == 1000) {
			buttons[E_CONTROLLER_DIGITAL_UP] = buttons[E_CONTROLLER_DIGITAL_LEFT] = 0;
			buttons[E_CONTROLLER_DIGITAL_B] = 1;
		}
		if (elapsed == 1040) throw StopLoop{};
	};
	try { opcontrol(); } catch (const StopLoop&) {}
	match_test::onDelay = {};
	assert(calibrationState().phase == CalibrationPhase::Cancelled);
	stopped(); oldTrims();
	mode = COMPETITION_AUTONOMOUS;
	assert(calibrationStart(CalibrationOrigin::Autonomous));
	mechanism_test::motors[10].connected = false;
	runCalibration();
	assert(calibrationState().drive == CalibrationPart::Failed);
	stopped(); oldTrims();
	mechanism_test::motors[10].connected = true;
	assert(calibrationStart(CalibrationOrigin::Autonomous));
	tick();
	mode = COMPETITION_DISABLED | COMPETITION_AUTONOMOUS;
	tick();
	stopped(); oldTrims();
	assert(calibrationState().phase == CalibrationPhase::Cancelled);
	neutral();
	mode = 0;
	buttons[E_CONTROLLER_DIGITAL_B] = 1;
	oneLoop(opcontrol);
	assert(calibrationActive());
	mode = COMPETITION_DISABLED;
	oneLoop(disabled);
	stopped();
	assert(!calibrationActive() && request == 0);
	mode = 0;
	oneLoop(opcontrol);
	assert(driverSpeedLimited() && driverSpeedPercent() == 80);
	neutral();
	mode = COMPETITION_AUTONOMOUS | COMPETITION_CONNECTED;
	const auto beginning = now;
	match_test::onDelay = [&](std::uint32_t) { if (now - beginning >= 15000) mode = COMPETITION_DISABLED; };
	autonomous();
	match_test::onDelay = {};
	assert(now - beginning == 15000 && calibrationState().phase == CalibrationPhase::Done);
	stopped();
	assert(beats > 0);
	puts("competition, calibration, all-axis scaling and exhaustive drive cap tests passed");
}
