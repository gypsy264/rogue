#pragma once
// Game controller input. In initialize(): define where the controller is,
// then gamepadInit(). After that, read the sticks and buttons from anywhere.
//
//   controllerDefinePort(1);                       // cable into smart port 1
//   controllerDefinePort(CONTROLLER_WIRELESS);     // or through the radio
//   gamepadInit();
//
//   int forward = gamepadLeftY();                  // -127 to 127, deadzone applied
//   if (gamepadHeld(GamepadButton::R1)) { ... }    // true while held down
//   if (gamepadPressed(GamepadButton::A)) { ... }  // true once per press
//   gamepadRumble(".-");                           // short, long buzz
//
// Until gamepadInit() runs, every read returns 0 / false, so nothing can touch
// the controller before the brain is ready.

constexpr int CONTROLLER_WIRELESS = 0;

enum class GamepadButton { A, B, X, Y, Up, Down, Left, Right, L1, L2, R1, R2 };
enum class GamepadAxis { LeftX, LeftY, RightX, RightY };

// Where the controller is connected: a smart port 1 to 21 when tethered with a
// cable, or CONTROLLER_WIRELESS. The brain treats both as the main controller;
// the port is checked so it is not also used by a motor. Call before gamepadInit().
void controllerDefinePort(int port);

// deadzone: stick values closer to 0 than this read as 0, so a worn stick
// that rests at 3 or 4 does not creep the robot.
void gamepadInit(int deadzone = 5);

// True when the brain can hear the controller, by cable or by radio.
bool gamepadConnected();

// "radio", "cable" or "off": how the controller is linked right now.
const char* gamepadLinkText();

// Sticks, -127 to 127. Up and right are positive.
int gamepadLeftX();
int gamepadLeftY();
int gamepadRightX();
int gamepadRightY();

// Stick value exactly as the brain receives it, no deadzone. For debugging.
int gamepadRawAxis(GamepadAxis axis);

// Short name of a button: "A", "Up", "L1"...
const char* gamepadButtonName(GamepadButton button);

// True as long as the button is held.
bool gamepadHeld(GamepadButton button);

// True only on the first check after the button goes down. Check each button
// from one place in the code, a second check of the same press returns false.
bool gamepadPressed(GamepadButton button);

// Vibration pattern: '.' short, '-' long, ' ' pause. Up to 8 characters.
void gamepadRumble(const char* pattern);

// Text on the controller's own screen, line 0 to 2, up to 15 characters.
// The controller only takes one update every 50 ms; faster calls are skipped.
void gamepadPrint(int line, const char* text);
