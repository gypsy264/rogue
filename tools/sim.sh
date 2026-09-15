#!/bin/sh
# Build the project and run it in the vex-v5-qemu emulator.
# Requires: pros-cli, arm-none-eabi toolchain, Rust nightly, QEMU,
# and a clone of https://github.com/vexide/vex-v5-qemu next to this repo.
set -e
HERE="$(cd "$(dirname "$0")/.." && pwd)"
SIM="${VEX_SIM_DIR:-$HERE/../vex-v5-qemu}"
export PATH="$HOME/.local/opt/arm-gnu-toolchain-14.3.rel1-darwin-arm64-arm-none-eabi/bin:/opt/homebrew/opt/rustup/bin:$HOME/.cargo/bin:$HOME/.local/bin:$PATH"

# ROGUE_SIM swaps pros::delay for a busy wait (see sleep_ms in main.cpp).
# Sources are touched so the flag change always recompiles them.
cd "$HERE" && touch src/*.cpp && pros make EXTRA_CXXFLAGS=-DROGUE_SIM
cd "$SIM" && cargo xtask run --release --pros=hot-cold --program "$HERE" "$@"
