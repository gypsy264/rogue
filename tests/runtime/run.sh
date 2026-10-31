#!/bin/sh
set -eu
RUNTIME_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$RUNTIME_ROOT"
mkdir -p build/tests
c++ -std=c++17 -Wall -Wextra -Werror -Itests/runtime/fakes -Iinclude \
  src/handlers/runtimeController.cpp tests/runtime/runtime_test.cpp \
  -o build/tests/runtime_test
build/tests/runtime_test
