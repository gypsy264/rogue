#include "handlers/detail/cameraTracking.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace camera_detail {
bool validId(CameraKind kind, int id) {
	if (kind == CameraKind::Color) return id >= 1 && id <= 7;
	if (kind == CameraKind::Code) return id >= 1 && id <= 5;
	if (kind == CameraKind::Tag || kind == CameraKind::Model) return id >= 0 && id <= 255;
	return false;
}

bool decode(const pros::aivision_object_s_t& raw, Detection& d) {
	d = {};
	d.id = raw.id;
	float left = 0, top = 0;
	bool square = true;
	if (raw.type == pros::E_AIVISION_DETECTED_TAG) {
		d.kind = CameraKind::Tag;
		const auto& t = raw.object.tag;
		const float x[] = {float(t. x0), float(t.x1), float(t.x2), float(t.x3)};
		const float y[] = {float(t.y0), float(t.y1), float(t.y2), float(t.y3)};
		left = *std::min_element(x, x + 4);
		top = *std::min_element(y, y + 4);
		d.width = *std::max_element(x, x + 4) - left;
		d.height = *std::max_element(y, y + 4) - top;
		float shortest = 10000, longest = 0;
		for (int i = 0; i < 4; ++i) {
			const int j = (i + 1) % 4, k = (i + 2) % 4;
			const float dx = x[j] - x[i], dy = y[j] - y[i];
			const float nx = x[k] - x[j], ny = y[k] - y[j];
			const float side = std::hypot(dx, dy), next = std::hypot(nx, ny);
			if (side < 1 || next < 1) return false;
			shortest = std::min(shortest, side);
			longest = std::max(longest, side);
			square = square && std::fabs(dx * nx + dy * ny) / (side * next) < 0.25f;
			d.span += side / 4;
			d.cx += x[i] / 4;
			d.cy += y[i] / 4;
		}
		square = square && shortest / longest > 0.8f;
	} else if (raw.type == pros::E_AIVISION_DETECTED_COLOR || raw.type == pros::E_AIVISION_DETECTED_CODE) {
		d.kind = raw.type == pros::E_AIVISION_DETECTED_COLOR ? CameraKind::Color : CameraKind::Code;
		left = raw.object.color.xoffset;
		top = raw.object.color.yoffset;
		d.width = raw.object.color.width;
		d.height = raw.object.color.height;
	} else if (raw.type == pros::E_AIVISION_DETECTED_OBJECT) {
		d.kind = CameraKind::Model;
		if (raw.object.element.score < 50 || raw.object.element.score > 100) return false;
		left = raw.object.element.xoffset;
		top = raw.object.element.yoffset;
		d.width = raw.object.element.width;
		d.height = raw.object.element.height;
	} else return false;
	if (!validId(d.kind, d.id) || d.width <= 0 || d.height <= 0 ||
	    left < 0 || top < 0 || left + d.width > 320 || top + d.height > 240) return false;
	if (d.kind != CameraKind::Tag) {
		d.cx = left + d.width / 2;
		d.cy = top + d.height / 2;
		d.span = d.width;
	}
	d.usableDepth = square && d.span >= 8 && left > 1 && top > 1 &&
	                left + d.width < 319 && top + d.height < 239;
	return true;
}

float Tracker::widthFor(CameraKind kind, int id) const {
	for (const auto& t : targets) if (t.width > 0 && t.kind == kind && t.id == id) return t.width;
	return 0;
}

bool Tracker::defineTarget(CameraKind kind, int id, float width) {
	if (!validId(kind, id) || !std::isfinite(width) || width <= 0) return false;
	for (auto& t : targets) {
		if (t.width > 0 && t.kind == kind && t.id == id) { t.width = width; return true; }
	}
	for (auto& t : targets) {
		if (!t.width) { t = {kind, id, width}; return true; }
	}
	return false;
}

bool Tracker::setReference(CameraKind kind, int id, float distance) {
	if (!widthFor(kind, id) || !std::isfinite(distance) || distance <= 0) return false;
	referenceKind = kind;
	referenceId = id;
	referenceDistance = distance;
	return true;
}

bool Tracker::setFocal(float pixels) {
	if (!std::isfinite(pixels) || pixels <= 0) return false;
	state.focalPx = pixels;
	state.calibration = CameraCalibration::Ready;
	return true;
}

bool Tracker::calibrate(std::uint32_t now) {
	if (!referenceDistance) {
		if (!state.focalPx) state.calibration = CameraCalibration::NeedsReference;
		return false;
	}
	calibrationStart = lastMotion = now;
	state.samples = 0;
	spanSum = 0;
	state.calibration = CameraCalibration::Waiting;
	return true;
}

void Tracker::cancelCalibration() {
	if (state.calibration != CameraCalibration::Waiting && state.calibration != CameraCalibration::Sampling) return;
	state.calibration = state.focalPx > 0 ? CameraCalibration::Ready : CameraCalibration::NeedsReference;
	state.samples = 0;
}

void Tracker::sample(const Detection* detections, int count, std::uint32_t now, bool moving) {
	if (state.calibration != CameraCalibration::Waiting && state.calibration != CameraCalibration::Sampling) return;
	if (now - calibrationStart > 15000) {
		state.calibration = CameraCalibration::TimedOut;
		state.samples = 0;
		return;
	}
	if (moving) lastMotion = now;
	const Detection* reference = nullptr;
	int matches = 0;
	for (int i = 0; i < count; ++i) {
		if (detections[i].kind == referenceKind && detections[i].id == referenceId) {
			reference = &detections[i];
			++matches;
		}
	}
	if (moving || now - lastMotion < 500 || matches != 1 || !reference->usableDepth ||
	    reference->span < 20 || std::fabs(reference->cx - 160) > 32 || std::fabs(reference->cy - 120) > 24) {
		state.samples = 0;
		state.calibration = CameraCalibration::Waiting;
		return;
	}
	if (state.samples && (std::fabs(reference->span - baseSpan) > baseSpan * 0.03f ||
	    std::fabs(reference->cx - baseX) > 3 || std::fabs(reference->cy - baseY) > 3)) state.samples = 0;
	if (state.samples && now - lastSample < 50) return;
	if (!state.samples) {
		spanSum = 0;
		baseSpan = reference->span;
		baseX = reference->cx;
		baseY = reference->cy;
	}
	spanSum += reference->span;
	lastSample = now;
	++state.samples;
	state.calibration = CameraCalibration::Sampling;
	if (state.samples >= 12) {
		const float focal = spanSum / state.samples * referenceDistance / widthFor(referenceKind, referenceId);
		if (!setFocal(focal)) state.calibration = CameraCalibration::TimedOut;
	}
}

void Tracker::lose(std::uint32_t now) {
	for (auto& o : state.objects) {
		o.visible = false;
		if (o.track && now - o.lastSeenMs > MEMORY_MS) o = {};
	}
}

void Tracker::update(const Detection* detections, int count, std::uint32_t now, bool moving) {
	count = std::clamp(count, 0, FRAME_LIMIT);
	lose(now);
	sample(detections, count, now, moving);
	std::array<int, FRAME_LIMIT> assignment;
	assignment.fill(-1);
	std::array<bool, CAMERA_MEMORY_SIZE> used{};
	for (;;) {
		float best = std::numeric_limits<float>::max();
		int detection = -1, slot = -1;
		for (int i = 0; i < count; ++i) {
			if (assignment[i] >= 0) continue;
			const auto& d = detections[i];
			for (std::size_t j = 0; j < state.objects.size(); ++j) {
				const auto& o = state.objects[j];
				if (used[j] || !o.track || o.kind != d.kind || o.id != d.id) continue;
				const float dx = o.centerX - d.cx, dy = o.centerY - d.cy;
				const float cost = dx * dx + dy * dy;
				if (d.kind != CameraKind::Tag && (now - o.lastSeenMs > FRESH_MS || cost > 1600 ||
				    d.width < o.width * 0.5f || d.width > o.width * 2)) continue;
				if (cost < best) { best = cost; detection = i; slot = int(j); }
			}
		}
		if (slot < 0) break;
		assignment[detection] = slot;
		used[slot] = true;
	}
	for (int i = 0; i < count; ++i) {
		int slot = assignment[i];
		if (slot < 0) {
			for (std::size_t j = 0; j < state.objects.size(); ++j) {
				if (!used[j] && (!state.objects[j].track || slot < 0 ||
				    now - state.objects[j].lastSeenMs > now - state.objects[slot].lastSeenMs)) {
					slot = int(j);
					if (!state.objects[j].track) break;
				}
			}
			if (slot < 0) continue;
			state.objects[slot] = {};
			state.objects[slot].track = nextTrack++;
			if (!nextTrack) nextTrack = 1;
		}
		used[slot] = true;
		auto& o = state.objects[slot];
		const auto& d = detections[i];
		o.kind = d.kind;
		o.id = d.id;
		o.centerX = d.cx;
		o.centerY = d.cy;
		o.width = d.width;
		o.height = d.height;
		o.lastSeenMs = now;
		o.visible = true;
		o.depthValid = false;
		o.xMm = o.yMm = o.zMm = 0;
		const float width = widthFor(d.kind, d.id);
		if (state.focalPx > 0 && width > 0 && d.usableDepth) {
			o.zMm = state.focalPx * width / d.span;
			o.xMm = (d.cx - 160) * o.zMm / state.focalPx;
			o.yMm = (d.cy - 120) * o.zMm / state.focalPx;
			o.depthValid = std::isfinite(o.zMm) && std::isfinite(o.xMm) && std::isfinite(o.yMm) && o.zMm > 0;
		}
	}
	state.frameMs = now;
}
}
