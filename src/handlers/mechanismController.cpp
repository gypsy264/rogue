#include "handlers/mechanismController.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <memory>

#include "handlers/gamepadController.hpp"
#include "main.h"

namespace {
struct Axis {
	std::unique_ptr<pros::Motor> motor;
	MechanismState state;
	double zero = 0;
	int testPower = 25;
	int gearRatio = 0;
	bool configured = false;
};
std::array<Axis, 2> axes;
bool started = false;

Axis& axis(Mechanism mechanism) { return axes.at(static_cast<unsigned>(mechanism)); }

void fail(Axis& a, int error) {
	if (a.motor) a.motor->brake();
	a.state.power = 0;
	a.state.connected = false;
	a.state.zeroed = false;
	a.state.error = error ? error : EIO;
	a.configured = false;
}

void refresh(Axis& a) {
	if (!a.motor) return;
	if (pros::c::get_plugged_type(std::abs(a.state.port)) != pros::c::E_DEVICE_MOTOR) {
		fail(a, ENODEV);
		return;
	}
	if (!a.configured) {
		if (a.gearRatio) {
			const auto gears = a.gearRatio == 36 ? pros::MotorGears::red
			                 : a.gearRatio == 6 ? pros::MotorGears::blue : pros::MotorGears::green;
			if (a.motor->set_gearing(gears) == PROS_ERR) { fail(a, errno); return; }
		}
		if (a.motor->set_encoder_units(pros::MotorUnits::degrees) == PROS_ERR ||
		    a.motor->set_brake_mode(pros::MotorBrake::hold) == PROS_ERR ||
		    a.motor->brake() == PROS_ERR) {
			fail(a, errno);
			return;
		}
		a.configured = true;
	}
	const double position = a.motor->get_position();
	if (!std::isfinite(position) || position == PROS_ERR_F) { fail(a, errno); return; }
	a.state.connected = true;
	a.state.error = 0;
	if (!a.state.zeroed) return;
	a.state.degrees = position - a.zero;
	a.state.percent = a.state.degrees * 100.0 / a.state.degreesPer100;
	a.state.minPercent = std::min(a.state.minPercent, a.state.percent);
	a.state.maxPercent = std::max(a.state.maxPercent, a.state.percent);
}
}

bool mechanismDefinePort(Mechanism mechanism, int port) {
	if (started || port == 0 || port < -21 || port > 21) return false;
	auto& a = axis(mechanism);
	for (const auto& other : axes)
		if (&other != &a && std::abs(other.state.port) == std::abs(port)) return false;
	a.state.port = port;
	return true;
}

bool mechanismSetGearRatio(Mechanism mechanism, int ratio) {
	if (started || (ratio != 6 && ratio != 18 && ratio != 36)) return false;
	axis(mechanism).gearRatio = ratio;
	return true;
}

bool mechanismSetScale(Mechanism mechanism, double degreesPer100) {
	if (started || !std::isfinite(degreesPer100) || degreesPer100 <= 0) return false;
	axis(mechanism).state.degreesPer100 = degreesPer100;
	return true;
}

bool mechanismSetTestPower(Mechanism mechanism, int power) {
	if (started || power < 1 || power > 127) return false;
	axis(mechanism).testPower = power;
	return true;
}

int mechanismInit() {
	if (!started) {
		started = true;
		for (auto& a : axes) {
			if (a.state.port) a.motor = std::make_unique<pros::Motor>(a.state.port);
			else a.state.error = ENXIO;
		}
		mechanismRefresh();
		mechanismZeroAll();
	}
	int missing = 0;
	for (const auto& a : axes) if (!a.state.connected) ++missing;
	return missing;
}

void mechanismRefresh() { for (auto& a : axes) refresh(a); }

bool mechanismMove(Mechanism mechanism, int power) {
	auto& a = axis(mechanism);
	if (!a.motor || !a.state.connected) return false;
	power = std::clamp(power, -127, 127);
	if (power == a.state.power) return true;
	const auto result = power ? a.motor->move(power) : a.motor->brake();
	if (result == PROS_ERR) { fail(a, errno); return false; }
	a.state.power = power;
	return true;
}

void mechanismStop() {
	mechanismMove(Mechanism::Claw, 0);
	mechanismMove(Mechanism::Elevator, 0);
}

void mechanismDriverUpdate() {
	mechanismRefresh();
	if (!gamepadConnected()) { mechanismStop(); return; }
	const int claw = int(gamepadHeld(GamepadButton::Left)) - int(gamepadHeld(GamepadButton::Right));
	const int lift = int(gamepadHeld(GamepadButton::Up)) - int(gamepadHeld(GamepadButton::Down));
	mechanismMove(Mechanism::Claw, claw * axis(Mechanism::Claw).testPower);
	mechanismMove(Mechanism::Elevator, lift * axis(Mechanism::Elevator).testPower);
}

bool mechanismZero(Mechanism mechanism) {
	auto& a = axis(mechanism);
	if (!a.motor || !a.state.connected || a.state.power != 0) return false;
	const double position = a.motor->get_position();
	if (!std::isfinite(position) || position == PROS_ERR_F) { fail(a, errno); return false; }
	a.zero = position;
	a.state.zeroed = true;
	a.state.degrees = a.state.percent = a.state.minPercent = a.state.maxPercent = 0;
	printf("mechanism: port %d zero, 100%% = %.1f deg\n", a.state.port, a.state.degreesPer100);
	return true;
}

void mechanismZeroAll() {
	if (mechanismMoving()) return;
	mechanismZero(Mechanism::Claw);
	mechanismZero(Mechanism::Elevator);
}

bool mechanismMoving() { return axes[0].state.power != 0 || axes[1].state.power != 0; }

MechanismState mechanismState(Mechanism mechanism) { return axis(mechanism).state; }
