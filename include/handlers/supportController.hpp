#pragma once
// Support tools: timing helper and temporary calibration routines.

#include <cstdint>

// Sleep for the control loop. On the robot this is pros::delay, which lets
// other tasks run. The QEMU emulator never wakes a sleeping task (its timer
// interrupt is incomplete), so emulator builds (tools/sim.sh, -DROGUE_SIM)
// busy-wait instead. Use this everywhere instead of pros::delay.
void sleep_ms(std::uint32_t ms);

// Startup trace. Prints `step` to the console, the brain screen at (250, 200)
// and the controller screen, then holds it for `hold_ms` so a person can read
// it. If the program dies at start, the last step shown is where it stopped.
void debugStep(const char* step, std::uint32_t hold_ms = 600);

// Competition mode as short text, e.g. "drive none", "off switch",
// "auto field". First word: drive (enabled), auto (autonomous), off
// (disabled). Second word: what is controlling it: field (field control),
// switch (competition switch), none (nothing plugged into the controller).
const char* competitionText();

// Raw competition bits from the brain, for the console.
unsigned competitionBits();

// Input debug screen, like the controller overlay in Ocarina of Time's debug
// mode: the 12 buttons light up green while held, both sticks show a dot,
// with the exact values and what the drive motors receive. full = true
// clears the screen and draws everything; false only redraws what changed.
void drawInputScreen(bool full);

// Prints a console line every time a button or stick changes, e.g.
// "rogue: input A L1 | LX 0 LY 64 RX 0 RY 0". Call it every loop.
void logInputChanges();

// TEMPORARY. Spins the robot in place at `power` (out of 127) for a few
// seconds, measures the real speed of each side, and sets the drive trims so
// both sides turn at the same speed. Put the robot on the floor with room to
// spin. Press B during the run to cancel. Results go to the console and the
// screen; copy them into motorSetTrim calls to keep them after a restart.
// Returns true when trims were applied.
bool autoCalibrate(int power = 100);
