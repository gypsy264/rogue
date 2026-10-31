#include "handlers/detail/cameraTracking.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

using camera_detail::Detection;
using camera_detail::Tracker;

Detection tag(int id, int x, int y, int side) {
	pros::aivision_object_s_t raw{};
	raw.type = pros::E_AIVISION_DETECTED_TAG;
	raw.id = id;
	raw.object.tag = {short(x), short(y), short(x + side), short(y),
	                  short(x + side), short(y + side), short(x), short(y + side)};
	Detection d;
	assert(camera_detail::decode(raw, d));
	return d;
}

Detection color(int id, int x, int y, int width, int height) {
	pros::aivision_object_s_t raw{};
	raw.type = pros::E_AIVISION_DETECTED_COLOR;
	raw.id = id;
	raw.object.color = {std::uint16_t(x), std::uint16_t(y), std::uint16_t(width), std::uint16_t(height), 0};
	Detection d;
	assert(camera_detail::decode(raw, d));
	return d;
}

int count(const Tracker& t, bool visibleOnly = false) {
	int n = 0;
	for (const auto& o : t.state.objects) if (o.track && (!visibleOnly || o.visible)) ++n;
	return n;
}

const CameraObject& find(const Tracker& t, int id, bool visible = true) {
	for (const auto& o : t.state.objects) if (o.track && o.id == id && o.visible == visible) return o;
	assert(false);
	return t.state.objects[0];
}

void close(float actual, float expected) { assert(std::fabs(actual - expected) < 0.05f); }

int main() {
	Tracker t;
	auto reference = tag(0, 140, 100, 40);
	t.update(&reference, 1, 0, false);
	assert(!find(t, 0).depthValid);
	assert(!t.defineTarget(CameraKind::Color, 0, 100));
	assert(!t.defineTarget(CameraKind::Tag, 0, std::numeric_limits<float>::quiet_NaN()));
	assert(!t.setReference(CameraKind::Tag, 0, 500));
	assert(!t.setFocal(0));
	assert(!t.setFocal(std::numeric_limits<float>::infinity()));
	assert(t.defineTarget(CameraKind::Tag, 0, 100));
	assert(t.setReference(CameraKind::Tag, 0, 500));
	assert(t.calibrate(0));
	for (unsigned now = 0; now < 1050; now += 10) t.update(&reference, 1, now, false);
	assert(t.state.calibration == CameraCalibration::Sampling);
	assert(t.state.samples == 11);
	t.update(&reference, 1, 1050, false);
	assert(t.state.calibration == CameraCalibration::Ready);
	close(t.state.focalPx, 200);
	close(find(t, 0).zMm, 500);
	close(find(t, 0).xMm, 0);
	close(find(t, 0).yMm, 0);

	auto near = tag(0, 140, 100, 80);
	t.update(&near, 1, 1100, false);
	close(find(t, 0).zMm, 250);
	close(find(t, 0).xMm, 25);
	close(find(t, 0).yMm, 25);

	Detection sameId[] = {near, color(1, 10, 30, 25, 40), tag(1, 220, 40, 40)};
	t.update(sameId, 3, 1150, false);
	assert(count(t, true) == 3);
	for (const auto& o : t.state.objects) if (o.id == 1) assert(!o.depthValid);

	const auto oldTrack = find(t, 0).track;
	t.update(nullptr, 0, 1200, false);
	assert(count(t, true) == 0 && count(t) == 3);
	assert(find(t, 0, false).lastSeenMs == 1150);
	t.update(&near, 1, 2000, false);
	assert(find(t, 0).track == oldTrack);
	t.update(nullptr, 0, 7100, false);
	assert(count(t) == 0);

	Tracker colors;
	Detection blobs[] = {color(2, 20, 50, 25, 25), color(2, 230, 50, 25, 25)};
	colors.update(blobs, 2, 0, false);
	assert(count(colors, true) == 2);
	const auto leftTrack = colors.state.objects[0].track;
	const auto rightTrack = colors.state.objects[1].track;
	Detection reordered[] = {color(2, 225, 50, 25, 25), color(2, 25, 50, 25, 25)};
	colors.update(reordered, 2, 50, false);
	assert(colors.state.objects[0].track == leftTrack && colors.state.objects[0].centerX < 100);
	assert(colors.state.objects[1].track == rightTrack && colors.state.objects[1].centerX > 200);
	colors.update(nullptr, 0, 400, false);
	colors.update(reordered, 2, 450, false);
	assert(count(colors, true) == 2 && count(colors) == 4);

	pros::aivision_object_s_t raw{};
	raw.id = 0;
	raw.type = pros::E_AIVISION_DETECTED_TAG;
	raw.object.tag = {160, 80, 200, 120, 160, 160, 120, 120};
	Detection rotated;
	assert(camera_detail::decode(raw, rotated) && rotated.usableDepth);
	close(rotated.span, std::sqrt(3200.0f));
	raw.object.tag = {140, 100, 180, 100, 180, 110, 140, 110};
	assert(camera_detail::decode(raw, rotated) && !rotated.usableDepth);
	auto clipped = tag(0, 0, 80, 40);
	t.update(&clipped, 1, 7200, false);
	assert(!find(t, 0).depthValid);
	raw.type = 255;
	assert(!camera_detail::decode(raw, rotated));
	raw.type = pros::E_AIVISION_DETECTED_COLOR;
	raw.id = 1;
	raw.object.color = {300, 10, 100, 20, 0};
	assert(!camera_detail::decode(raw, rotated));

	Tracker calibration;
	calibration.defineTarget(CameraKind::Tag, 0, 100);
	calibration.setReference(CameraKind::Tag, 0, 500);
	calibration.calibrate(0);
	Detection duplicate[] = {reference, reference};
	calibration.update(duplicate, 2, 1000, false);
	assert(calibration.state.samples == 0);
	calibration.update(&reference, 1, 1050, true);
	assert(calibration.state.samples == 0);
	for (unsigned now = 1100; now < 1550; now += 50) calibration.update(&reference, 1, now, false);
	assert(calibration.state.samples == 0);
	calibration.update(&reference, 1, 1550, false);
	assert(calibration.state.samples == 1);
	auto shifted = tag(0, 145, 100, 40);
	calibration.update(&shifted, 1, 1600, false);
	assert(calibration.state.samples == 1);
	calibration.update(nullptr, 0, 1610, false);
	assert(calibration.state.samples == 0);
	calibration.update(nullptr, 0, 15001, false);
	assert(calibration.state.calibration == CameraCalibration::TimedOut && !calibration.state.focalPx);
	calibration.calibrate(16000);
	calibration.cancelCalibration();
	assert(calibration.state.calibration == CameraCalibration::NeedsReference);
	calibration.setFocal(210);
	calibration.calibrate(17000);
	calibration.cancelCalibration();
	assert(calibration.state.calibration == CameraCalibration::Ready && calibration.state.focalPx == 210);

	Tracker bounded;
	for (int id = 0; id < 100; ++id) {
		auto d = tag(id, 140, 100, 40);
		bounded.update(&d, 1, id, false);
	}
	assert(count(bounded) == int(CAMERA_MEMORY_SIZE));
	assert(find(bounded, 99).visible);
	Tracker wrapped;
	wrapped.update(&reference, 1, UINT32_MAX - 100, false);
	wrapped.update(nullptr, 0, 50, false);
	assert(count(wrapped) == 1);
	wrapped.update(nullptr, 0, 6000, false);
	assert(count(wrapped) == 0);
	puts("camera tracking/calibration tests passed");
}
