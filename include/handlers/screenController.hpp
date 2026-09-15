#pragma once
// Brain screen helper. Call from anywhere in the code to show debug text.
//
//   writeScreen("hello", 10, 40);
//   writeScreen("loop %d  imu %.1f", 10, 70, loops, heading);   // printf style
//   writeScreenLarge("AUTON LEFT", 10, 40);
//   showError("motor on port 3 missing");
//   clearScreen();
//
// The screen is 480 x 240 px. Rows 0 to 31 belong to the brain's status bar,
// so draw at y >= 32. x, y is the top-left corner of the text. Text drawn at
// the same x, y again erases the old text first, so values can be refreshed
// in a loop without ghosting.

#include <cstdio>
#include <cstring>

enum class ScreenFont { Small, Medium, Large };

// Low level, plain text. Erases what was previously written at (x, y).
void writeScreenText(const char* text, int x, int y, ScreenFont font = ScreenFont::Medium);

// Blacks out the whole user area and forgets every written line.
void clearScreen();

// Red text in the middle of the screen plus the same message on the console.
void showError(const char* text);

namespace screen_detail {
template <typename... Args>
void format(char* buf, size_t size, const char* fmt, Args... args) {
	if constexpr (sizeof...(args) == 0) {
		std::strncpy(buf, fmt, size - 1);
		buf[size - 1] = '\0';
	} else {
		std::snprintf(buf, size, fmt, args...);
	}
}
}  // namespace screen_detail

// printf-style helpers. Extra arguments go after x and y.
template <typename... Args>
void writeScreen(const char* fmt, int x, int y, Args... args) {
	char buf[96];
	screen_detail::format(buf, sizeof buf, fmt, args...);
	writeScreenText(buf, x, y, ScreenFont::Medium);
}

template <typename... Args>
void writeScreenSmall(const char* fmt, int x, int y, Args... args) {
	char buf[96];
	screen_detail::format(buf, sizeof buf, fmt, args...);
	writeScreenText(buf, x, y, ScreenFont::Small);
}

template <typename... Args>
void writeScreenLarge(const char* fmt, int x, int y, Args... args) {
	char buf[96];
	screen_detail::format(buf, sizeof buf, fmt, args...);
	writeScreenText(buf, x, y, ScreenFont::Large);
}
