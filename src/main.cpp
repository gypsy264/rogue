#include "main.h"

#include "handlers/gamepadController.hpp"
#include "handlers/motorController.hpp"
#include "handlers/screenController.hpp"
#include "handlers/supportController.hpp"

static void driverLoop();  // defined below, used by disabled() and opcontrol()

/**
 * Runs once when the program starts. Keep it short, competition modes
 * wait for it to finish.
 */
void initialize() {
	clearScreen();
	writeScreenLarge("rogue", 10, 10);

	// Drive ports. Negative = motor mounted backwards, spins reversed.
	// Front of the robot: if pushing the stick forward drives it backwards,
	// flip both signs. If it spins instead of going straight, flip only one.
	debugStep("start");

	debugStep("motor ports");
	motorDefinePort(DriveSide::Left, 10);
	motorDefinePort(DriveSide::Right, -9);
	motorSetMaxRpm(200);  // green cartridge; 100 red, 600 blue
	// Paste the values autoCalibrate prints here to keep them after a restart:
	// motorSetTrim(DriveSide::Left, 1.000);
	// motorSetTrim(DriveSide::Right, 1.000);
	debugStep("motor init");
	if (motorInit() == 0) {
		writeScreen("drive ok  %d + %d motors", 10, 70, motorCount(DriveSide::Left), motorCount(DriveSide::Right));
	}

	// Controller on a cable in smart port 1. Use CONTROLLER_WIRELESS for the radio.
	debugStep("pad port");
	controllerDefinePort(1);
	debugStep("pad init");
	gamepadInit();

	debugStep("init done");
}

/**
 * Runs while the brain reports "disabled". With a real field or competition
 * switch plugged into the controller, the robot must stay still. Without one,
 * the brain can still report disabled (seen with the controller linked by
 * radio), so the robot keeps driving in practice mode.
 */
void disabled() {
	debugStep("disabled", 0);
	driveStop();
	if (!pros::competition::is_connected()) {
		printf("rogue: disabled with no field or switch, practice mode\n");
		driverLoop();
	}
	// PROS ends this task by itself as soon as the robot is enabled again.
	while (true) {
		printf("rogue: disabled, mode %s, bits 0x%02x, pad %s\n", competitionText(), competitionBits(),
		       gamepadLinkText());
		writeScreen("mode: %s", 250, 200, competitionText());
		gamepadPrint(1, competitionText());
		sleep_ms(500);
	}
}

/** Runs after initialize() when a field or competition switch is connected. */
void competition_initialize() {
	debugStep("comp init", 0);
}

/** Runs the 15 second autonomous period. */
void autonomous() {
	debugStep("autonomous", 0);
}

/**
 * Driver control loop. Arcade drive: left stick Y = forward, right stick X = turn.
 *   A: toggle a slow spin in place
 *   B: run autoCalibrate (temporary), press B again to cancel it
 * Never returns; PROS stops it when the competition mode changes.
 */
// Title and the fixed lines of the status page.
static void drawStatusPage() {
	clearScreen();
	writeScreenLarge("rogue", 10, 10);
	writeScreen("tap screen: inputs", 10, 70);
}

static void driverLoop() {
	constexpr int SPIN_POWER = 30;  // out of 127, slow
	bool spinning = false;
	int loops = 0;
	static bool input_page = true;  // starts on the input screen while we debug the controller
	if (input_page) drawInputScreen(true);

	while (true) {
		// Tapping the brain screen switches between the input and status pages.
		if (screenTapped()) {
			input_page = !input_page;
			if (input_page) {
				drawInputScreen(true);
			} else {
				drawStatusPage();
			}
		}
		logInputChanges();

		if (gamepadPressed(GamepadButton::A)) {
			spinning = !spinning;
			gamepadRumble(".");
			printf("rogue: spin %s\n", spinning ? "on" : "off");
		}

		if (gamepadPressed(GamepadButton::B)) {
			spinning = false;
			gamepadRumble("-");
			autoCalibrate(100);
		}

		if (spinning) {
			driveTank(SPIN_POWER, -SPIN_POWER);
		} else {
			driveArcade(gamepadLeftY(), gamepadRightX());
		}

		if (input_page && loops % 5 == 0) drawInputScreen(false);  // 10 times a second

		if (loops % 50 == 0) {  // once a second: console heartbeat
			printf("rogue: loop %d, %lu ms, mode %s, bits 0x%02x, pad %s\n", loops, (unsigned long)pros::millis(),
			       competitionText(), competitionBits(), gamepadLinkText());
		}
		if (!input_page && loops % 50 == 0) {  // once a second: status page refresh
			writeScreen("mode: %s", 250, 200, competitionText());
			writeScreen("L %4d   R %4d", 10, 130, driveLeftPower(), driveRightPower());
			writeScreen("spin %s", 250, 130, spinning ? "on" : "off");
			writeScreen("pad %s", 10, 165, gamepadLinkText());
			writeScreen("trim %.2f %.2f", 250, 165, motorTrim(DriveSide::Left), motorTrim(DriveSide::Right));
			writeScreen("up %.1f s", 10, 200, pros::millis() / 1000.0);
		}
		loops++;
		sleep_ms(20);  // 50 Hz control loop
	}
}

/** Driver control. With no competition switch connected this starts right after initialize(). */
void opcontrol() {
	debugStep("opcontrol", 0);
	driverLoop();
}
