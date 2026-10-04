# Build with the same pinned libdragon toolchain as n64-util-rom.
build:
    ./tools/build-rom.sh -j4

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

# Build a separate, scripted two-player ROM for emulator/performance testing.
smoke:
    ./tools/build-rom.sh -j4 SMOKE=1 ROM=plasmapong-smoke BUILD_DIR=build/smoke_rsp_confinement

# Per-stage fluid timings in the emulator debug log; no production overhead.
benchmark:
    ./tools/build-rom.sh -j4 SMOKE=1 FLUID_PROFILE=1 ROM=plasmapong-benchmark BUILD_DIR=build/benchmark_rsp_confinement

# CPU reference backend for comparison with the default RSP pressure, dye, velocity, and confinement.
cpu:
    ./tools/build-rom.sh -j4 FLUID_RSP=0 DYE_RSP=0 CONFINEMENT_RSP=0 VELOCITY_RSP=0 ROM=plasmapong-cpu BUILD_DIR=build/cpu

benchmark-cpu:
    ./tools/build-rom.sh -j4 FLUID_RSP=0 DYE_RSP=0 CONFINEMENT_RSP=0 VELOCITY_RSP=0 SMOKE=1 FLUID_PROFILE=1 ROM=plasmapong-cpu-benchmark BUILD_DIR=build/benchmark_cpu

# Float dye comparison, retaining RSP pressure solving.
float-dye:
    ./tools/build-rom.sh -j4 CONFINEMENT_RSP=0 VELOCITY_RSP=0 DYE_RSP=0 ROM=plasmapong-float BUILD_DIR=build/rsp

# Compatibility recipe for a separately named ROM with the default backend.
rsp:
    ./tools/build-rom.sh -j4 FLUID_RSP=1 ROM=plasmapong-rsp BUILD_DIR=build/rsp_confinement

# Bit-exact pressure checks at boot, then scripted gameplay with stage timings.
benchmark-rsp:
    ./tools/build-rom.sh -j4 FLUID_RSP=1 RSP_TEST=1 SMOKE=1 FLUID_PROFILE=1 ROM=plasmapong-rsp-benchmark BUILD_DIR=build/rsp_confinement_benchmark

# Exact float-advection checks at boot, then profiled scripted gameplay.
benchmark-advection:
    ./tools/build-rom.sh -j4 CONFINEMENT_RSP=0 VELOCITY_RSP=0 DYE_RSP=0 ADVECTION_TEST=1 SMOKE=1 FLUID_PROFILE=1 ROM=plasmapong-advection-benchmark BUILD_DIR=build/advection_benchmark

# Compatibility recipe for a separately named ROM with the default backend.
dye:
    ./tools/build-rom.sh -j4 DYE_RSP=1 ROM=plasmapong-dye BUILD_DIR=build/rsp_confinement

# Power off the N64 before uploading the separately named dye ROM.
deploy-dye: dye
    sc64deployer sd upload plasmapong-dye.z64 /CUSTOM/plasmapong.z64

benchmark-dye:
    ./tools/build-rom.sh -j4 DYE_RSP=1 DYE_TEST=1 RSP_TEST=1 SMOKE=1 FLUID_PROFILE=1 ROM=plasmapong-dye-benchmark BUILD_DIR=build/dye_confinement_benchmark

# Exercise one-controller arcade mode against the real AI in Ares.
smoke-arcade:
    ./tools/build-rom.sh -j4 SMOKE=1 SMOKE_ARCADE=1 ROM=plasmapong-arcade-smoke BUILD_DIR=build/arcade_smoke_rsp_confinement

# Dedicated EEPROM fixture ROM: run twice to verify persistence across boots.
smoke-save:
    ./tools/build-rom.sh -j4 SMOKE_SAVE=1 ROM=plasmapong-save-smoke BUILD_DIR=build/save_smoke_rsp_confinement

# Compatibility recipe for a separately named ROM with the default backend.
velocity:
    ./tools/build-rom.sh -j4 VELOCITY_RSP=1 ROM=plasmapong-velocity

# Power off the N64 before uploading the separately named velocity ROM.
deploy-velocity: velocity
    sc64deployer sd upload plasmapong-velocity.z64 /CUSTOM/plasmapong.z64

benchmark-velocity:
    ./tools/build-rom.sh -j4 VELOCITY_RSP=1 VELOCITY_TEST=1 DYE_TEST=1 RSP_TEST=1 SMOKE=1 FLUID_PROFILE=1 ROM=plasmapong-velocity-benchmark BUILD_DIR=build/velocity_confinement_benchmark

# Previous float-velocity implementation, retaining RSP pressure and dye.
float-velocity:
    ./tools/build-rom.sh -j4 CONFINEMENT_RSP=0 VELOCITY_RSP=0 ROM=plasmapong-float-velocity BUILD_DIR=build/rsp_dye

# Previous float curl/confinement, retaining RSP pressure, dye, and velocity.
float-confinement:
    ./tools/build-rom.sh -j4 CONFINEMENT_RSP=0 ROM=plasmapong-float-confinement BUILD_DIR=build/rsp_velocity

# Compatibility recipe for a separately named ROM with the default backend.
confinement:
    ./tools/build-rom.sh -j4 CONFINEMENT_RSP=1 ROM=plasmapong-confinement

# Power off the N64 before uploading the separately named confinement ROM.
deploy-confinement: confinement
    sc64deployer sd upload plasmapong-confinement.z64 /CUSTOM/plasmapong.z64

benchmark-confinement:
    ./tools/build-rom.sh -j4 CONFINEMENT_RSP=1 CONFINEMENT_TEST=1 VELOCITY_TEST=1 DYE_TEST=1 RSP_TEST=1 SMOKE=1 FLUID_PROFILE=1 ROM=plasmapong-confinement-benchmark BUILD_DIR=build/confinement_benchmark

# Cycle all four flow effects during scripted play with RDP validation.
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

# Exact preparation/pressure/advection/confinement fixtures, then stage timings.
benchmark-prepare:
    ./tools/build-rom.sh -j4 SMOKE=1 FLUID_PROFILE=1 VELOCITY_TEST=1 DYE_TEST=1 RSP_TEST=1 CONFINEMENT_TEST=1 ROM=plasmapong-prepare-benchmark BUILD_DIR=build/prepare_benchmark

# Include the final RSP/RDP completion wait in the drawing timer (diagnostic only).
benchmark-complete:
    ./tools/build-rom.sh -j4 SMOKE=1 DRAW_SYNC_PROFILE=1 ROM=plasmapong-complete-benchmark BUILD_DIR=build/complete_benchmark

# Fixed SPEED effect, per-stage timings, and completed-frame drawing time.
benchmark-speed:
    ./tools/build-rom.sh -j4 SMOKE=1 SMOKE_EFFECT=3 FLUID_PROFILE=1 DRAW_SYNC_PROFILE=1 ROM=plasmapong-speed-benchmark BUILD_DIR=build/speed_benchmark
