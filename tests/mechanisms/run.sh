#!/bin/sh
set -eu
MECHANISM_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$MECHANISM_ROOT"
mkdir -p build/tests
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -Itests/mechanisms/fakes -Iinclude \
  src/handlers/mechanismController.cpp src/handlers/mechanismScreen.cpp tests/mechanisms/mechanism_test.cpp \
  -o build/tests/mechanism_test
build/tests/mechanism_test
build/tests/mechanism_test missing
