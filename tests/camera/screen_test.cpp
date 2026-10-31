#include "handlers/cameraController.hpp"
#include "handlers/screenController.hpp"
#include "main.h"

#include <cassert>
#include <cstring>

namespace {
CameraSnapshot data;
int texts = 0, previews = 0;
bool hasDepth = false, hasLost = false;
float pollHz = -1, uiFps = -1;
}

bool cameraSnapshot(CameraSnapshot& result) { result = data; return true; }
void clearScreen() {}
void drawScreenBox(int x, int y, int w, int h) {
	assert(x >= 0 && y >= 0 && x + w < 480 && y + h < 218);
}
void drawScreenPreview(int x, int y, int w, int h, const ScreenBox* boxes, size_t count) {
	assert(x >= 0 && y >= 0 && x + w < 480 && y + h < 218);
	for (size_t i = 0; i < count; ++i) {
		assert(boxes[i].x >= 0 && boxes[i].y >= 0);
		assert(boxes[i].x + boxes[i].w <= w && boxes[i].y + boxes[i].h <= h);
	}
	++previews;
}
void writeScreenText(const char* text, int x, int y, ScreenFont font) {
	const int width = font == ScreenFont::Small ? 8 : font == ScreenFont::Large ? 24 : 12;
	const int height = font == ScreenFont::Small ? 16 : font == ScreenFont::Large ? 40 : 22;
	assert(x >= 0 && y >= 0 && x + int(std::strlen(text)) * width <= 480 && y + height < 218);
	hasDepth = hasDepth || std::strstr(text, "Z ~") != nullptr;
	hasLost = hasLost || std::strstr(text, "LOST") != nullptr;
	if (std::strncmp(text, "Poll ", 5) == 0)
		assert(std::sscanf(text, "Poll %f Hz | UI %f FPS", &pollHz, &uiFps) == 2);
	++texts;
}

int main() {
	data.port = 11;
	for (const auto mode : {false, true}) {
		for (auto status : {CameraStatus::Offline, CameraStatus::Warming, CameraStatus::Error, CameraStatus::Ready}) {
			data.status = status;
			drawCameraScreen(true, mode);
		}
	}
	data.objects[0] = {1, 0, CameraKind::Tag, 0, 160, 120, 40, 40, 0, 0, 500, true, true};
	data.calibration = CameraCalibration::Ready;
	data.focalPx = 200;
	drawCameraScreen(true, false);
	drawCameraScreen(true, true);
	assert(hasDepth);
	data.objects[0].visible = false;
	drawCameraScreen(false, false);
	drawCameraScreen(false, true);
	assert(hasLost);
	assert(texts > 0 && previews == 12);
	data.pollHz = 100;
	drawCameraScreen(true, false);
	const int before = previews;
	for (unsigned now = 20; now <= 1000; now += 20) {
		camera_test::now = now;
		data.updatedMs = now;
		drawCameraScreen(false, false);
	}
	assert(previews - before == 30);
	assert(pollHz == 100 && uiFps == 30);
	const int unchanged = previews;
	drawCameraScreen(false, false);
	assert(previews == unchanged);
	drawCameraScreen(true, true);
	assert(uiFps == 0);
	puts("camera screen layout tests passed");
}
