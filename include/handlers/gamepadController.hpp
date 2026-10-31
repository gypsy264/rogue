#pragma once

constexpr int CONTROLLER_WIRELESS = 0;

enum class GamepadButton { A, B, X, Y, Up, Down, Left, Right, L1, L2, R1, R2 };
enum class GamepadAxis { LeftX, LeftY, RightX, RightY };

// Wiring note only; input uses MASTER.
void controllerDefinePort(int port);

void gamepadInit(int deadzone = 5);
void gamepadSetAxisPercent(int percent);

bool gamepadConnected();

const char* gamepadLinkText();

int gamepadLeftX();
int gamepadLeftY();
int gamepadRightX();
int gamepadRightY();

int gamepadRawAxis(GamepadAxis axis);

const char* gamepadButtonName(GamepadButton button);

bool gamepadHeld(GamepadButton button);

// Read each button from one place.
bool gamepadPressed(GamepadButton button);

void gamepadRumble(const char* pattern);

void gamepadPrint(int line, const char* text);
