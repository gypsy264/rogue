#!/bin/sh
set -eu
CAMERA_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$CAMERA_ROOT"
mkdir -p build/tests
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude \
  tests/camera/timing_test.cpp -o build/tests/camera_timing_test
build/tests/camera_timing_test
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude \
  src/handlers/cameraTracking.cpp tests/camera/tracking_test.cpp \
  -o build/tests/camera_tracking_test
build/tests/camera_tracking_test
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -Itests/camera/fakes -Iinclude \
  src/handlers/cameraTracking.cpp src/handlers/cameraController.cpp tests/camera/controller_test.cpp \
  -o build/tests/camera_controller_test
build/tests/camera_controller_test > build/tests/camera_controller.log
tail -n 1 build/tests/camera_controller.log
build/tests/camera_controller_test auto > build/tests/camera_auto.log
tail -n 1 build/tests/camera_auto.log
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -Itests/camera/fakes -Iinclude \
  src/handlers/cameraScreen.cpp tests/camera/screen_test.cpp -o build/tests/camera_screen_test
build/tests/camera_screen_test
