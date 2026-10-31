#pragma once

#include <cstdio>
#include <cstdint>
#include <cstring>

constexpr std::uint32_t SCREEN_FRAME_MS = 33;
enum class ScreenFont { Small, Medium, Large };

void writeScreenText(const char* text, int x, int y, ScreenFont font = ScreenFont::Medium);

void clearScreen();

// Status: y=218..239.
bool writeScreenStatus(const char* text, bool alert);

void drawScreenBox(int x, int y, int w, int h);

struct ScreenBox { int x, y, w, h; unsigned color; };
void drawScreenPreview(int x, int y, int w, int h, const ScreenBox* boxes, size_t count);

void drawButton(const char* label, int x, int y, int w, int h, bool on);

void drawStick(int cx, int cy, int r, int x, int y);

bool screenTapped();

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
}

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
