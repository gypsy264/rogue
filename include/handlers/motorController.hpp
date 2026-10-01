#pragma once
// Drive motors. Define the ports once in initialize(), then drive from anywhere.
//
//   motorDefinePort(DriveSide::Left, 10);
//   motorDefinePort(DriveSide::Right, -9);    // negative port = motor spins reversed
//   motorSetMaxRpm(200);                      // cartridge: 100 red, 200 green, 600 blue
//   motorInit();                              // builds the groups, checks every motor
//
//   driveArcade(forward, turn);               // values from -127 to 127
//   driveTank(left, right);
//   driveStop();
//
// Power is sent as a target speed, not a voltage: the motor's own controller
// holds that speed even if one side has more friction or load, so both sides
// turn at the same speed for the same number. A per-side trim (see
// autoCalibrate in supportController) corrects what is left.

enum class DriveSide { Left, Right };

// Registers one drive motor. Ports are 1 to 21. Call before motorInit().
void motorDefinePort(DriveSide side, int port);

// Top speed of the gear cartridge inside the motors: 100 (red), 200 (green,
// the default) or 600 (blue). Must match the real cartridge, otherwise speeds
// are wrong. Call before motorInit().
void motorSetMaxRpm(int rpm);
int motorMaxRpm();

// Creates the left and right motor groups from the registered ports and checks
// that every motor is plugged in. Missing motors are printed to the console and
// shown on the screen. Returns how many motors are missing (0 means all good).
int motorInit();

// Power per side, -127 (full reverse) to 127 (full forward). Values are clamped.
void driveTank(int left, int right);

// forward: + goes forward. turn: + turns right.
void driveArcade(int forward, int turn);

// Stops both sides.
void driveStop();

// Last power asked for each side, before trim, handy for writeScreen.
int driveLeftPower();
int driveRightPower();

// Speed multiplier per side, 0.5 to 1.0. 1.0 means no correction.
void motorSetTrim(DriveSide side, double trim);
double motorTrim(DriveSide side);

// Measured speed of a side in rpm, averaged over its motors, always positive.
double motorMeasuredRpm(DriveSide side);

// Number of motors registered on a side.
int motorCount(DriveSide side);
