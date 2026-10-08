# Build with the same pinned libdragon toolchain as n64-util-rom.
build:
    ./tools/build-rom.sh -j4

# Human-controlled development ROM with SummerCart AUX halt/reboot support.
build-reload:
    ./tools/build-rom.sh -j4 USB_LOG=1 SC64_RELOAD=1 ROM=plasmapong-reload BUILD_DIR=build/reload

# Power off the N64 to release its SD-card lock.
deploy: build
    sc64deployer sd upload plasmapong.z64 /CUSTOM/plasmapong.z64

# Portable gameplay, numerical stability checks, and a deterministic preview.
check:
    ./tools/check.sh

# Launch the existing Ares checkout, or pass another emulator executable.
emulate ares="../ares/build/rundir/bin/ares": build
    {{ares}} --setting Developer/HomebrewMode=true --system 'Nintendo 64' --no-file-prompt plasmapong.z64

clean:
    ./tools/build-rom.sh clean

# Complete reproducible loop: host checks, Ares validation (8/4 MiB), console capture.
# Experiment names are unique; logs and the uploaded ROM are retained in build/dev-loop/.
dev-loop name="baseline" *args:
    python3 tools/dev-loop.py {{name}} {{args}}

# Explicit remote outlet control; never toggle an unknown state.
n64-power action="status":
    python3 tools/n64_power.py {{action}}

# Build a separate, scripted two-player ROM for emulator/performance testing.
smoke:
    ./tools/build-rom.sh -j4 SMOKE=1 ROM=plasmapong-smoke BUILD_DIR=build/smoke_official

# Per-stage fluid timings in the emulator debug log; no production overhead.
benchmark:
    ./tools/build-rom.sh -j4 SMOKE=1 FLUID_PROFILE=1 ROM=plasmapong-benchmark BUILD_DIR=build/benchmark_official

# Exercise one-controller arcade mode against the real AI in Ares.
smoke-arcade:
    ./tools/build-rom.sh -j4 SMOKE=1 SMOKE_ARCADE=1 ROM=plasmapong-arcade-smoke BUILD_DIR=build/arcade_smoke_official

# Dedicated EEPROM fixture ROM: run twice to verify persistence across boots.
smoke-save:
    ./tools/build-rom.sh -j4 SMOKE_SAVE=1 ROM=plasmapong-save-smoke BUILD_DIR=build/save_smoke_official

# Cycle all eight flow effects during scripted play with RDP validation.
smoke-flow:
    ./tools/build-rom.sh -j4 SMOKE_FLOW=1 RDP_VALIDATE=1 ROM=plasmapong-flow-smoke BUILD_DIR=build/flow_smoke

# Change video modes through Options, then run scripted gameplay in high res.
smoke-video:
    ./tools/build-rom.sh -j4 SMOKE_VIDEO=1 RDP_VALIDATE=1 ROM=plasmapong-video-smoke BUILD_DIR=build/video_smoke

# Complete per-frame work, excluding intentional rate-limiter sleep.
benchmark-hires:
    ./tools/build-rom.sh -j4 SMOKE=1 SMOKE_PLAYERS=4 SMOKE_HIGH_RES=1 SMOKE_EFFECT=2 FRAME_WORK_PROFILE=1 ROM=plasmapong-hires-work BUILD_DIR=build/hires_work

# Matched high-res 2P replay for console-driven optimisation comparisons.
benchmark-hires-2p:
    ./tools/build-rom.sh -j4 SMOKE=1 SMOKE_PLAYERS=2 SMOKE_HIGH_RES=1 SMOKE_EFFECT=2 FRAME_WORK_PROFILE=1 ROM=plasmapong-hires-2p BUILD_DIR=build/hires_2p

# 2P continuous powers while cycling every flow effect; collect at least 24 windows.
benchmark-hires-2p-effects:
    ./tools/build-rom.sh -j4 SMOKE=1 SMOKE_PLAYERS=2 SMOKE_HIGH_RES=1 SMOKE_FLOW=1 SMOKE_STRESS=1 FRAME_WORK_PROFILE=1 ROM=plasmapong-hires-2p-effects BUILD_DIR=build/hires_2p_effects

# Test-only continuous powers, keeping every player alive and bypassing cooldowns.
benchmark-hires-stress:
    ./tools/build-rom.sh -j4 SMOKE=1 SMOKE_PLAYERS=4 SMOKE_HIGH_RES=1 SMOKE_EFFECT=2 SMOKE_STRESS=1 FRAME_WORK_PROFILE=1 ROM=plasmapong-hires-stress BUILD_DIR=build/hires_stress

# Continuous powers while cycling every flow display option.
benchmark-hires-effects:
    ./tools/build-rom.sh -j4 SMOKE=1 SMOKE_PLAYERS=4 SMOKE_HIGH_RES=1 SMOKE_FLOW=1 SMOKE_STRESS=1 FRAME_WORK_PROFILE=1 ROM=plasmapong-hires-effects BUILD_DIR=build/hires_effects

# Optional framebuffer bank placement; requires hardware timing comparison.
benchmark-expansion:
    ./tools/build-rom.sh -j4 EXPANSION_BANKS=1 SMOKE=1 SMOKE_PLAYERS=4 SMOKE_HIGH_RES=1 SMOKE_EFFECT=2 FRAME_WORK_PROFILE=1 ROM=plasmapong-expansion-work BUILD_DIR=build/expansion_work

# Include the final RSP/RDP completion wait in the drawing timer (diagnostic only).
benchmark-complete:
    ./tools/build-rom.sh -j4 SMOKE=1 DRAW_SYNC_PROFILE=1 ROM=plasmapong-complete-benchmark BUILD_DIR=build/complete_benchmark

# Fixed SPEED effect, per-stage timings, and completed-frame drawing time.
benchmark-speed:
    ./tools/build-rom.sh -j4 SMOKE=1 SMOKE_EFFECT=3 FLUID_PROFILE=1 DRAW_SYNC_PROFILE=1 ROM=plasmapong-speed-benchmark BUILD_DIR=build/speed_benchmark

# Human-controlled prototype: all eight effects in the existing Options selector.
prototype-views:
    ./tools/build-rom.sh -j4 USB_LOG=1 ROM=plasmapong-views BUILD_DIR=build/views_playable

# Matched 4P high-res stress; ordinary asynchronous renderer, console USB logs.
# Effect IDs: dye=0 speed=3 vortex=4 relief=5 bands=6 pressure=7.
benchmark-view effect="4":
    ./tools/build-views.sh benchmark "{{effect}}"

# Normal 2P replay for screenshots; low-res with RDP validation.
showcase-view effect="4":
    ./tools/build-views.sh showcase "{{effect}}"

# Navigate the actual Options selector and preview every mode for three seconds.
smoke-view-options:
    ./tools/build-rom.sh -j4 SMOKE_OPTIONS=1 SMOKE_HIGH_RES=1 RDP_VALIDATE=1 ROM=plasmapong-views-options BUILD_DIR=build/views_options

# Select an effect in Options, press B, then hold the matching main-menu preview.
smoke-menu-view effect="7":
    ./tools/build-rom.sh -j4 SMOKE_MENU_VIEW={{effect}} SMOKE_HIGH_RES=1 RDP_VALIDATE=1 ROM=plasmapong-menu-view-{{effect}} BUILD_DIR=build/menu_view_{{effect}}
