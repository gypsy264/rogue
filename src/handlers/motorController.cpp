#include "handlers/motorController.hpp"

#include <cmath>
#include <memory>
#include <vector>

#include "handlers/screenController.hpp"
#include "main.h"

namespace {

std::vector<std::int8_t> left_ports;
std::vector<std::int8_t> right_ports;
std::unique_ptr<pros::MotorGroup> left_group;
std::unique_ptr<pros::MotorGroup> right_group;
bool initialized = false;
int max_rpm = 200;
double left_trim = 1.0;
double right_trim = 1.0;
int left_power = 0;
int right_power = 0;

int clamp_power(int value) {
	if (value > 127) return 127;
	if (value < -127) return -127;
	return value;
}

pros::MotorGears gears_for(int rpm) {
	if (rpm == 100) return pros::MotorGears::red;
	if (rpm == 600) return pros::MotorGears::blue;
	return pros::MotorGears::green;
}

bool port_taken(int port) {
	const int abs_port = port < 0 ? -port : port;
	for (auto list : {&left_ports, &right_ports}) {
		for (std::int8_t p : *list) {
			if ((p < 0 ? -p : p) == abs_port) return true;
		}
	}
	return false;
}

// Appends the ports with no motor attached to `missing` and returns the count.
int check_plugged(const std::vector<std::int8_t>& ports, char* missing, size_t size) {
	int count = 0;
	for (std::int8_t p : ports) {
		const std::uint8_t port = static_cast<std::uint8_t>(p < 0 ? -p : p);
		if (pros::c::get_plugged_type(port) != pros::c::E_DEVICE_MOTOR) {
			const size_t used = std::strlen(missing);
			std::snprintf(missing + used, size - used, " %d", port);
			count++;
		}
	}
	return count;
}

// -127..127 power to a target rpm, with the side's trim applied.
int to_rpm(int power, double trim) {
	return static_cast<int>(std::lround(power * trim * max_rpm / 127.0));
}

}  // namespace

void motorDefinePort(DriveSide side, int port) {
	if (initialized) {
		printf("motor: port %d ignored, define ports before motorInit()\n", port);
		return;
	}
	const int abs_port = port < 0 ? -port : port;
	if (abs_port < 1 || abs_port > 21) {
		printf("motor: port %d is not valid, use 1 to 21\n", port);
		return;
	}
	if (port_taken(port)) {
		printf("motor: port %d is already defined\n", abs_port);
		return;
	}
	(side == DriveSide::Left ? left_ports : right_ports).push_back(static_cast<std::int8_t>(port));
}

void motorSetMaxRpm(int rpm) {
	if (rpm != 100 && rpm != 200 && rpm != 600) {
		printf("motor: max rpm %d is not a V5 cartridge, use 100, 200 or 600\n", rpm);
		return;
	}
	if (initialized) {
		printf("motor: max rpm ignored, call motorSetMaxRpm before motorInit()\n");
		return;
	}
	max_rpm = rpm;
}

int motorMaxRpm() { return max_rpm; }

int motorInit() {
	if (initialized) return 0;
	initialized = true;

	if (left_ports.empty() || right_ports.empty()) {
		showError("no drive ports defined");
		return -1;
	}

	left_group = std::make_unique<pros::MotorGroup>(left_ports, gears_for(max_rpm));
	right_group = std::make_unique<pros::MotorGroup>(right_ports, gears_for(max_rpm));
	left_group->set_gearing_all(gears_for(max_rpm));
	right_group->set_gearing_all(gears_for(max_rpm));
	left_group->set_brake_mode_all(pros::MotorBrake::coast);
	right_group->set_brake_mode_all(pros::MotorBrake::coast);

	char missing[64] = "";
	const int count = check_plugged(left_ports, missing, sizeof missing) +
	                  check_plugged(right_ports, missing, sizeof missing);

	printf("motor: %d left, %d right, %d rpm, %d missing\n", (int)left_ports.size(), (int)right_ports.size(),
	       max_rpm, count);
	if (count > 0) {
		char message[80];
		std::snprintf(message, sizeof message, "no motor:%s", missing);
		showError(message);
	}
	return count;
}

void driveTank(int left, int right) {
	if (!left_group || !right_group) return;
	left_power = clamp_power(left);
	right_power = clamp_power(right);
	left_group->move_velocity(to_rpm(left_power, left_trim));
	right_group->move_velocity(to_rpm(right_power, right_trim));
}

void driveArcade(int forward, int turn) {
	driveTank(forward + turn, forward - turn);
}

void driveStop() {
	driveTank(0, 0);
}

int driveLeftPower() { return left_power; }
int driveRightPower() { return right_power; }

void motorSetTrim(DriveSide side, double trim) {
	if (trim < 0.5) trim = 0.5;
	if (trim > 1.0) trim = 1.0;
	(side == DriveSide::Left ? left_trim : right_trim) = trim;
}

double motorTrim(DriveSide side) { return side == DriveSide::Left ? left_trim : right_trim; }

double motorMeasuredRpm(DriveSide side) {
	const auto& group = side == DriveSide::Left ? left_group : right_group;
	if (!group) return 0.0;
	const std::vector<double> speeds = group->get_actual_velocity_all();
	double sum = 0.0;
	int n = 0;
	for (double v : speeds) {
		if (v == PROS_ERR_F || std::isinf(v) || std::isnan(v)) continue;
		sum += std::fabs(v);
		n++;
	}
	return n > 0 ? sum / n : 0.0;
}

int motorCount(DriveSide side) {
	return static_cast<int>((side == DriveSide::Left ? left_ports : right_ports).size());
}
