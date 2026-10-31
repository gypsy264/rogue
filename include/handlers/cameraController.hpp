#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

constexpr int CAMERA_AUTO_PORT = 0;
constexpr std::size_t CAMERA_MEMORY_SIZE = 32;
enum class CameraKind { Tag, Color, Code, Model };
enum class CameraFamily { Circle21H7, Tag16H5, Tag25H9, Tag61H11 };
enum class CameraStatus { Searching, Warming, Ready, Offline, Error, Multiple, Simulator };
enum class CameraCalibration { NeedsReference, Waiting, Sampling, Ready, TimedOut };

struct CameraObject {
	std::uint32_t track = 0, lastSeenMs = 0;
	CameraKind kind = CameraKind::Tag;
	int id = 0;
	float centerX = 0, centerY = 0, width = 0, height = 0;
	float xMm = 0, yMm = 0, zMm = 0;
	bool visible = false, depthValid = false;
};

struct CameraSnapshot {
	CameraStatus status = CameraStatus::Searching;
	CameraCalibration calibration = CameraCalibration::NeedsReference;
	int port = 0, error = 0, samples = 0;
	float focalPx = 0, pollHz = 0;
	std::uint32_t updatedMs = 0, frameMs = 0;
	std::uint32_t calibrationRequest = 0;
	std::array<CameraObject, CAMERA_MEMORY_SIZE> objects{};
};

// Configure before cameraInit(). All dimensions are millimeters.
bool cameraDefinePort(int port);
bool cameraSetTagFamily(CameraFamily family);
bool cameraDefineColor(int id, int red, int green, int blue, float hueRange, float saturationRange);
bool cameraDefineTarget(CameraKind kind, int id, float widthMm);
bool cameraSetCalibrationReference(CameraKind kind, int id, float distanceMm);
bool cameraSetFocalLength(float pixels);
void cameraInit();
std::uint32_t cameraCalibrate();
void cameraCancelCalibration();
void cameraSetMoving(bool moving);
bool cameraSnapshot(CameraSnapshot& result);
bool cameraFind(CameraKind kind, int id, CameraObject& result, std::uint32_t maxAgeMs = 250);
void drawCameraScreen(bool full, bool autonomous = false);
