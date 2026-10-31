# rogue
VEX V5 robot, written in C++ on [PROS](https://pros.cs.purdue.edu/) kernel 4.2.2.

## Layout
- `src/main.cpp` - robot code: `initialize`, `autonomous`, `opcontrol`
- `src/handlers/` and `include/handlers/` - helpers shared by all robot code:
  - `autonomousController`: starts unified calibration once per autonomous entry
  - `cameraController`: AI Vision sensing, object memory, calibration, and camera debug screen
  - `mechanismController`: manual claw/elevator control and travel measurements
  - `driverController`: 80%/100% toggle, held precision mode, and manual drive
  - `calibrationController`: bounded drive/mechanism/camera calibration
  - `screenController`: `writeScreen("text %d", x, y, value)`, `writeScreenLarge`, `showError`, `clearScreen`
  - `motorController`: `motorDefinePort(DriveSide::Left, 10)` for each drive motor, `motorSetMaxRpm(200)` for the cartridge, then `motorInit()`; drive with `driveArcade`, `driveTank`, `driveStop`. Uses speed control (`move_velocity`) with a per-side trim
  - `gamepadController`: `gamepadInit()` reads the logical MASTER controller; VEXos detects cable/radio automatically. `controllerDefinePort(1)` is only a wiring note, not input configuration. Sticks with `gamepadLeftY()` and friends, buttons with `gamepadHeld` and `gamepadPressed`, `gamepadRumble`
  - `supportController`: `sleep_ms`, input diagnostics, and competition status
  - `runtimeController`: persistent screen status and a heartbeat monitor, started before device setup
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

## Startup and runtime status

`initialize()` starts diagnostics before setting up devices and no longer adds
600 ms delays between startup steps. The bottom screen row is reserved for a
task that stays alive across competition modes. `clearScreen()` preserves it.

- `LIVE` plus uptime, mode, and current step confirms that diagnostics are running.
- `DISABLED | waiting for enable` or `competition: waiting` explains why driver
  code is not running. Competition can prevent driving without preventing the UI.
- `AUTO` accompanies the autonomous calibration screen, with its current phase and results.
- `STALL` shows the last watched step when no heartbeat arrives for over 1.5 seconds.
- Startup motor errors remain in the status row even when the main page changes.

Call `runtimeStep("operation")` before work that could block, and
`runtimeHeartbeat()` each iteration of a long loop. Use `runtimeIdle("reason")`
for intentional waits after a callback returns. Labels must be string literals.
The monitor observes; it does not enable competition, restart the program, or
automatically stop motors. A frozen kernel, failed display, or failure before
`initialize()` cannot be guaranteed to show a new message. If the drawing lock
is stuck, the monitor reports that to the terminal instead of waiting forever.
The independent watchdog is disabled in the current QEMU emulator, whose timer
does not reliably support sleeping tasks; its screen explicitly says so.

Host fault-injection checks: `sh tests/runtime/run.sh`.

## Match controls

- Left stick Y: forward/back. Right stick X: turn.
- L1: toggle manual drive between 80% (default at each driver entry) and 100%.
- Hold R1: all four joystick getters use a 20% scale. Release it to return to
  the L1 setting. R1 takes precedence even with the 80% limit turned off.
- D-pad Left/Right: claw open/close. Up/Down: elevator raise/lower.
- B: start unified calibration from any driver page; release and press again
  to cancel. During autonomous, B can only cancel, not restart the sequence.
- A, X, Y, L2, R2: no action assigned.

The drive cap is applied after arcade mixing, so pushing forward and turn
simultaneously cannot exceed the selected cap. R1 scales joysticks only; claw
and elevator keep their configured powers. Raw input diagnostics remain raw.
The mechanisms and status pages show the active drive percentage. Driver inputs
are suspended during calibration. After it ends, manual control resumes and
its results stay on screen until a tap returns to the debug pages.

## Unified calibration / autonomous

Entering enabled autonomous starts one calibration sequence. B runs the same
sequence during driver control. It is a nonblocking state machine updated every
20 ms, with a 10-second budget to leave margin inside a 15-second autonomous
period. Finishing early leaves the robot stopped for the rest of autonomous.
This routine measures the drive; it does not implement a scoring action.

1. Stop the mechanisms and camera range calibration.
2. Command drive at +30/-30 on the -127..127 scale, without existing trims:
   settle for 1 second, then measure both sides for 2 seconds. At 200 RPM these
   commands request approximately +47/-47 RPM. `CALIBRATION_SPIN_POWER` sets
   this speed for both autonomous and B. Missing motors or repeated
   invalid/near-zero readings stop this stage and mark it failed.
3. Stop the drive and wait 500 ms. Set the current claw/elevator encoder positions
   as their zero references. This is not automatic homing; there are no known
   physical endpoints, and neither mechanism is driven toward a stop.
4. Request camera range calibration if a measured reference was configured.
   A missing reference is reported as skipped. Sensor errors are reported as
   failed; an absent target can wait only until the overall budget expires.

With the current zero camera-reference settings, the normal sequence takes
about 3.5 seconds. A configured camera needs its known target face-on, centered,
at the measured distance, with robot and target stationary after the spin.
White balance remains automatic on camera connection.

The drive candidate is `trim = slower average RPM / side average RPM`, clamped
by the existing 0.5..1.0 trim range. Previous trims remain in use until the routine
finishes normally or partially with valid drive readings. Cancellation, budget
expiry, or drive failure retain the previous trims. Partial results report which
stage failed. Calibrating motor RPM does not measure heading, wheel slip, wheel
diameter, or physical robot drift.

Disabled/mode changes stop outputs and cancel pending calibration; driver-side
controller loss does the same. Autonomous does not require a connected gamepad.
Every competition callback cancels any previous run on entry, covering PROS task
replacement. The camera uses request IDs so an old successful result cannot be
mistaken for completion of a new request. Cancellation clears queued requests.

`sh tests/competition/run.sh` runs the real main/mode callbacks and handlers with
fake hardware, sanitizers, exhaustive forward/turn caps, four-axis scaling,
held-button/toggle checks, unchanged D-pad powers, controller/read failures,
mode changes, disabled handling, camera request matching, timeout and clock-wrap
cases. This checks software behavior, not mechanical performance on the Brain.
Competition status follows the [PROS API](https://pros.cs.purdue.edu/v5/pros-4/namespacepros_1_1competition.html).

## Claw and elevator measurement

Ports, test power, and scales are near the top of `src/main.cpp`:

- `CLAW_PORT = -11`: physical port 11, reversed so D-pad Left opens and Right closes.
- `ELEVATOR_PORT = -5`: physical port 5, reversed so D-pad Up raises and Down lowers.
- `CLAW_GEAR_RATIO = 18`: configure the claw for the green 18:1 (200 RPM) cartridge.
- `CLAW_TEST_POWER = 45`, `ELEVATOR_TEST_POWER = 50` on the -127..127 voltage-command scale.
- `CLAW_DEGREES_PER_100` and `ELEVATOR_DEGREES_PER_100` initially equal 360.

These controls work in driver mode on every debug page. Release the button to
stop and hold position; opposite directions together stop that mechanism.
Controller loss, disabled/autonomous entry, and B's calibration sequence
stop both mechanisms. No position limits or automatic homing are configured.
Power is adjustable independently of travel. If a direction is backwards, negate
that motor's port in `main.cpp`; its position sign reverses too.

The default screen now shows both mechanisms: signed percent, encoder degrees,
minimum/maximum percent seen since zero, command, and scale. Tap cycles
mechanisms → controller → camera → status → mechanisms. These readings come
from the motor encoders, not elapsed button time or a physical opening sensor.

`percent = (position - zero) * 100 / degreesPer100`. Initially, 446.4 encoder
degrees reads 124%; negative values and values above 100% are allowed. The scale
is a measurement reference, not the unknown maximum opening or height. The
claw cartridge is explicitly configured as 18:1; the elevator's existing
cartridge configuration is retained. `mechanismSetGearRatio(...)` accepts 6, 18,
or 36 before initialization and reapplies the setting after reconnecting. It must
match the physical cartridge. Reported encoder degrees depend on that setting
and are not degrees of the claw jaws or millimeters of elevator travel.

The claw test power was raised from 25 to 45 because the initial setting may
not overcome the mechanism's load. This is not a confirmed diagnosis. With a
direction held, its screen should show `cmd +45` or `cmd -45`; `motor offline`
indicates a connection or API error. Configuring 18:1 alone does not increase the
voltage command or change the physical gearing.

The position at program startup becomes zero. To compare travel consistently,
move to your chosen closed/lowered reference, release the D-pad, then run B's
unified calibration. It spins the drive first, stops, and then zeros both
mechanism readouts and their extrema at their current positions.
After a motor disconnect/read fault, reconnect it and set zero again; the screen
shows `B: calibrate` instead of presenting stale measurements. Missing motors
are reported on this page and do not prevent the rest of the program from starting.
Measurements reset when the program restarts.

`sh tests/mechanisms/run.sh` checks held inputs, reversal, release/disconnect
stops, read/command faults, scales beyond 100%, zeroing, and screen bounds using
fake motors. Verify the physical directions and useful test power on the robot.
Motor commands use the [PROS motor API](https://pros.cs.purdue.edu/v5/pros-4/group__cpp-motors.html).

## AI Vision camera

The camera uses `cameraDefinePort(CAMERA_AUTO_PORT)` in `initialize()` because
port 11 is now the claw motor. Connect the camera to a free smart port, or change
that call to its explicit port. `CAMERA_AUTO_PORT` scans for one AI Vision Sensor;
multiple cameras require an explicit port. Setup runs in a separate task and
never waits in `initialize()` for hardware or a calibration target.

The task reads tags, color signatures, and color codes with a 10 ms polling
target (100 Hz) in all competition modes. Work time is included in that budget;
overruns yield instead of spinning. It runs below the control task's priority.
Existing sensor color definitions are preserved. To
define colors in code, call `cameraDefineColor(id, r, g, b, hueRange,
saturationRange)` before `cameraInit()`, using values measured in the VEX AI
Vision Utility. It does not guess colors or train an AI model. The selected
tag family is `CameraFamily::Tag16H5`; change `cameraSetTagFamily(...)` to match
your printed tags. The V5 sensor exposes 320x240 pixel coordinates.

The initial driver/disabled page shows the mechanisms. Tap cycles mechanisms →
controller inputs → camera → robot status. Autonomous shows the unified calibration progress and results. Boxes are detections, not a live video feed: green for tags, blue
for colors/codes, grey for lost objects. The details show the largest visible
object (or a remembered one), its ID, local track number, pixel position, size,
age, estimated depth, and connection/calibration errors. B runs the shared
calibration sequence from this page too.

The debug display targets about 30 FPS in driver, disabled, and autonomous modes.
`Poll Hz` measures successful sensor reads and `UI FPS` measures camera-page
redraws over one-second windows; both start at zero while collecting samples.
These are measured software rates, not guaranteed hardware rates. Polling stops
counting during warmup/errors, and stale snapshots report zero. Driver control
keeps its 20 ms target independently of the display's 33 ms schedule.

The sensor camera runs at 30 FPS; colors usually reach 30 FPS, while AprilTags
typically run at 5–15 FPS depending on the scene, according to
[VEX's firmware engineer](https://www.vexforum.com/t/frame-rate-of-ai-vision-sensor/142464).
Faster polling reduces the wait to read available results; it cannot increase
the sensor's processing rate. The API has no capture counter, so repeated reads
are not labeled as new camera frames. AI model detection remains disabled.

### Camera reference calibration

White balance starts automatically on connection, followed by a 1.2-second
warmup. Metric depth needs a reference; its settings are initially zero so
unconfigured distances display `Z --`.

1. Set `CAMERA_REFERENCE_TAG`, `CAMERA_REFERENCE_WIDTH_MM`, and
   `CAMERA_REFERENCE_Z_MM` near the top of `src/main.cpp`.
2. Measure the tag's square between its detected corners (exclude the outer
   white margin). Place it face-on and centered, at the specified distance
   from the camera lens. Keep the robot and target still.
3. Enter autonomous or press B in driver mode. After the drive stage, the camera
   collects 12 stable readings at least 50 ms apart after
   drive commands have been zero for at least 500 ms. These are time-spaced
   readings, not guaranteed distinct frames. Missing, duplicate, off-center, clipped,
   tiny, tilted, or unstable references cannot complete calibration.
4. The shared 10-second budget cancels an unfinished camera calibration.
   Reposition the reference at the **same measured distance**, then retry B.

It estimates `focalPx = apparentWidthPx * referenceZMm / actualWidthMm`, then
uses `Z = focalPx * actualWidthMm / apparentWidthPx`. Tag width comes from its
four edge lengths; color/code width comes from the bounding box. Register the
real width for **each** tag/color/code ID with `cameraDefineTarget(...)` before
`cameraInit()`. Unknown sizes remain 2D even after the camera is calibrated.
Two objects sharing a color ID must have the same real width for this estimate.

Calibration is a simple size/range estimate: it assumes equal horizontal and
vertical focal lengths, an optical center of (160,120), and no lens distortion.
Tilt, occlusion, lighting-dependent color boundaries, or a wrong reference
measurement can bias depth. This is not a full camera intrinsic/pose calibration.
White balance is separate from distance calibration. Zero drive commands do not
prove physical stillness if the robot is pushed or coasting.

The console prints `cameraSetFocalLength(...)` after calibration; add that call
before `cameraInit()` to reuse a measured value on future boots. A configured
focal length supplies depth immediately. Unified calibration can replace it
when a reference is configured. A failed recalibration retains the previous focal length.
No SD card writes or persistence of object positions is performed.

### Shared detections

`cameraSnapshot(snapshot)` copies up to 32 tracked objects. AprilTags match by
ID; color/code blobs use nearby detections of the same signature to keep
separate local track IDs. Similar objects crossing can still swap identity.
Lost detections retain their last measurement for up to 5 seconds, with their
original timestamp. A color blob lost for over 250 ms receives a new local
track on return. Polling freshness cannot detect a sensor internally repeating
the same image: PROS does not expose a frame counter through this API.

`cameraFind(kind, id, object)` returns only a currently visible, recent reading
(at most 250 ms old by default). For multiple matching blobs it selects the
largest. Check `depthValid` before using `xMm`, `yMm`, or `zMm`. X is right, Y is
down, and Z is forward **relative to the camera at detection time**. These are
not field coordinates or a map updated as the robot moves. Motion/odometry and
camera mounting geometry are needed before adding that capability.

The camera handler sends no motor commands. The existing `driveTank()` handler
reports movement so camera calibration pauses during driving and motor
calibration. Future autonomous/assistance routines can use the same snapshot
API without owning the sensor or its task.

Validation: `sh tests/camera/run.sh` tests geometry, calibration, association,
expiry, disconnect/reconnect, API errors, and port selection with fake hardware.
Physical camera accuracy and the screen still need verification on the Brain.

References: [PROS AI Vision](https://pros.cs.purdue.edu/v5/pros-4/aivision.html),
[VEX V5 AI Vision API](https://api.vex.com/v5/home/cpp/Smart_Port_Devices/AI_Vision_Sensor.html),
[OpenCV camera model](https://docs.opencv.org/4.5.5/d9/d0c/group__calib3d.html).

## Controller diagnostic on the real brain

The user confirmed that the original input problem was a controller bug and
that input now works. The isolated diagnostic below is retained for future
troubleshooting. The previous logs showed a live program in `disabled()` with
a radio link and zero inputs; a link alone does not prove usable input.

Corrections to the previous diagnosis:
- `controllerDefinePort(1)` never configured an input port. PROS selects
  MASTER or PARTNER; VEXos handles the physical connection.
- The PROS Controller constructor only stores an ID; it does not access hardware.
- `debugStep("disabled")` also wrote that text to the controller LCD, so that
  text alone was not independent evidence of the controller's actual mode.
- The `0x01` competition status *is* the disabled bit from VEXos. Calling our
  driver loop from `disabled()` did not enable anything; that workaround is
  removed. The input display remains active while disabled.
- Axis errors used to be converted silently to zero. The handler now logs
  failed reads and errno. The separate diagnostic preserves every failed read.

Build the isolated diagnostic:

```sh
sh tools/controller-check.sh
```

Connect the brain to the Mac over USB, then upload to an available slot. This
example **replaces slot 8**; use another number if slot 8 contains needed code:

```sh
sh tools/controller-check.sh upload 8
pros terminal
```

Optional explicit serial port: `sh tools/controller-check.sh upload 8 /dev/cu.usbmodem...`.
Select **ctrl-check** on the brain and press Run. Hold A and move both sticks.
The brain displays both MASTER and PARTNER, button names, raw axes, link state,
and mode. The terminal logs changes plus a heartbeat each second. Error records
include errno and sample index: 0=link, 1..4=LX/LY/RX/RY,
5..16=L1/L2/R1/R2/Up/Down/Left/Right/X/Y/A/B.

This program uses direct PROS C reads in a task started by `initialize()`, so
it can observe inputs across mode changes. It has no rogue handlers, motor
commands, controller LCD writes, or competition overrides. It builds its own
monolithic binary under `build/controller-check/`, without reusing the emulator
or normal robot objects. Its upload runs outside the project because CLI 3.5.6
can otherwise prefer the auto-detected project's binary over an explicit file.

Interpret the result:
- Inputs change in MASTER: the basic PROS/VEXos input path works; compare the
  normal project's handler/mode behavior next.
- Only PARTNER changes: investigate the controller's main/partner role.
- `ERROR ... errno=...`: a failed API read, not a centered stick.
- Both stay zero with a nonzero link and no errors: the failure reproduces
  without our handlers. Compare the built-in Drive program with the same
  controller and connection; record whether its inputs work and whether this
  diagnostic reports disabled. This still does not by itself prove broken hardware.

References: [PROS 4.2.2 C++ controller implementation](https://github.com/purduesigbots/pros/blob/4.2.2/src/devices/controller.cpp),
[PROS input and competition reads](https://github.com/purduesigbots/pros/blob/4.2.2/src/devices/controller.c),
[VEX engineer's explanation of competition status](https://www.vexforum.com/t/competition-code-and-user-created-tasks/88616).

## Emulate on the Mac
`tools/sim.sh` builds the project and runs it in
[vex-v5-qemu](https://github.com/vexide/vex-v5-qemu), which emulates the
brain's CPU and opens a window with the brain screen. It expects the emulator
repo cloned at `../vex-v5-qemu` (override with `VEX_SIM_DIR`).

- `printf` output appears in the terminal, so use it for feedback.
- Add `--gdb` to pause at start and attach a debugger on port 1234.
- Draw on the screen with `writeScreen` from `handlers/screenController.hpp`. The
  PROS LCD (`pros::lcd`) only renders its first strip in the emulator (upstream
  issue 49), so do not use it.
- `pros::delay` never returns in the emulator because its timer interrupt is
  incomplete. `tools/sim.sh` builds with `-DROGUE_SIM`, which makes the
  `sleep_ms` helper in `supportController` busy-wait instead. Always call `sleep_ms`,
  never `pros::delay` directly, so both builds behave.
- Motors, sensors and the controller read zero because nothing is attached,
  so the screen always shows "no motor" in the emulator. On the robot that
  message means a cable is loose.
- `tools/vex-v5-qemu-textsize.patch` fixes an emulator bug where normal text
  stayed large after a large print. `tools/sim.sh` applies it automatically.

## Docs
- PROS tutorials: https://pros.cs.purdue.edu/v5/tutorials/index.html
- PROS API: https://pros.cs.purdue.edu/v5/api/index.html
