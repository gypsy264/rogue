#pragma once

enum class Mechanism { Claw, Elevator };

struct MechanismState {
	int port = 0, power = 0, error = 0;
	bool connected = false, zeroed = false;
	double degrees = 0, percent = 0, minPercent = 0, maxPercent = 0;
	double degreesPer100 = 360;
};

// Negative port reverses movement and position.
bool mechanismDefinePort(Mechanism mechanism, int port);
// Cartridge ratio: 6, 18 or 36.
bool mechanismSetGearRatio(Mechanism mechanism, int ratio);
bool mechanismSetScale(Mechanism mechanism, double degreesPer100);
bool mechanismSetTestPower(Mechanism mechanism, int power);
int mechanismInit();
void mechanismRefresh();
void mechanismDriverUpdate();
void mechanismStop();
bool mechanismMove(Mechanism mechanism, int power);
bool mechanismZero(Mechanism mechanism);
void mechanismZeroAll();
bool mechanismMoving();
MechanismState mechanismState(Mechanism mechanism);
void drawMechanismScreen(bool full);
