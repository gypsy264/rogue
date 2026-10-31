#pragma once

#include <array>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#define PROS_ERR INT32_MAX
#define PROS_ERR_F INFINITY

namespace mechanism_test {
struct MotorState {
	bool connected = true, readError = false, commandError = false, gearError = false;
	double position = 0;
	int power = 0, brakes = 0;
	int gearRpm = 0;
	bool degrees = false, hold = false;
};
inline std::array<MotorState, 22> motors;
}

namespace pros {
enum class MotorGears { red = 100, green = 200, blue = 600 };
enum class MotorUnits { degrees };
enum class MotorBrake { hold, coast };
class Motor {
public:
	explicit Motor(std::int8_t port) : port(port) {}
	std::int32_t set_gearing(MotorGears gears) {
		if (state().gearError) { errno = EIO; return PROS_ERR; }
		state().gearRpm = static_cast<int>(gears);
		return 1;
	}
	std::int32_t set_encoder_units(MotorUnits) { state().degrees = true; return 1; }
	std::int32_t set_brake_mode(MotorBrake) { state().hold = true; return 1; }
	std::int32_t move(std::int32_t power) {
		if (state().commandError) { errno = EIO; return PROS_ERR; }
		state().power = port < 0 ? -power : power;
		return 1;
	}
	std::int32_t brake() { state().power = 0; ++state().brakes; return 1; }
	double get_position() {
		if (state().readError) { errno = EIO; return PROS_ERR_F; }
		return state().position * (port < 0 ? -1 : 1);
	}
private:
	std::int8_t port;
	mechanism_test::MotorState& state() { return mechanism_test::motors[std::abs(port)]; }
};
namespace c {
enum device_type { E_DEVICE_NONE, E_DEVICE_MOTOR };
inline device_type get_plugged_type(std::uint8_t port) {
	return mechanism_test::motors[port].connected ? E_DEVICE_MOTOR : E_DEVICE_NONE;
}
}
}
