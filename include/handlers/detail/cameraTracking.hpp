#pragma once

#include "handlers/cameraController.hpp"
#include "pros/ai_vision.h"

namespace camera_detail {
constexpr std::uint32_t MEMORY_MS = 5000;
constexpr std::uint32_t FRESH_MS = 250;
constexpr int FRAME_LIMIT = 24;

struct Detection {
	CameraKind kind = CameraKind::Tag;
	int id = 0;
	float cx = 0, cy = 0, width = 0, height = 0, span = 0;
	bool usableDepth = false;
};

bool decode(const pros::aivision_object_s_t& raw, Detection& result);
bool validId(CameraKind kind, int id);

class Tracker {
public:
	CameraSnapshot state;
	bool defineTarget(CameraKind kind, int id, float width);
	bool setReference(CameraKind kind, int id, float distance);
	bool setFocal(float pixels);
	bool calibrate(std::uint32_t now);
	void cancelCalibration();
	void update(const Detection* detections, int count, std::uint32_t now, bool moving);
	void lose(std::uint32_t now);

private:
	struct Target { CameraKind kind{}; int id = 0; float width = 0; };
	std::array<Target, 16> targets{};
	CameraKind referenceKind = CameraKind::Tag;
	int referenceId = 0;
	float referenceDistance = 0, spanSum = 0, baseSpan = 0, baseX = 0, baseY = 0;
	std::uint32_t nextTrack = 1, calibrationStart = 0, lastMotion = 0, lastSample = 0;
	float widthFor(CameraKind kind, int id) const;
	void sample(const Detection* detections, int count, std::uint32_t now, bool moving);
};
}
