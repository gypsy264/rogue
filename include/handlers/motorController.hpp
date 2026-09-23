#pragma once
// Drive motors. Define the ports once in initialize(), then drive from anywhere.
//
//   motorDefinePort(DriveSide::Left, 1);
//   motorDefinePort(DriveSide::Left, -2);    // negative port = motor spins reversed
//   motorDefinePort(DriveSide::Right, -4);
//   motorInit();                              // builds the groups, checks every motor
//
//   driveArcade(forward, turn);               // values from -127 to 127
//   driveTank(left, right);
//   driveStop();

enum class DriveSide { Left, Right };

// Registers one drive motor. Ports are 1 to 21. Call before motorInit().
void motorDefinePort(DriveSide side, int port);

// Creates the left and right motor groups from the registered ports and checks
// that every motor is plugged in. Missing motors are printed to the console and
// shown on the screen. Returns how many motors are missing (0 means all good).
int motorInit();

// Power per side, -127 (full reverse) to 127 (full forward). Values are clamped.
void driveTank(int left, int right);

// forward: + goes forward. turn: + turns right.
void driveArcade(int forward, int turn);

// Cuts power to both sides.
void driveStop();

// Last power sent to each side, handy for writeScreen.
int driveLeftPower();
int driveRightPower();

// Number of motors registered on a side.
int motorCount(DriveSide side);
