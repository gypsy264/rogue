#!/bin/sh
# Build the project and run it in the vex-v5-qemu emulator.
# Requires: pros-cli, arm-none-eabi toolchain, Rust nightly, QEMU,
# and a clone of https://github.com/vexide/vex-v5-qemu next to this repo.
set -e
HERE="$(cd "$(dirname "$0")/.." && pwd)"
SIM="${VEX_SIM_DIR:-$HERE/../vex-v5-qemu}"
export PATH="$HOME/.local/opt/arm-gnu-toolchain-14.3.rel1-darwin-arm64-arm-none-eabi/bin:/opt/homebrew/opt/rustup/bin:$HOME/.cargo/bin:$HOME/.local/bin:$PATH"

# ROGUE_SIM swaps pros::delay for a busy wait (see sleep_ms in supportController).
# Every source, subfolders included, is touched before the emulator build so the
# flag reaches all of them, and again after it so the next plain `pros make`
# rebuilds everything for the robot instead of reusing emulator objects.
cd "$HERE"
find src -name '*.cpp' -exec touch {} +
pros make EXTRA_CXXFLAGS=-DROGUE_SIM
sleep 1  # make compares whole seconds; make sure sources end up newer than objects
find src -name '*.cpp' -exec touch {} +
# Local fix for the emulator: normal-size text stayed large after a large print.
PATCH="$HERE/tools/vex-v5-qemu-textsize.patch"
if git -C "$SIM" apply --check "$PATCH" 2>/dev/null; then git -C "$SIM" apply "$PATCH"; fi

cd "$SIM" && cargo xtask run --release --pros=hot-cold --program "$HERE" "$@"
