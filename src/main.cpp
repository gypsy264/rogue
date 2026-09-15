#include "main.h"

#include "handlers/screenController.hpp"

// Hardware layout. Change ports here, nowhere else.
namespace ports {
	constexpr std::initializer_list<int8_t> LEFT_DRIVE = {1, -2, 3};    // negative = reversed
	constexpr std::initializer_list<int8_t> RIGHT_DRIVE = {-4, 5, -6};
}

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
	writeScreenLarge("rogue", 10, 40);
	writeScreen("ready", 10, 90);
}

/** Runs while the robot is disabled by the field or competition switch. */
void disabled() {}

/** Runs after initialize() when a field or competition switch is connected. */
void competition_initialize() {}

/** Runs the 15 second autonomous period. */
void autonomous() {}

/**
 * Driver control. With no competition switch connected this starts right
 * after initialize(). Arcade drive: left stick Y = forward, right stick X = turn.
 */
void opcontrol() {
	pros::Controller master(pros::E_CONTROLLER_MASTER);
	pros::MotorGroup left_mg(ports::LEFT_DRIVE);
	pros::MotorGroup right_mg(ports::RIGHT_DRIVE);

	int loops = 0;
	while (true) {
		int dir = master.get_analog(ANALOG_LEFT_Y);
		int turn = master.get_analog(ANALOG_RIGHT_X);
		left_mg.move(dir - turn);
		right_mg.move(dir + turn);

		if (loops % 50 == 0) {  // once a second: console heartbeat and screen refresh
			printf("rogue: loop %d, %lu ms\n", loops, (unsigned long)pros::millis());
			writeScreen("L %4d   R %4d", 10, 130, dir - turn, dir + turn);
			writeScreen("loop %d", 10, 160, loops);
			writeScreen("up %.1f s", 250, 160, pros::millis() / 1000.0);
		}
		loops++;
		sleep_ms(20);  // 50 Hz control loop
	}
}
