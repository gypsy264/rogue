#include "handlers/calibrationController.hpp"

#include <algorithm>
#include <cmath>

#include "handlers/cameraController.hpp"
#include "handlers/gamepadController.hpp"
#include "handlers/mechanismController.hpp"
#include "handlers/motorController.hpp"
#include "handlers/runtimeController.hpp"
#include "main.h"

namespace {
CalibrationState state;
CalibrationOrigin origin = CalibrationOrigin::Driver;
std::uint32_t started = 0, phase_started = 0, last_sample = 0, camera_request = 0;
double left_sum = 0, right_sum = 0, left_trim = 1, right_trim = 1;
int bad_samples = 0;

bool modeAllowed() {
	return !pros::competition::is_disabled() &&
	       bool(pros::competition::is_autonomous()) == (origin == CalibrationOrigin::Autonomous) &&
	       (origin == CalibrationOrigin::Autonomous || gamepadConnected());
}

void stop() {
	driveStop();
	mechanismStop();
	cameraSetMoving(false);
}

void finish(CalibrationPhase phase) {
	stop();
	cameraCancelCalibration();
	if ((phase == CalibrationPhase::Done || phase == CalibrationPhase::Partial) && state.drive == CalibrationPart::Ready) {
		motorSetTrim(DriveSide::Left, left_trim);
		motorSetTrim(DriveSide::Right, right_trim);
	}
	state.phase = phase;
	state.elapsedMs = pros::millis() - started;
	runtimeStep(phase == CalibrationPhase::Done ? "cal: done" : phase == CalibrationPhase::Partial ? "cal: partial, see screen"
	            : phase == CalibrationPhase::TimedOut ? "cal: timeout, stopped" : "cal: cancelled, stopped");
	printf("cal: result=%d drive=%d mechanisms=%d camera=%d elapsed=%lums\n", int(phase), int(state.drive),
	       int(state.mechanisms), int(state.camera), (unsigned long)state.elapsedMs);
}

void complete() {
	finish(state.drive == CalibrationPart::Ready && state.mechanisms == CalibrationPart::Ready &&
	       state.camera != CalibrationPart::Failed ? CalibrationPhase::Done : CalibrationPhase::Partial);
}

void rest(std::uint32_t now) {
	stop();
	state.phase = CalibrationPhase::Resting;
	phase_started = now;
	runtimeStep("cal: stopped, settling");
}
}

bool calibrationActive() {
	return state.phase == CalibrationPhase::Settling || state.phase == CalibrationPhase::Measuring ||
	       state.phase == CalibrationPhase::Resting || state.phase == CalibrationPhase::Camera;
}

bool calibrationStart(CalibrationOrigin value) {
	calibrationCancel();
	origin = value;
	if (!modeAllowed()) return false;
	state = {};
	started = phase_started = last_sample = pros::millis();
	left_sum = right_sum = 0;
	bad_samples = 0;
	camera_request = 0;
	state.phase = CalibrationPhase::Settling;
	runtimeStep("cal: drive settling 1s");
	return true;
}

void calibrationCancel() {
	if (calibrationActive()) finish(CalibrationPhase::Cancelled);
	else { stop(); cameraCancelCalibration(); }
}

void calibrationUpdate() {
	if (!calibrationActive()) return;
	const auto now = pros::millis();
	state.elapsedMs = now - started;
	if (!modeAllowed()) { finish(CalibrationPhase::Cancelled); return; }
	if (state.elapsedMs >= CALIBRATION_BUDGET_MS) { finish(CalibrationPhase::TimedOut); return; }
	if (state.phase == CalibrationPhase::Settling || state.phase == CalibrationPhase::Measuring) {
		if (!driveReady()) { state.drive = CalibrationPart::Failed; rest(now); return; }
		if (state.phase == CalibrationPhase::Settling) {
			if (now - phase_started >= 1000) {
				state.phase = CalibrationPhase::Measuring;
				phase_started = now;
				runtimeStep("cal: drive measuring 2s");
			}
		} else if (now - last_sample >= 20) {
			last_sample = now;
			const double left = motorMeasuredRpm(DriveSide::Left), right = motorMeasuredRpm(DriveSide::Right);
			if (!std::isfinite(left) || !std::isfinite(right) || left < 5 || right < 5) {
				if (++bad_samples >= 3) { state.drive = CalibrationPart::Failed; rest(now); return; }
			} else {
				bad_samples = 0;
				left_sum += left;
				right_sum += right;
				++state.samples;
			}
			if (now - phase_started >= 2000) {
				if (state.samples >= 20) {
					state.leftRpm = left_sum / state.samples;
					state.rightRpm = right_sum / state.samples;
					const double slower = std::min(state.leftRpm, state.rightRpm);
					left_trim = slower / state.leftRpm;
					right_trim = slower / state.rightRpm;
					state.drive = CalibrationPart::Ready;
				} else state.drive = CalibrationPart::Failed;
				rest(now);
				return;
			}
		}
		driveTank(CALIBRATION_SPIN_POWER, -CALIBRATION_SPIN_POWER, false);
		return;
	}
	if (state.phase == CalibrationPhase::Resting) {
		if (now - phase_started < 500) return;
		mechanismRefresh();
		const bool claw = mechanismZero(Mechanism::Claw);
		const bool lift = mechanismZero(Mechanism::Elevator);
		state.mechanisms = claw && lift ? CalibrationPart::Ready : CalibrationPart::Failed;
		camera_request = cameraCalibrate();
		if (!camera_request) { state.camera = CalibrationPart::Skipped; complete(); return; }
		state.phase = CalibrationPhase::Camera;
		runtimeStep("cal: waiting camera reference");
	}
	if (state.phase == CalibrationPhase::Camera) {
		CameraSnapshot snapshot;
		if (!cameraSnapshot(snapshot)) return;
		if (snapshot.status == CameraStatus::Offline || snapshot.status == CameraStatus::Error ||
		    snapshot.status == CameraStatus::Multiple || snapshot.status == CameraStatus::Simulator ||
		    now - snapshot.updatedMs > 1500) {
			state.camera = CalibrationPart::Failed;
			complete();
			return;
		}
		if (snapshot.calibrationRequest != camera_request) return;
		if (snapshot.calibration == CameraCalibration::Ready) {
			state.camera = CalibrationPart::Ready;
			complete();
		} else if (snapshot.calibration == CameraCalibration::TimedOut) {
			state.camera = CalibrationPart::Failed;
			complete();
		}
	}
}

CalibrationState calibrationState() { return state; }
