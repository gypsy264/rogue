#pragma once

#include <cstdint>

void sleep_ms(std::uint32_t ms);

void debugStep(const char* step, std::uint32_t hold_ms = 0);

const char* competitionText();

unsigned competitionBits();

void drawInputScreen(bool full);

void logInputChanges();
