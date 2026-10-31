
#include "main.h"

#include <array>
#include <cerrno>
#include <cstring>

#ifdef ROGUE_SIM
#error "controller-check must be built for real hardware"
#endif

namespace {
constexpr pros::controller_analog_e_t axes[] = {
	pros::E_CONTROLLER_ANALOG_LEFT_X, pros::E_CONTROLLER_ANALOG_LEFT_Y,
	pros::E_CONTROLLER_ANALOG_RIGHT_X, pros::E_CONTROLLER_ANALOG_RIGHT_Y,
};
constexpr pros::controller_digital_e_t buttons[] = {
	pros::E_CONTROLLER_DIGITAL_L1, pros::E_CONTROLLER_DIGITAL_L2,
	pros::E_CONTROLLER_DIGITAL_R1, pros::E_CONTROLLER_DIGITAL_R2,
	pros::E_CONTROLLER_DIGITAL_UP, pros::E_CONTROLLER_DIGITAL_DOWN,
	pros::E_CONTROLLER_DIGITAL_LEFT, pros::E_CONTROLLER_DIGITAL_RIGHT,
	pros::E_CONTROLLER_DIGITAL_X, pros::E_CONTROLLER_DIGITAL_Y,
	pros::E_CONTROLLER_DIGITAL_A, pros::E_CONTROLLER_DIGITAL_B,
};
constexpr const char* button_names[] = {
	"L1", "L2", "R1", "R2", "Up", "Dn", "Lt", "Rt", "X", "Y", "A", "B",
};

struct Sample {

	std::array<int, 17> values{};
	std::array<int, 17> errors{};
	bool operator==(const Sample&) const = default;
};

Sample read(pros::controller_id_e_t id) {
	Sample result;
	auto save = [&](int index, int value) {
		result.values[index] = value;
		result.errors[index] = value == PROS_ERR ? errno : 0;
	};
	errno = 0;
	save(0, pros::c::controller_is_connected(id));
	for (int i = 0; i < 4; ++i) {
		errno = 0;
		save(i + 1, pros::c::controller_get_analog(id, axes[i]));
	}
	for (int i = 0; i < 12; ++i) {
		errno = 0;
		save(i + 5, pros::c::controller_get_digital(id, buttons[i]));
	}
	return result;
}

void button_text(const Sample& s, char (&text)[64]) {
	text[0] = '\0';
	for (int i = 0; i < 12; ++i) {
		if (s.values[i + 5] != 1) continue;
		const auto used = std::strlen(text);
		std::snprintf(text + used, sizeof text - used, "%s ", button_names[i]);
	}
	if (!text[0]) std::snprintf(text, sizeof text, "-");
}

void report(const char* name, const Sample& s) {
	char pressed[64];
	button_text(s, pressed);
	printf("check: %s link=%d LX=%d LY=%d RX=%d RY=%d buttons=%s\n",
	       name, s.values[0], s.values[1], s.values[2], s.values[3], s.values[4], pressed);
	for (int i = 0; i < 17; ++i) {
		if (s.values[i] == PROS_ERR) {
			printf("check: %s ERROR sample[%d]=PROS_ERR errno=%d\n", name, i, s.errors[i]);
		}
	}
}

void draw(const char* name, const Sample& s, int y) {
	char pressed[64];
	button_text(s, pressed);
	int error_count = 0;
	for (int value : s.values) if (value == PROS_ERR) ++error_count;
	pros::screen::print(pros::E_TEXT_SMALL, 8, y, "%s link=%d errors=%d", name, s.values[0], error_count);
	pros::screen::print(pros::E_TEXT_SMALL, 8, y + 20, "LX=%d LY=%d RX=%d RY=%d",
	                    s.values[1], s.values[2], s.values[3], s.values[4]);
	pros::screen::print(pros::E_TEXT_SMALL, 8, y + 40, "Buttons: %s", pressed);
}

void monitor() {
	std::array<Sample, 2> previous{};
	unsigned previous_mode = ~0u;
	std::uint32_t last_log = 0, last_draw = 0;
	bool first = true;
	while (true) {
		const auto now = pros::millis();
		const unsigned mode = pros::competition::get_status();
		const std::array<Sample, 2> samples = {
			read(pros::E_CONTROLLER_MASTER), read(pros::E_CONTROLLER_PARTNER),
		};
		if (first || samples != previous || mode != previous_mode || now - last_log >= 1000) {
			printf("check: %lu ms mode=0x%02x disabled=%d auto=%d connected=%d\n",
			       (unsigned long)now, mode, !!(mode & COMPETITION_DISABLED),
			       !!(mode & COMPETITION_AUTONOMOUS), !!(mode & COMPETITION_CONNECTED));
			report("MASTER", samples[0]);
			report("PARTNER", samples[1]);
			last_log = now;
		}
		if (first || now - last_draw >= 100) {
			pros::screen::set_pen(0x000000);
			pros::screen::fill_rect(0, 0, 479, 239);
			pros::screen::set_pen(0xffffff);
			pros::screen::print(pros::E_TEXT_SMALL, 8, 5, "CTRL CHECK v1 | no motor commands | %lu ms", (unsigned long)now);
			pros::screen::print(pros::E_TEXT_SMALL, 8, 25, "mode=0x%02x %s", mode,
			                    mode & COMPETITION_DISABLED ? "DISABLED" : "ENABLED");
			draw("MASTER", samples[0], 60);
			draw("PARTNER", samples[1], 135);
			pros::screen::print(pros::E_TEXT_SMALL, 8, 215, "Errors: see pros terminal. Move sticks / hold A.");
			last_draw = now;
		}
		first = false;
		previous = samples;
		previous_mode = mode;
		pros::delay(20);
	}
}
}

void initialize() {
	printf("check: CTRL CHECK v1, built " __DATE__ " " __TIME__ "\n");

	static pros::Task task(monitor, "controller-check");
}

void disabled() { printf("check: callback disabled\n"); }
void competition_initialize() { printf("check: callback competition_initialize\n"); }
void autonomous() { printf("check: callback autonomous\n"); }
void opcontrol() { printf("check: callback opcontrol\n"); }
