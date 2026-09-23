#pragma once
// Game controller input. Call gamepadInit() once in initialize(), then read
// the sticks and buttons from anywhere.
//
//   int forward = gamepadLeftY();                  // -127 to 127, deadzone applied
//   if (gamepadHeld(GamepadButton::R1)) { ... }    // true while held down
//   if (gamepadPressed(GamepadButton::A)) { ... }  // true once per press
//   gamepadRumble(".-");                           // short, long buzz

enum class GamepadButton { A, B, X, Y, Up, Down, Left, Right, L1, L2, R1, R2 };

// deadzone: stick values closer to 0 than this read as 0, so a worn stick
// that rests at 3 or 4 does not creep the robot.
void gamepadInit(int deadzone = 5);

bool gamepadConnected();

// Sticks, -127 to 127. Up and right are positive.
int gamepadLeftX();
int gamepadLeftY();
int gamepadRightX();
int gamepadRightY();

// True as long as the button is held.
bool gamepadHeld(GamepadButton button);

// True only on the first check after the button goes down. Check each button
// from one place in the code, a second check of the same press returns false.
bool gamepadPressed(GamepadButton button);

// Vibration pattern: '.' short, '-' long, ' ' pause. Up to 8 characters.
void gamepadRumble(const char* pattern);
