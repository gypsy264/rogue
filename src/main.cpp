#include "main.h"

#include "handlers/gamepadController.hpp"
#include "handlers/motorController.hpp"
#include "handlers/screenController.hpp"

/**
 * Sleep for the control loop. On the robot this is pros::delay, which lets
 * other tasks run. The QEMU emulator never wakes a sleeping task (its timer
 * interrupt is incomplete), so emulator builds (tools/sim.sh, -DROGUE_SIM)
 * busy-wait instead. Use this everywhere instead of pros::delay.
 */
static void sleep_ms(uint32_t ms) {
#ifdef ROGUE_SIM
	uint32_t start = pros::millis();
	while (pros::millis() - start < ms) {}
#else
	pros::delay(ms);
#endif
}

/**
 * Runs once when the program starts. Keep it short, competition modes
 * wait for it to finish.
 */
void initialize() {
	printf("rogue: initialize\n");  // shows in `pros terminal` and in the emulator console
	clearScreen();
	writeScreenLarge("rogue", 10, 10);

	// Drive ports. Negative = motor mounted backwards, spins reversed.
	// Front of the robot: if pushing the stick forward drives it backwards,
	// flip both signs. If it spins instead of going straight, flip only one.
	motorDefinePort(DriveSide::Left, 10);
	motorDefinePort(DriveSide::Right, -9);
	if (motorInit() == 0) {
		writeScreen("drive ok  %d + %d motors", 10, 70, motorCount(DriveSide::Left), motorCount(DriveSide::Right));
	}

	gamepadInit();
}

/** Runs while the robot is disabled by the field or competition switch. */
void disabled() {
	driveStop();
}

/** Runs after initialize() when a field or competition switch is connected. */
void competition_initialize() {}

/** Runs the 15 second autonomous period. */
void autonomous() {}

/**
 * Driver control. With no competition switch connected this starts right
 * after initialize(). Arcade drive: left stick Y = forward, right stick X = turn.
 */
void opcontrol() {
	constexpr int SPIN_POWER = 30;  // out of 127, slow
	bool spinning = false;
	int loops = 0;
	while (true) {
		// A toggles a slow spin in place. Sticks drive normally when it is off.
		if (gamepadPressed(GamepadButton::A)) {
			spinning = !spinning;
			gamepadRumble(".");
			printf("rogue: spin %s\n", spinning ? "on" : "off");
		}

		if (spinning) {
			driveTank(SPIN_POWER, -SPIN_POWER);
		} else {
			driveArcade(gamepadLeftY(), gamepadRightX());
		}

		if (loops % 50 == 0) {  // once a second: console heartbeat and screen refresh
			printf("rogue: loop %d, %lu ms\n", loops, (unsigned long)pros::millis());
			writeScreen("L %4d   R %4d", 10, 130, driveLeftPower(), driveRightPower());
			writeScreen("spin %s", 250, 130, spinning ? "on" : "off");
			writeScreen("pad %s", 10, 165, gamepadConnected() ? "on" : "off");
			writeScreen("loop %d", 250, 165, loops);
			writeScreen("up %.1f s", 10, 200, pros::millis() / 1000.0);
		}
		loops++;
		sleep_ms(20);  // 50 Hz control loop
	}
}
