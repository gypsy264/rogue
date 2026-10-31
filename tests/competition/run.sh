#!/bin/sh
set -eu
MATCH_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$MATCH_ROOT"
mkdir -p build/tests
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -Itests/competition/fakes -Iinclude \
  src/main.cpp src/handlers/autonomousController.cpp src/handlers/calibrationController.cpp \
  src/handlers/calibrationScreen.cpp src/handlers/driverController.cpp src/handlers/gamepadController.cpp \
  src/handlers/motorController.cpp src/handlers/mechanismController.cpp src/handlers/mechanismScreen.cpp \
  src/handlers/supportController.cpp src/handlers/cameraScreen.cpp tests/competition/competition_test.cpp \
  -o build/tests/competition_test
build/tests/competition_test > build/tests/competition.log
tail -n 1 build/tests/competition.log
