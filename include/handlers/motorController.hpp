#pragma once

enum class DriveSide { Left, Right };

// Negative port reverses the motor.
void motorDefinePort(DriveSide side, int port);

void motorSetMaxRpm(int rpm);
int motorMaxRpm();

int motorInit();

// -127..127.
void driveTank(int left, int right, bool applyTrim = true);

void driveArcade(int forward, int turn, int maxPower = 127);
bool driveReady();

void driveStop();

int driveLeftPower();
int driveRightPower();

void motorSetTrim(DriveSide side, double trim);
double motorTrim(DriveSide side);

double motorMeasuredRpm(DriveSide side);

int motorCount(DriveSide side);
