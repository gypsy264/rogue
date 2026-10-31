#include "main.h"
#include "handlers/gamepadController.hpp"
#include "handlers/mechanismController.hpp"
#include "handlers/screenController.hpp"

#include <cassert>
#include <cstring>

namespace {
bool connected = true;
std::array<bool, 12> held{};
bool saw124 = false, sawOffline = false, sawZero = false;
}

bool gamepadConnected() { return connected; }
int driverSpeedPercent() { return 80; }
bool gamepadHeld(GamepadButton button) { return held[static_cast<int>(button)]; }
void press(GamepadButton button, bool value) { held[static_cast<int>(button)] = value; }
void clearScreen() {}
void writeScreenText(const char* text, int x, int y, ScreenFont font) {
	const int w = font == ScreenFont::Small ? 8 : 12;
	const int h = font == ScreenFont::Small ? 16 : 22;
	assert(x >= 0 && x + int(std::strlen(text)) * w <= 480 && y >= 0 && y + h < 218);
	saw124 |= std::strstr(text, "+124.0%") != nullptr;
	sawOffline |= std::strstr(text, "motor offline") != nullptr;
	sawZero |= std::strstr(text, "B: calibrate") != nullptr;
}
void close(double value, double expected) { assert(std::fabs(value - expected) < 0.001); }

int main(int argc, char**) {
	using namespace mechanism_test;
	assert(!mechanismDefinePort(Mechanism::Claw, 0));
	assert(!mechanismDefinePort(Mechanism::Claw, 22));
	assert(mechanismDefinePort(Mechanism::Claw, -11));
	assert(!mechanismDefinePort(Mechanism::Elevator, 11));
	assert(mechanismDefinePort(Mechanism::Elevator, -5));
	assert(!mechanismSetGearRatio(Mechanism::Claw, 19));
	assert(mechanismSetGearRatio(Mechanism::Claw, 18));
	assert(!mechanismSetScale(Mechanism::Claw, 0));
	assert(!mechanismSetScale(Mechanism::Claw, NAN));
	assert(mechanismSetScale(Mechanism::Claw, 360));
	assert(mechanismSetScale(Mechanism::Elevator, 720));
	assert(!mechanismSetTestPower(Mechanism::Claw, 128));
	assert(mechanismSetTestPower(Mechanism::Claw, 45));
	assert(mechanismSetTestPower(Mechanism::Elevator, 50));
	motors[11].position = 50;
	motors[5].position = 80;
	if (argc > 1) {
		motors[11].connected = false;
		assert(mechanismInit() == 1);
		assert(!mechanismState(Mechanism::Claw).connected);
		assert(mechanismState(Mechanism::Elevator).zeroed);
		motors[11].connected = true;
		mechanismRefresh();
		assert(mechanismState(Mechanism::Claw).connected);
		assert(motors[11].gearRpm == 200);
		assert(!mechanismState(Mechanism::Claw).zeroed);
		mechanismZeroAll();
		assert(mechanismState(Mechanism::Claw).zeroed);
		puts("mechanism missing-at-start test passed");
		return 0;
	}
	assert(mechanismInit() == 0);
	assert(!mechanismDefinePort(Mechanism::Claw, 12));
	assert(!mechanismSetGearRatio(Mechanism::Claw, 6));
	assert(motors[11].gearRpm == 200 && motors[5].gearRpm == 0);
	assert(motors[11].degrees && motors[5].hold);
	assert(motors[11].power == 0 && motors[5].power == 0);
	press(GamepadButton::Left, true);
	press(GamepadButton::Up, true);
	mechanismDriverUpdate();
	assert(motors[11].power == -45 && motors[5].power == -50);
	motors[11].position -= 446.4;
	motors[5].position -= 900;
	mechanismDriverUpdate();
	close(mechanismState(Mechanism::Claw).percent, 124);
	close(mechanismState(Mechanism::Elevator).percent, 125);
	assert(motors[11].power == -45 && motors[5].power == -50);
	assert(!mechanismZero(Mechanism::Claw));
	mechanismZeroAll();
	close(mechanismState(Mechanism::Claw).percent, 124);
	drawMechanismScreen(true);
	assert(saw124);
	press(GamepadButton::Right, true);
	press(GamepadButton::Down, true);
	mechanismDriverUpdate();
	assert(!mechanismMoving() && motors[11].power == 0 && motors[5].power == 0);
	press(GamepadButton::Left, false);
	press(GamepadButton::Up, false);
	mechanismDriverUpdate();
	assert(motors[11].power == 45 && motors[5].power == 50);
	motors[11].position = 50 + 180;
	mechanismDriverUpdate();
	close(mechanismState(Mechanism::Claw).percent, -50);
	close(mechanismState(Mechanism::Claw).minPercent, -50);
	close(mechanismState(Mechanism::Claw).maxPercent, 124);
	held.fill(false);
	mechanismDriverUpdate();
	assert(motors[11].power == 0 && motors[5].power == 0);
	const auto brakes = motors[5].brakes;
	mechanismDriverUpdate();
	assert(motors[5].brakes == brakes);
	mechanismZeroAll();
	close(mechanismState(Mechanism::Claw).percent, 0);
	close(mechanismState(Mechanism::Claw).minPercent, 0);
	close(mechanismState(Mechanism::Claw).maxPercent, 0);
	press(GamepadButton::Left, true);
	press(GamepadButton::Up, true);
	mechanismDriverUpdate();
	connected = false;
	mechanismDriverUpdate();
	assert(!mechanismMoving() && motors[11].power == 0 && motors[5].power == 0);
	connected = true;
	mechanismDriverUpdate();
	mechanismStop();
	assert(!mechanismMoving());
	mechanismDriverUpdate();
	motors[11].readError = true;
	mechanismDriverUpdate();
	assert(motors[11].power == 0 && !mechanismState(Mechanism::Claw).connected);
	drawMechanismScreen(false);
	assert(sawOffline);
	motors[11].readError = false;
	motors[5].commandError = true;
	press(GamepadButton::Up, false);
	press(GamepadButton::Down, true);
	mechanismDriverUpdate();
	assert(!mechanismState(Mechanism::Claw).zeroed);
	assert(!mechanismState(Mechanism::Elevator).connected && motors[5].power == 0);
	drawMechanismScreen(false);
	assert(sawZero);
	held.fill(false);
	motors[5].commandError = false;
	mechanismDriverUpdate();
	mechanismZeroAll();
	assert(mechanismState(Mechanism::Claw).zeroed && mechanismState(Mechanism::Elevator).zeroed);
	motors[11].connected = false;
	mechanismDriverUpdate();
	assert(!mechanismState(Mechanism::Claw).connected && !mechanismState(Mechanism::Claw).zeroed);
	motors[11].connected = true;
	motors[11].gearError = true;
	press(GamepadButton::Left, true);
	mechanismDriverUpdate();
	assert(!mechanismState(Mechanism::Claw).connected && motors[11].power == 0);
	motors[11].gearError = false;
	motors[11].gearRpm = 0;
	mechanismDriverUpdate();
	assert(motors[11].gearRpm == 200 && motors[11].power == -45);
	puts("mechanism input, telemetry, fault and screen tests passed");
}
