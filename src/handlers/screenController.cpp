#include "handlers/screenController.hpp"

#include "main.h"

namespace {

constexpr int SCREEN_W = 480;
constexpr int SCREEN_H = 240;
constexpr int STATUS_BAR_H = 32;

// Approximate glyph sizes of the brain's built-in monospace font, used to
// size the erase box behind each line. The emulator draws the font bigger
// than the real brain, so the box is made wider there.
struct Metrics { int w; int h; };
Metrics metrics(ScreenFont font) {
#ifdef ROGUE_SIM
	switch (font) {
		case ScreenFont::Small:  return {14, 20};
		case ScreenFont::Large:  return {28, 40};
		default:                 return {20, 28};
	}
#else
	switch (font) {
		case ScreenFont::Small:  return {8, 16};
		case ScreenFont::Large:  return {24, 40};
		default:                 return {12, 22};
	}
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

void writeScreenText(const char* text, int x, int y, ScreenFont font) {
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
	pros::screen::set_pen(pros::Color::white);
	pros::screen::print(format(font), x, y, "%s", text);
	lock.give();
}

void clearScreen() {
	lock.take();
	for (Slot& s : slots) s.used = false;
	pros::screen::set_pen(pros::Color::black);
	pros::screen::fill_rect(0, STATUS_BAR_H, SCREEN_W, SCREEN_H);
	lock.give();
}

void showError(const char* text) {
	printf("ERROR: %s\n", text);
	lock.take();
	pros::screen::set_pen(pros::Color::black);
	pros::screen::fill_rect(0, 100, SCREEN_W, 150);
	pros::screen::set_pen(pros::Color::red);
	pros::screen::print(pros::E_TEXT_LARGE, 10, 105, "%s", text);
	pros::screen::set_pen(pros::Color::white);
	lock.give();
}
