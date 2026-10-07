# Fluid performance development loop

The connected test system is an NTSC-J N64 and SummerCart64, currently firmware
2.20.2, with an 8 MiB Expansion Pak. The N64's physical power switch stays on.
Its ESPHome outlet URL is configured by `N64_POWER_URL`, entity `switch/switch`.
The user authorizes ROM uploads and explicit outlet on/off commands for development.
Use this interface instead of desktop automation to power the console.

## Run an experiment

Copy `.env.example` to `.env` and set `N64_POWER_URL` to your outlet URL.
The tools read this file automatically; an exported environment variable takes
precedence. `.env` is ignored by Git. Keep local outlet addresses out of tracked
source and documentation. `--power-url` (or `--url` on the power tool) overrides
the configured value. Emulator-only runs do not require an outlet setting.

```sh
just dev-loop baseline-20261008
just dev-loop integer-forces --define YOUR_IMPLEMENTED_FLAG=1
just dev-loop candidate-stages --profile
just dev-loop candidate-effects --workload effects --windows 60
just n64-power status
just n64-power on
just n64-power off
```

The example `YOUR_IMPLEMENTED_FLAG` is a placeholder: implement a candidate build
flag before using it. Every experiment name must be new. Candidate settings use
repeated `--define KEY=VALUE` arguments. `--skip-check` is available only when the
current source already passed portable checks. `--emulator-only` omits console
upload and power control. Python entry points work without `just`:
`python3 tools/dev-loop.py NAME` and `python3 tools/n64_power.py status`.

The loop runs portable gameplay/numerical tests, builds a separate RDP-validation
ROM, and runs scripted gameplay in Ares with both 8 MiB and 4 MiB memory. It then
builds the benchmark ROM, retains a copy, runs that exact ROM in Ares, and measures
it on the console. Default workload is continuous four-player jets/suction in
high-resolution tails mode. `--workload 2p`, `menu`, and `effects` select other
deterministic scenarios. Effects cycle every 15 simulated seconds: use enough
windows to cover the full cycle (60 windows at full speed).

Inputs come from `tests/rom_smoke.h` inside the test ROM, including actual menu
navigation, so neither emulator nor console requires desktop key injection.
Do not use X11/KDE input automation. Interactive visual QA can use Ares's own
controls and native Capture Screenshot command. The existing runner's optional
X11 screenshot path is not used by this loop. There is no automated visual
acceptance or console video capture in this setup yet.

## Hardware sequencing

1. Confirm outlet state, send `turn_off`, verify OFF, wait two seconds.
2. Upload to SummerCart RAM with direct boot; no SD menu selection is needed.
3. Start the USB debugger before turning power on so startup messages are captured.
4. Send `turn_on`, verify ON, capture timing/presentation/audio windows.
5. Stop the debugger and turn power OFF. Capture failures also attempt power-off.

`tools/benchmark-hardware.py --upload --power-control` provides the
same sequencing for an existing ROM. The explicit serial port is
`serial:///dev/ttyUSB0`; change `--port` if device enumeration changes.
RAM upload selects direct boot on the cartridge. `just deploy` remains the separate
SD-card deployment workflow and requires power OFF. The automated loop normally
leaves the console off; `--leave-on` on the hardware capture tool leaves it running
after success. Network, Docker, USB and Ares display access may require sandbox
escalation; use the authorized tool paths, without changing desktop permissions.

## Evidence and decisions

Each run retains `build/dev-loop/NAME/`: source revision/status/hashes, tracked
source patch, build settings/commands, build logs, validation and exact-ROM Ares
logs, the uploaded ROM and SHA-256, and raw hardware USB logs plus JSON summaries.
An incomplete run records failure in `manifest.json`; reuse neither its name nor
its capture files. Validation and profiling ROMs are separate from the ordinary
hardware benchmark because instrumentation changes scheduling and execution cost.
Ares timings are useful for regression checks; console measurements decide speed.

Prioritize simulation milliseconds and completed-frame headroom, not just FPS:
FPS is capped at about 59.94 on this console. Check presentation repeats, maximum
VI gap and audio underruns as well. High resolution is 640x480 interlaced with a
new framebuffer each NTSC field, not 60 complete two-field scans per second.

The aim is affordable, believable currents with predictable ball response,
leaving room for features and a finer fluid grid. Approximate candidates need not
match the old solver bit for bit. They must stay bounded, preserve smooth ball
sampling, maintain visible transport in the same direction as ball forces, and
avoid persistent wall leaks, quantization sticking, or unstable jets/suction.
Use current portable interaction tests plus deterministic visual/playback checks;
adapt exact-oracle tests deliberately when changing the numerical method.

Compare one change at a time against the same workload/settings. First measure
force kernels, flow sampling and pressure solver alternatives at 48x33; then
spend demonstrated headroom on higher resolution. Grid sizes are currently baked
into CPU/RSP layouts and fixture assumptions, so increasing resolution requires
updating those together rather than only changing `FW`/`FH`.

## Initial console control, 2026-10-08

The first automatic capture, `build/dev-loop/baseline-hardware.json`, recorded
59.9409 FPS, 9.9919 ms/update across nine simulation windows, 2.2229 ms draw
submission across ten rendering windows, 1,490 new PLAY frames / 1,490 VI, and no
repeats or audio underruns. It used the shipping 48x33 grid and scripted 4P stress.
Draw submission is not standalone RDP execution time and cannot establish all
remaining frame headroom by subtraction. The subsequent named complete-loop run
retains build/source provenance and separate 4/8 MiB validation.
