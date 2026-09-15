#include "main.h"

// Hardware layout. Change ports here, nowhere else.
namespace ports {
	constexpr std::initializer_list<int8_t> LEFT_DRIVE = {1, -2, 3};    // negative = reversed
	constexpr std::initializer_list<int8_t> RIGHT_DRIVE = {-4, 5, -6};
}

/**
 * Runs once when the program starts. Keep it short, competition modes
 * wait for it to finish.
 */
void initialize() {
	printf(" rogue: initialize\n");  // shows in `pros terminal` and in the emulator console
	pros::lcd::initialize();
	pros::lcd::set_text(1, "rogue");
	pros::lcd::set_text(2, "ready");
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

	while (true) {
		int dir = master.get_analog(ANALOG_LEFT_Y);
		int turn = master.get_analog(ANALOG_RIGHT_X);
		left_mg.move(dir - turn);
		right_mg.move(dir + turn);

		pros::lcd::print(3, "L %4d  R %4d", dir - turn, dir + turn);
		pros::delay(20);  // 50 Hz control loop
	}
}
