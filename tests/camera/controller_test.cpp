#include "main.h"
#include "handlers/cameraController.hpp"

#include <cassert>
#include <cerrno>
#include <cmath>
#include <cstring>

namespace {
bool connected = true, readError = false, invalidObject = false;
int colorWrites = 0, whiteBalance = 0;
std::uint32_t readMs = 0, sleptMs = 0;
pros::aivision_object_s_t target{};
struct NextTick {};
}

namespace pros::c {
device_type get_plugged_type(std::uint8_t port) {
	return connected && (port == 11 || port == 12) ? E_DEVICE_AIVISION : E_DEVICE_NONE;
}
std::int32_t aivision_set_tag_family_override(std::uint8_t port, aivision_tag_family_e_t family) {
	assert(port == 11 && family == pros::TAG_16H5);
	return 1;
}
std::int32_t aivision_set_enabled_detection_types(std::uint8_t, std::uint8_t bits, std::uint8_t mask) {
	assert(bits == (pros::E_AIVISION_MODE_TAGS | pros::E_AIVISION_MODE_COLORS));
	assert(mask & pros::E_AIVISION_MODE_COLOR_MERGE);
	return 1;
}
std::int32_t aivision_set_color(std::uint8_t, const aivision_color_s_t* color) {
	assert(color->id == 2);
	++colorWrites;
	return 1;
}
std::int32_t aivision_start_awb(std::uint8_t) { ++whiteBalance; return 1; }
std::int32_t aivision_get_object_count(std::uint8_t) {
	camera_test::now += readMs;
	if (!connected || readError) { errno = ENODEV; return PROS_ERR; }
	return 1;
}
aivision_object_s_t aivision_get_object(std::uint8_t, std::uint32_t) {
	if (invalidObject) { auto bad = target; bad.type = 255; return bad; }
	return target;
}
}

void sleep_ms(std::uint32_t ms) { sleptMs = ms; throw NextTick{}; }

void tick(std::uint32_t time) {
	camera_test::now = time;
	try { camera_test::task(nullptr); } catch (const NextTick&) {}
}

int main(int argc, char** argv) {
	const bool autoPort = argc > 1 && std::strcmp(argv[1], "auto") == 0;
	assert(!cameraDefinePort(22));
	assert(cameraDefinePort(autoPort ? CAMERA_AUTO_PORT : 11));
	assert(cameraSetTagFamily(CameraFamily::Tag16H5));
	assert(!cameraDefineColor(0, 200, 20, 20, 10, 0.3f));
	assert(!cameraDefineColor(2, 200, 20, 20, NAN, 0.3f));
	assert(cameraDefineColor(2, 200, 20, 20, 10, 0.3f));
	assert(cameraDefineTarget(CameraKind::Tag, 0, 100));
	assert(cameraSetCalibrationReference(CameraKind::Tag, 0, 500));
	target.type = pros::E_AIVISION_DETECTED_TAG;
	target.object.tag = {140, 100, 180, 100, 180, 140, 140, 140};
	cameraInit();
	cameraInit();
	assert(camera_test::taskCount == 1);
	assert(cameraCalibrate() != 0);
	assert(!cameraDefinePort(12));
	CameraSnapshot snapshot;
	CameraObject object;
	assert(!cameraFind(CameraKind::Tag, 0, object));
	tick(0);
	assert(cameraSnapshot(snapshot));
	if (autoPort) {
		assert(snapshot.status == CameraStatus::Multiple && whiteBalance == 0);
		puts("camera multiple-device test passed");
		return 0;
	}
	assert(snapshot.status == CameraStatus::Warming);
	assert(snapshot.pollHz == 0 && sleptMs == 10);
	assert(colorWrites == 1 && whiteBalance == 1);
	for (unsigned now = 10; now <= 2300; now += 10) tick(now);
	assert(cameraSnapshot(snapshot));
	assert(snapshot.calibration == CameraCalibration::Ready);
	assert(std::fabs(snapshot.pollHz - 100) < 0.1f);
	assert(cameraFind(CameraKind::Tag, 0, object));
	assert(object.depthValid && std::fabs(object.zMm - 500) < 0.1f);
	const auto track = object.track;
	camera_test::now += 300;
	assert(!cameraFind(CameraKind::Tag, 0, object, 5000));
	assert(cameraSnapshot(snapshot) && snapshot.objects[0].track == track && !snapshot.objects[0].visible);
	assert(snapshot.pollHz == 0);

	camera_test::lockBusy = true;
	assert(!cameraSnapshot(snapshot));
	assert(!cameraFind(CameraKind::Tag, 0, object));
	camera_test::lockBusy = false;
	readError = true;
	tick(2700);
	assert(cameraSnapshot(snapshot) && snapshot.status == CameraStatus::Error && snapshot.error == ENODEV);
	assert(snapshot.pollHz == 0);
	assert(!cameraFind(CameraKind::Tag, 0, object));
	readError = false;
	connected = false;
	tick(3700);
	assert(cameraSnapshot(snapshot) && snapshot.status == CameraStatus::Offline);
	connected = true;
	tick(4700);
	assert(cameraSnapshot(snapshot) && snapshot.status == CameraStatus::Warming);
	assert(snapshot.pollHz == 0);
	tick(5900);
	assert(cameraFind(CameraKind::Tag, 0, object) && object.depthValid);
	assert(whiteBalance == 2 && colorWrites == 2);
	invalidObject = true;
	tick(5950);
	assert(cameraSnapshot(snapshot) && snapshot.error == EIO && snapshot.status == CameraStatus::Error);
	assert(!cameraFind(CameraKind::Tag, 0, object));
	invalidObject = false;
	tick(6950);
	tick(8150);
	assert(cameraCalibrate());
	cameraSetMoving(true);
	for (unsigned now = 8200; now <= 9000; now += 50) tick(now);
	assert(cameraSnapshot(snapshot) && snapshot.samples == 0);
	cameraSetMoving(false);
	for (unsigned now = 9050; now <= 10100; now += 50) tick(now);
	assert(cameraSnapshot(snapshot) && snapshot.calibration == CameraCalibration::Ready);
	readMs = 4;
	tick(10110);
	assert(sleptMs == 6);
	readMs = 14;
	tick(10120);
	assert(sleptMs == 1);
	const auto request = cameraCalibrate();
	tick(10150);
	assert(cameraSnapshot(snapshot) && snapshot.calibrationRequest == request);
	assert(snapshot.calibration == CameraCalibration::Waiting);
	cameraCancelCalibration();
	tick(10170);
	assert(cameraSnapshot(snapshot) && snapshot.calibrationRequest == 0);
	assert(snapshot.calibration == CameraCalibration::Ready && snapshot.focalPx > 0);
	cameraCalibrate();
	cameraCancelCalibration();
	tick(10200);
	assert(cameraSnapshot(snapshot) && snapshot.calibrationRequest == 0);
	puts("camera service tests passed");
}
