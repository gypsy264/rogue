#pragma once

void runtimeInit();

// Labels must outlive the program (use literals).
void runtimeStep(const char* label);
void runtimeHeartbeat();

void runtimeIdle(const char* label);

void runtimeError(const char* label);
