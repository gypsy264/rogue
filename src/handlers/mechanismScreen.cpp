#include "handlers/mechanismController.hpp"

#include "handlers/screenController.hpp"
#include "handlers/driverController.hpp"

namespace {
void drawAxis(Mechanism mechanism, const char* name, const char* controls, int y) {
	const auto state = mechanismState(mechanism);
	writeScreenSmall("%s P%d  %s", 8, y, name, state.port, controls);
	if (!state.connected || !state.zeroed) {
		writeScreen("%s", 8, y + 18, state.connected ? "B: calibrate" : "motor offline");
		writeScreenSmall("errno %d", 310, y + 21, state.error);
		writeScreenSmall("", 8, y + 44);
		writeScreenSmall("", 8, y + 60);
		return;
	}
	writeScreen("%+.1f%%", 8, y + 18, state.percent);
	writeScreenSmall("%+.1f deg", 310, y + 21, state.degrees);
	writeScreenSmall("min %+.1f%%  max %+.1f%%", 8, y + 44, state.minPercent, state.maxPercent);
	writeScreenSmall("100%% = %.1f deg  cmd %+d", 8, y + 60, state.degreesPer100, state.power);
}
}

void drawMechanismScreen(bool full) {
	if (full) {
		clearScreen();
		writeScreen("Claw / elevator", 8, 2);
		writeScreenSmall("tap: inputs", 380, 6);
	}
	drawAxis(Mechanism::Claw, "CLAW", "Left: open  Right: close", 28);
	drawAxis(Mechanism::Elevator, "LIFT", "Up: raise  Down: lower", 112);
	writeScreenSmall("drive %d%%  L1:80/100  R1:slow  B:cal", 8, 198, driverSpeedPercent());
}
