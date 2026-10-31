#include "handlers/cameraController.hpp"

#include <algorithm>

#include "handlers/detail/loopTiming.hpp"
#include "handlers/screenController.hpp"
#include "main.h"

namespace {
const char* statusName(CameraStatus status) {
	switch (status) {
		case CameraStatus::Searching: return "searching";
		case CameraStatus::Warming: return "white balance / warmup";
		case CameraStatus::Ready: return "ready";
		case CameraStatus::Offline: return "camera not connected";
		case CameraStatus::Error: return "sensor read error";
		case CameraStatus::Multiple: return "select a camera port";
		case CameraStatus::Simulator: return "no camera in simulator";
	}
	return "unknown";
}

const char* calibrationName(CameraCalibration calibration) {
	switch (calibration) {
		case CameraCalibration::NeedsReference: return "set size + reference Z";
		case CameraCalibration::Waiting: return "stop & center reference";
		case CameraCalibration::Sampling: return "calibrating";
		case CameraCalibration::Ready: return "depth calibrated (approx)";
		case CameraCalibration::TimedOut: return "calibration timed out";
	}
	return "unknown";
}

const char* kindName(CameraKind kind) {
	switch (kind) {
		case CameraKind::Tag: return "tag";
		case CameraKind::Color: return "color";
		case CameraKind::Code: return "code";
		case CameraKind::Model: return "model";
	}
	return "?";
}
}

void drawCameraScreen(bool full, bool autonomous) {
	static handler_detail::FrameClock frame_clock;
	static handler_detail::RateCounter frame_rate;
	static bool previous_mode = false;
	full = full || autonomous != previous_mode;
	previous_mode = autonomous;
	if (!frame_clock.due(pros::millis(), SCREEN_FRAME_MS, full)) return;
	if (full) frame_rate.reset();
	if (full) {
		clearScreen();
		if (autonomous) {
			drawScreenBox(10, 3, 460, 66);
			writeScreenLarge("Autonomous", 120, 7);
			writeScreenSmall("mode active", 196, 49);
		} else {
			writeScreen("AI Vision", 8, 3);
			writeScreenSmall("B: calibrate  tap: next", 226, 9);
		}
	}
	const int x = autonomous ? 180 : 226;
	const int y = autonomous ? 89 : 44;
	CameraSnapshot snapshot;
	if (!cameraSnapshot(snapshot)) { writeScreenSmall("camera data busy", x, y); return; }
	const auto now = pros::millis();
	writeScreenSmall("Poll %.0f Hz | UI %.0f FPS", 8, autonomous ? 71 : 27,
	                 snapshot.pollHz, frame_rate.tick(now));
	std::array<ScreenBox, CAMERA_MEMORY_SIZE> boxes{};
	std::size_t count = 0;
	int visible = 0, remembered = 0;
	const CameraObject* selected = nullptr;
	const float scale = autonomous ? 0.5f : 0.65f;
	for (const auto& object : snapshot.objects) {
		if (!object.track) continue;
		++remembered;
		if (object.visible) ++visible;
		if (!selected || (object.visible && !selected->visible) ||
		    (object.visible == selected->visible && object.width * object.height > selected->width * selected->height)) selected = &object;
		const unsigned color = !object.visible ? 0x00404040u : object.kind == CameraKind::Tag ? 0x0000E080u : 0x0000C8FFu;
		const int left = std::clamp(int((object.centerX - object.width / 2) * scale), 0, int(320 * scale));
		const int top = std::clamp(int((object.centerY - object.height / 2) * scale), 0, int(240 * scale));
		boxes[count++] = {left, top, std::clamp(int(object.width * scale), 0, int(320 * scale) - left),
		                  std::clamp(int(object.height * scale), 0, int(240 * scale) - top), color};
	}
	drawScreenPreview(8, y, int(320 * scale), int(240 * scale), boxes.data(), count);
	const bool stalled = now - snapshot.updatedMs > 1500 && snapshot.status != CameraStatus::Simulator;
	writeScreenSmall("P%d %s", x, y, snapshot.port, stalled ? "camera task stalled" : statusName(snapshot.status));
	writeScreenSmall("seen %d  memory %d  err %d", x, y + 16, visible, remembered, snapshot.error);
	if (snapshot.calibration == CameraCalibration::Sampling)
		writeScreenSmall("calibrating %d/12", x, y + 32, snapshot.samples);
	else writeScreenSmall("%s", x, y + 32, calibrationName(snapshot.calibration));
	if (!selected) {
		writeScreenSmall("no objects", x, y + 48);
		writeScreenSmall("", x, y + 64);
		writeScreenSmall("", x, y + 80);
		writeScreenSmall("", x, y + 96);
		writeScreenSmall("f %.1fpx samples %d/12", x, y + 112, snapshot.focalPx, snapshot.samples);
		return;
	}
	const auto& o = *selected;
	writeScreenSmall("%s %d  #%lu  %s", x, y + 48, kindName(o.kind), o.id, (unsigned long)o.track, o.visible ? "seen" : "LOST");
	writeScreenSmall("px %.0f,%.0f size %.0fx%.0f", x, y + 64, o.centerX, o.centerY, o.width, o.height);
	if (o.depthValid) {
		writeScreenSmall("Z ~%.0f mm%s", x, y + 80, o.zMm, o.visible ? "" : " (last)");
		writeScreenSmall("X ~%.0f Y ~%.0f mm", x, y + 96, o.xMm, o.yMm);
	} else {
		writeScreenSmall("Z --", x, y + 80);
		writeScreenSmall("size/cal/geometry needed", x, y + 96);
	}
	writeScreenSmall("age %lums  f %.1fpx", x, y + 112, (unsigned long)(now - o.lastSeenMs), snapshot.focalPx);
}
