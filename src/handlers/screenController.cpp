#include "handlers/screenController.hpp"

#include "main.h"

namespace {

constexpr int SCREEN_W = 480;
constexpr int SCREEN_H = 272;  // physical panel; the status bar area is protected by the brain


// Approximate glyph sizes of the brain's built-in monospace font, used to
// size the erase box behind each line. Slightly generous on purpose.
struct Metrics { int w; int h; };
Metrics metrics(ScreenFont font) {
	switch (font) {
		case ScreenFont::Small:  return {8, 16};
		case ScreenFont::Large:  return {24, 40};
		default:                 return {12, 22};
	}
}

// The emulator's display channel stalls when draw commands arrive back to
// back, so emulator builds pause briefly after each one. No-op on the robot.
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

// Remembers how wide the last text at each (x, y) was, so a shorter new
// value still wipes the old one completely.
struct Slot { int x; int y; int width; int height; bool used; };
constexpr int MAX_SLOTS = 32;
Slot slots[MAX_SLOTS];

// Where showError writes. Anything else written at this spot replaces it.
constexpr int ERROR_X = 10;
constexpr int ERROR_Y = 70;
pros::Mutex lock;

Slot* find_slot(int x, int y) {
	Slot* free_slot = nullptr;
	for (Slot& s : slots) {
		if (s.used && s.x == x && s.y == y) return &s;
		if (!s.used && !free_slot) free_slot = &s;
	}
	if (free_slot) {
		*free_slot = {x, y, 0, 0, true};
	}
	return free_slot;  // nullptr when all slots are taken: text is still drawn, just not erased later
}

}  // namespace

namespace {

void draw_text(const char* text, int x, int y, ScreenFont font, pros::Color color) {
	const Metrics m = metrics(font);
	const int width = static_cast<int>(std::strlen(text)) * m.w;

	lock.take();
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
	lock.give();
}

}  // namespace

void writeScreenText(const char* text, int x, int y, ScreenFont font) {
	draw_text(text, x, y, font, pros::Color::white);
}

void clearScreen() {
	lock.take();
	for (Slot& s : slots) s.used = false;
	pros::screen::set_pen(pros::Color::black);
	pros::screen::fill_rect(0, 0, SCREEN_W, SCREEN_H);
	pace();
	lock.give();
}

void showError(const char* text) {
	printf("ERROR: %s\n", text);
	draw_text(text, ERROR_X, ERROR_Y, ScreenFont::Large, pros::Color::red);
}
