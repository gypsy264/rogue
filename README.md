# rogue
VEX V5 robot, written in C++ on [PROS](https://pros.cs.purdue.edu/) kernel 4.2.2.

## Layout
- `src/main.cpp` - robot code: `initialize`, `autonomous`, `opcontrol`
- `include/main.h` - project header, include it from every source file
- `include/pros/` and `firmware/` - PROS kernel API and libraries (do not edit)
- `project.pros`, `Makefile`, `common.mk` - PROS build configuration
- `tools/sim.sh` - build and run in the QEMU emulator

## Toolchain (macOS, Apple Silicon)
Installed outside Homebrew, no sudo needed:
- PROS CLI 3.5.6 via `pipx install pros-cli` (in `~/.local/bin`)
- Arm GNU Toolchain 14.3.rel1 unpacked in `~/.local/opt/` (kernel 4.2.2 needs GCC 14 for `gnu++26`)
- Rust nightly plus QEMU (Homebrew) for the emulator

`~/.zshrc` puts all of them on the PATH. The PROS VS Code extension also
works with this project; it just needs the same PATH.

## Build
```
pros make
```
Output lands in `bin/`. Upload to a brain with `pros upload`. Watch `printf`
output from the brain with `pros terminal`.

## Emulate on the Mac
`tools/sim.sh` builds the project and runs it in
[vex-v5-qemu](https://github.com/vexide/vex-v5-qemu), which emulates the
brain's CPU and opens a window with the brain screen. It expects the emulator
repo cloned at `../vex-v5-qemu` (override with `VEX_SIM_DIR`).

- `printf` output appears in the terminal, so use it for feedback.
- Add `--gdb` to pause at start and attach a debugger on port 1234.
- Known limit: the LCD (`pros::lcd`) only renders its first strip; the rest of
  the screen stays black. This is an open bug in the emulator (issue 49), not in
  this project. Motors and sensors read as zero because nothing is attached.

## Docs
- PROS tutorials: https://pros.cs.purdue.edu/v5/tutorials/index.html
- PROS API: https://pros.cs.purdue.edu/v5/api/index.html
