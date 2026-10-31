#include "handlers/cameraController.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cmath>

#include "handlers/detail/cameraTracking.hpp"
#include "handlers/detail/loopTiming.hpp"
#include "handlers/supportController.hpp"
#include "main.h"

namespace {
constexpr std::uint32_t POLL_MS = 10;
camera_detail::Tracker tracker;
handler_detail::RateCounter poll_rate;
CameraSnapshot published;
std::array<pros::aivision_color_s_t, 7> colors{};
CameraFamily family = CameraFamily::Tag16H5;
int configured_port = CAMERA_AUTO_PORT;
int active_port = 0;
bool started = false, has_reference = false;
std::atomic<bool> moving{false};
std::atomic<std::uint32_t> calibration_request{0}, next_request{0};
std::uint32_t connected_at = 0, retry_at = 0;
bool retry_pending = false;

pros::Mutex& data_lock() {
	static pros::Mutex mutex;
	return mutex;
}

void publish(std::uint32_t now) {
	tracker.state.updatedMs = now;
	if (data_lock().take(5)) {
		published = tracker.state;
		data_lock().give();
	}
}

void failure(CameraStatus status, int error, std::uint32_t now) {
	tracker.state.status = status;
	tracker.state.error = error;
	tracker.update(nullptr, 0, now, moving.load());
	active_port = 0;
	retry_at = now;
	retry_pending = true;
}

int discover() {
	if (configured_port) return configured_port;
	int result = 0;
	for (int port = 1; port <= 21; ++port) {
		if (pros::c::get_plugged_type(port) != pros::c::E_DEVICE_AIVISION) continue;
		if (result) return -1;
		result = port;
	}
	return result;
}

bool connect(std::uint32_t now) {
	const int port = discover();
	if (port < 0) { failure(CameraStatus::Multiple, 0, now); return false; }
	tracker.state.port = port;
	if (!port || pros::c::get_plugged_type(port) != pros::c::E_DEVICE_AIVISION) {
		failure(CameraStatus::Offline, ENODEV, now);
		return false;
	}
	const auto tag_family = static_cast<pros::aivision_tag_family_e_t>(family);
	const auto modes = pros::E_AIVISION_MODE_TAGS | pros::E_AIVISION_MODE_COLORS;
	const auto mask = pros::E_AIVISION_MODE_ALL | pros::E_AIVISION_MODE_COLOR_MERGE;
	if (pros::c::aivision_set_tag_family_override(port, tag_family) == PROS_ERR ||
	    pros::c::aivision_set_enabled_detection_types(port, modes, mask) == PROS_ERR) {
		failure(CameraStatus::Error, errno, now);
		return false;
	}
	for (const auto& color : colors) {
		if (color.id && pros::c::aivision_set_color(port, &color) == PROS_ERR) {
			failure(CameraStatus::Error, errno, now);
			return false;
		}
	}
	if (pros::c::aivision_start_awb(port) == PROS_ERR) {
		failure(CameraStatus::Error, errno, now);
		return false;
	}
	active_port = port;
	connected_at = now;
	retry_pending = false;
	tracker.state.status = CameraStatus::Warming;
	tracker.state.error = 0;
	return true;
}

bool poll(std::uint32_t now) {
	if (!active_port) {
		tracker.lose(now);
		if (retry_pending && now - retry_at < 1000) return false;
		if (!connect(now)) return false;
	}
	if (now - connected_at < 1200) { tracker.lose(now); return false; }
	errno = 0;
	const int count = pros::c::aivision_get_object_count(active_port);
	if (count == PROS_ERR || count < 0 || count > camera_detail::FRAME_LIMIT) {
		failure(CameraStatus::Error, errno ? errno : EIO, now);
		return false;
	}
	std::array<camera_detail::Detection, camera_detail::FRAME_LIMIT> detections{};
	int valid = 0;
	for (int i = 0; i < count; ++i) {
		errno = 0;
		const auto object = pros::c::aivision_get_object(active_port, i);
		if (errno || object.type == 255) { failure(CameraStatus::Error, errno ? errno : EIO, now); return false; }
		if (camera_detail::decode(object, detections[valid])) ++valid;
	}
	tracker.state.status = CameraStatus::Ready;
	tracker.state.error = 0;
	tracker.update(detections.data(), valid, now, moving.load());
	return true;
}

void camera_task(void*) {
	CameraStatus previous_status = CameraStatus::Searching;
	CameraCalibration previous_calibration = CameraCalibration::NeedsReference;
	while (true) {
		const auto now = pros::millis();
		const auto request = calibration_request.load();
		if (request != tracker.state.calibrationRequest) {
			if (request) tracker.calibrate(now);
			else tracker.cancelCalibration();
			tracker.state.calibrationRequest = request;
		}
		if (poll(now)) tracker.state.pollHz = poll_rate.tick(pros::millis());
		else { poll_rate.reset(); tracker.state.pollHz = 0; }
		publish(pros::millis());
		if (tracker.state.status != previous_status || tracker.state.calibration != previous_calibration) {
			printf("camera: port=%d status=%d calibration=%d errno=%d focal=%.2f px\n",
			       tracker.state.port, int(tracker.state.status), int(tracker.state.calibration),
			       tracker.state.error, tracker.state.focalPx);
			if (tracker.state.calibration == CameraCalibration::Ready && previous_calibration != CameraCalibration::Ready)
				printf("camera: save with cameraSetFocalLength(%.2ff);\n", tracker.state.focalPx);
		}
		previous_status = tracker.state.status;
		previous_calibration = tracker.state.calibration;
		sleep_ms(handler_detail::remainingDelay(now, pros::millis(), POLL_MS));
	}
}
}

bool cameraDefinePort(int port) {
	if (started || port < 0 || port > 21) return false;
	configured_port = port;
	return true;
}

bool cameraSetTagFamily(CameraFamily value) {
	if (started || int(value) < 0 || int(value) > 3) return false;
	family = value;
	return true;
}

bool cameraDefineColor(int id, int red, int green, int blue, float hueRange, float saturationRange) {
	if (started || id < 1 || id > 7 || red < 0 || red > 255 || green < 0 || green > 255 ||
	    blue < 0 || blue > 255 || !std::isfinite(hueRange) || hueRange < 1 || hueRange > 40 ||
	    !std::isfinite(saturationRange) || saturationRange < 0.1f || saturationRange > 1) return false;
	colors[id - 1] = {std::uint8_t(id), std::uint8_t(red), std::uint8_t(green), std::uint8_t(blue), hueRange, saturationRange};
	return true;
}

bool cameraDefineTarget(CameraKind kind, int id, float widthMm) {
	return !started && tracker.defineTarget(kind, id, widthMm);
}

bool cameraSetCalibrationReference(CameraKind kind, int id, float distanceMm) {
	if (started || !tracker.setReference(kind, id, distanceMm)) return false;
	has_reference = true;
	return true;
}

bool cameraSetFocalLength(float pixels) { return !started && tracker.setFocal(pixels); }

void cameraInit() {
	if (started) return;
	started = true;
	tracker.state.port = configured_port;
#ifdef ROGUE_SIM
	tracker.state.status = CameraStatus::Simulator;
	publish(pros::millis());
#else
	publish(pros::millis());
	static pros::Task task(camera_task, nullptr, TASK_PRIORITY_DEFAULT - 1,
	                       TASK_STACK_DEPTH_DEFAULT, "rogue camera");
#endif
}

std::uint32_t cameraCalibrate() {
	if (!started || !has_reference) return 0;
	auto request = next_request.fetch_add(1) + 1;
	if (!request) request = next_request.fetch_add(1) + 1;
	calibration_request.store(request);
	return request;
}

void cameraCancelCalibration() { calibration_request.store(0); }

void cameraSetMoving(bool value) { moving.store(value); }

bool cameraSnapshot(CameraSnapshot& result) {
	if (!data_lock().take(5)) return false;
	result = published;
	data_lock().give();
	const auto now = pros::millis();
	if (result.status != CameraStatus::Ready || now - result.frameMs > camera_detail::FRESH_MS) result.pollHz = 0;
	for (auto& o : result.objects) {
		if (!o.track) continue;
		if (now - o.lastSeenMs > camera_detail::MEMORY_MS) { o = {}; continue; }
		if (result.status != CameraStatus::Ready || now - o.lastSeenMs > camera_detail::FRESH_MS) o.visible = false;
	}
	return true;
}

bool cameraFind(CameraKind kind, int id, CameraObject& result, std::uint32_t maxAgeMs) {
	result = {};
	CameraSnapshot snapshot;
	if (!cameraSnapshot(snapshot) || snapshot.status != CameraStatus::Ready) return false;
	const auto now = pros::millis();
	for (const auto& o : snapshot.objects) {
		if (!o.visible || o.kind != kind || o.id != id || now - o.lastSeenMs > maxAgeMs) continue;
		if (!result.track || o.width * o.height > result.width * result.height) result = o;
	}
	return result.track != 0;
}
