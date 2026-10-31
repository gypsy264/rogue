#include "handlers/calibrationController.hpp"

#include "handlers/detail/loopTiming.hpp"
#include "handlers/driverController.hpp"
#include "handlers/screenController.hpp"
#include "main.h"

namespace {
const char* phaseName(CalibrationPhase phase) {
	switch (phase) {
		case CalibrationPhase::Idle: return "ready to calibrate";
		case CalibrationPhase::Settling: return "drive: settling";
		case CalibrationPhase::Measuring: return "drive: measuring";
		case CalibrationPhase::Resting: return "stopping / zeroing";
		case CalibrationPhase::Camera: return "camera: center reference";
		case CalibrationPhase::Done: return "calibration complete";
		case CalibrationPhase::Partial: return "partial - check results";
		case CalibrationPhase::Cancelled: return "cancelled / stopped";
		case CalibrationPhase::TimedOut: return "timeout / stopped";
	}
	return "unknown";
}
const char* partName(CalibrationPart part) {
	switch (part) {
		case CalibrationPart::Pending: return "pending";
		case CalibrationPart::Ready: return "OK";
		case CalibrationPart::Skipped: return "skipped: no reference";
		case CalibrationPart::Failed: return "failed";
	}
	return "unknown";
}
}

void drawCalibrationScreen(bool full, bool autonomous) {
	static handler_detail::FrameClock frames;
	if (!frames.due(pros::millis(), SCREEN_FRAME_MS, full)) return;
	if (full) {
		clearScreen();
		writeScreenLarge(autonomous ? "Autonomous" : "Calibrate", 10, 3);
	}
	const auto state = calibrationState();
	writeScreen("%s", 10, 49, phaseName(state.phase));
	writeScreenSmall("time %.1f / %lus  B: %s", 10, 79, state.elapsedMs / 1000.0,
	                 static_cast<unsigned long>(CALIBRATION_BUDGET_MS / 1000),
	                 calibrationActive() ? "cancel" : autonomous ? "-" : "calibrate");
	writeScreenSmall("drive: %s  samples %d", 10, 102, partName(state.drive), state.samples);
	writeScreenSmall("L %.1f  R %.1f rpm", 10, 123, state.leftRpm, state.rightRpm);
	writeScreenSmall("claw/lift zero: %s", 10, 144, partName(state.mechanisms));
	writeScreenSmall("camera: %s", 10, 165, partName(state.camera));
	writeScreenSmall("%s", 10, 195, autonomous ? "autonomous mode active" : "tap: back  L1: limit  R1: slow");
}
