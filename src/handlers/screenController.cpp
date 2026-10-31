#include "handlers/screenController.hpp"

#include "main.h"

namespace {

constexpr int SCREEN_W = 480;
constexpr int STATUS_Y = 218;

struct Metrics { int w; int h; };
Metrics metrics(ScreenFont font) {
	switch (font) {
		case ScreenFont::Small:  return {8, 16};
		case ScreenFont::Large:  return {24, 40};
		default:                 return {12, 22};
	}
}

void pace() {
#ifdef ROGUE_SIM
	uint32_t start = pros::millis();
	while (pros::millis() - start < 3) {}
#endif
}

pros::text_format_e_t format(ScreenFont font) {
	switch (font) {
		case ScreenFont::Small:  return pros::E_TEXT_SMALL;
		case ScreenFont::Large:  return pros::E_TEXT_LARGE;
		default:                 return pros::E_TEXT_MEDIUM;
	}
}

struct Slot { int x; int y; int width; int height; bool used; };
constexpr int MAX_SLOTS = 32;
Slot slots[MAX_SLOTS];

constexpr int ERROR_X = 10;
constexpr int ERROR_Y = 70;

pros::Mutex& screen_lock() {
	static pros::Mutex m;
	return m;
}

Slot* find_slot(int x, int y) {
	Slot* free_slot = nullptr;
	for (Slot& s : slots) {
		if (s.used && s.x == x && s.y == y) return &s;
		if (!s.used && !free_slot) free_slot = &s;
	}
	if (free_slot) {
		*free_slot = {x, y, 0, 0, true};
	}
	return free_slot;
}

}

namespace {

void draw_text(const char* text, int x, int y, ScreenFont font, pros::Color color) {
	const Metrics m = metrics(font);
	const int width = static_cast<int>(std::strlen(text)) * m.w;

	screen_lock().take();
	Slot* slot = find_slot(x, y);
	int erase_w = width;
	int erase_h = m.h;
	if (slot) {
		if (slot->width > erase_w) erase_w = slot->width;
		if (slot->height > erase_h) erase_h = slot->height;
		slot->width = width;
		slot->height = m.h;
	}

	pros::screen::set_pen(pros::Color::black);
	pros::screen::fill_rect(x, y, x + erase_w, y + erase_h);
	pace();
	pros::screen::set_pen(color);
	pros::screen::print(format(font), x, y, "%s", text);
	pace();
	pros::screen::set_pen(pros::Color::white);
	screen_lock().give();
}

}

void writeScreenText(const char* text, int x, int y, ScreenFont font) {
	draw_text(text, x, y, font, pros::Color::white);
}

void clearScreen() {
	screen_lock().take();
	for (Slot& s : slots) s.used = false;
	pros::screen::set_pen(pros::Color::black);
	pros::screen::fill_rect(0, 0, SCREEN_W - 1, STATUS_Y - 1);
	pace();
	screen_lock().give();
}

bool writeScreenStatus(const char* text, bool alert) {
	if (!screen_lock().take(5)) return false;
	pros::screen::set_pen(alert ? 0x00600000u : 0x00181818u);
	pros::screen::fill_rect(0, STATUS_Y, SCREEN_W - 1, 239);
	pace();
	pros::screen::set_pen(pros::Color::white);
	pros::screen::print(pros::E_TEXT_SMALL, 4, STATUS_Y + 2, "%.58s", text);
	pace();
	screen_lock().give();
	return true;
}

void showError(const char* text) {
	printf("ERROR: %s\n", text);
	draw_text(text, ERROR_X, ERROR_Y, ScreenFont::Large, pros::Color::red);
}

void drawScreenBox(int x, int y, int w, int h) {
	screen_lock().take();
	pros::screen::set_pen(0x0040B0FFu);
	pros::screen::draw_rect(x, y, x + w, y + h);
	pace();
	pros::screen::set_pen(pros::Color::white);
	screen_lock().give();
}

void drawScreenPreview(int x, int y, int w, int h, const ScreenBox* boxes, size_t count) {
	screen_lock().take();
	pros::screen::set_pen(0x00080810u);
	pros::screen::fill_rect(x, y, x + w, y + h);
	pace();
	pros::screen::set_pen(0x0040B0FFu);
	pros::screen::draw_rect(x, y, x + w, y + h);
	pace();
	for (size_t i = 0; i < count; ++i) {
		const auto& b = boxes[i];
		pros::screen::set_pen(b.color);
		pros::screen::draw_rect(x + b.x, y + b.y, x + b.x + b.w, y + b.y + b.h);
		pace();
	}
	pros::screen::set_pen(pros::Color::white);
	screen_lock().give();
}

void drawButton(const char* label, int x, int y, int w, int h, bool on) {
	screen_lock().take();
	pros::screen::set_pen(on ? 0x0000C000u : 0x00303030u);
	pros::screen::fill_rect(x, y, x + w, y + h);
	pace();
	pros::screen::set_pen(on ? pros::Color::black : pros::Color::white);
	pros::screen::print(pros::E_TEXT_SMALL, x + 6, y + (h - 16) / 2, "%s", label);
	pace();
	pros::screen::set_pen(pros::Color::white);
	screen_lock().give();
}

void drawStick(int cx, int cy, int r, int x, int y) {
	const int dot_x = cx + x * r / 127;
	const int dot_y = cy - y * r / 127;
	screen_lock().take();
	pros::screen::set_pen(pros::Color::black);
	pros::screen::fill_rect(cx - r - 8, cy - r - 8, cx + r + 8, cy + r + 8);
	pace();
	pros::screen::set_pen(0x00808080u);
	pros::screen::draw_circle(cx, cy, r);
	pace();
	pros::screen::set_pen((x != 0 || y != 0) ? 0x00FFD000u : 0x00606060u);
	pros::screen::fill_circle(dot_x, dot_y, 6);
	pace();
	pros::screen::set_pen(pros::Color::white);
	screen_lock().give();
}

bool screenTapped() {
	static std::int32_t last_presses = -1;
	const pros::screen_touch_status_s_t touch = pros::screen::touch_status();
	if (touch.touch_status == pros::E_TOUCH_ERROR) return false;
	if (last_presses < 0) {
		last_presses = touch.press_count;
		return false;
	}
	if (touch.press_count != last_presses) {
		last_presses = touch.press_count;
		return true;
	}
	return false;
}
