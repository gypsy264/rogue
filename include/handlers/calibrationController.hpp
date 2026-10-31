#pragma once

#include <cstdint>

enum class CalibrationOrigin { Driver, Autonomous };
enum class CalibrationPhase { Idle, Settling, Measuring, Resting, Camera, Done, Partial, Cancelled, TimedOut };
enum class CalibrationPart { Pending, Ready, Skipped, Failed };

struct CalibrationState {
	CalibrationPhase phase = CalibrationPhase::Idle;
	CalibrationPart drive = CalibrationPart::Pending, mechanisms = CalibrationPart::Pending, camera = CalibrationPart::Pending;
	std::uint32_t elapsedMs = 0;
	int samples = 0;
	double leftRpm = 0, rightRpm = 0;
};

constexpr std::uint32_t CALIBRATION_BUDGET_MS = 10000;
constexpr int CALIBRATION_SPIN_POWER = 30;
bool calibrationStart(CalibrationOrigin origin);
void calibrationUpdate();
void calibrationCancel();
bool calibrationActive();
CalibrationState calibrationState();
void drawCalibrationScreen(bool full, bool autonomous = false);
