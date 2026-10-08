# Build with the same pinned libdragon toolchain as n64-util-rom.
build:
    ./tools/build-rom.sh -j4

# Compatibility alias; the 64x44 configuration is now the official build.
build-fluid64: build

# Original solver retained only for historical comparisons.
build-legacy:
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy ROM=plasmapong-legacy BUILD_DIR=build/legacy

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
    ./tools/build-rom.sh -j4 SMOKE=1 ROM=plasmapong-smoke BUILD_DIR=build/smoke_rsp_confinement

# Per-stage fluid timings in the emulator debug log; no production overhead.
benchmark:
    ./tools/build-rom.sh -j4 SMOKE=1 FLUID_PROFILE=1 ROM=plasmapong-benchmark BUILD_DIR=build/benchmark_rsp_confinement

# Historical hardware comparisons below retain the legacy solver for matched runs.
# Use dev-loop for new measurements of the official configuration.
# Real hardware menu baseline: USB logs, repeatable seed, no completion fence.
benchmark-hardware:
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy EXPANSION_BANKS=0 MENU_STAMPS=0 MENU_LABEL_BLOCK=0 FLUID_HIGHPRI=0 USB_LOG=1 BENCH_MENU=1 ROM=plasmapong-hardware BUILD_DIR=build/hardware_menu

# Diagnostic timings; the extra completion fence can alter presentation cadence.
benchmark-hardware-profile:
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy EXPANSION_BANKS=0 MENU_STAMPS=0 MENU_LABEL_BLOCK=0 FLUID_HIGHPRI=0 USB_LOG=1 BENCH_MENU=1 FLUID_PROFILE=1 FRAME_WORK_PROFILE=1 ROM=plasmapong-hardware-profile BUILD_DIR=build/hardware_menu_profile

# Stage timings with ordinary asynchronous RSP/RDP overlap.
benchmark-hardware-stages:
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy EXPANSION_BANKS=0 MENU_STAMPS=0 MENU_LABEL_BLOCK=0 FLUID_HIGHPRI=0 USB_LOG=1 BENCH_MENU=1 FLUID_PROFILE=1 ROM=plasmapong-hardware-stages BUILD_DIR=build/hardware_menu_stages

# Split earlier RSP queue work from velocity advection, without a full RDP fence.
benchmark-hardware-queue:
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy EXPANSION_BANKS=0 MENU_STAMPS=0 MENU_LABEL_BLOCK=0 FLUID_HIGHPRI=0 USB_LOG=1 BENCH_MENU=1 FLUID_PROFILE=1 QUEUE_PROFILE=1 ROM=plasmapong-hardware-queue BUILD_DIR=build/hardware_menu_queue

# Candidate: consolidate per-frame texture, particles and text command buffers.
benchmark-hardware-batch:
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy EXPANSION_BANKS=0 MENU_STAMPS=0 MENU_LABEL_BLOCK=0 FLUID_HIGHPRI=0 USB_LOG=1 BENCH_MENU=1 FRAME_BLOCK=1 ROM=plasmapong-hardware-batch BUILD_DIR=build/hardware_menu_batch

# Candidate: let simulation overtake queued drawing, retaining cached rendering.
benchmark-hardware-highpri:
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy EXPANSION_BANKS=0 MENU_STAMPS=0 MENU_LABEL_BLOCK=0 FLUID_HIGHPRI_YIELD=0 USB_LOG=1 BENCH_MENU=1 FLUID_HIGHPRI=1 ROM=plasmapong-hardware-highpri BUILD_DIR=build/hardware_menu_highpri

# Open high priority only for RSP batches; let rendering proceed during CPU work.
benchmark-hardware-yield:
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy USB_LOG=1 BENCH_MENU=1 EXPANSION_BANKS=1 MENU_STAMPS=1 MENU_LABEL_BLOCK=1 FLUID_HIGHPRI=1 FLUID_HIGHPRI_YIELD=1 ROM=plasmapong-hardware-yield BUILD_DIR=build/hardware_yield

# Hardware test of existing framebuffer bank placement; ordinary cached renderer.
benchmark-hardware-banks:
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy MENU_STAMPS=0 MENU_LABEL_BLOCK=0 FLUID_HIGHPRI=0 USB_LOG=1 BENCH_MENU=1 EXPANSION_BANKS=1 ROM=plasmapong-hardware-banks BUILD_DIR=build/hardware_menu_banks

# Measure a texture that fits TMEM; compare against the bank-placement control.
benchmark-hardware-ink16:
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy MENU_STAMPS=0 MENU_LABEL_BLOCK=0 FLUID_HIGHPRI=0 USB_LOG=1 BENCH_MENU=1 EXPANSION_BANKS=1 INK16=1 ROM=plasmapong-hardware-ink16 BUILD_DIR=build/hardware_menu_ink16

# Same texture experiment with direct RSP packing and asynchronous production.
benchmark-hardware-ink16-rsp:
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy MENU_STAMPS=0 MENU_LABEL_BLOCK=0 FLUID_HIGHPRI=0 USB_LOG=1 BENCH_MENU=1 EXPANSION_BANKS=1 INK16_RSP=1 ROM=plasmapong-hardware-ink16-rsp BUILD_DIR=build/hardware_menu_ink16_rsp

# Cache fixed menu source geometry; compare against the bank-placement control.
benchmark-hardware-menu-stamps:
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy MENU_LABEL_BLOCK=0 FLUID_HIGHPRI=0 USB_LOG=1 BENCH_MENU=1 EXPANSION_BANKS=1 MENU_STAMPS=1 ROM=plasmapong-hardware-menu-stamps BUILD_DIR=build/hardware_menu_stamps

# Keep invariant menu title and labels in one cached RDP command buffer.
benchmark-hardware-menu-labels:
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy FLUID_HIGHPRI=0 USB_LOG=1 BENCH_MENU=1 EXPANSION_BANKS=1 MENU_STAMPS=1 MENU_LABEL_BLOCK=1 ROM=plasmapong-hardware-menu-labels BUILD_DIR=build/hardware_menu_labels

# Attribute the remaining simulation cost without adding a full RDP fence.
benchmark-hardware-retained-stages:
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy FLUID_HIGHPRI=0 USB_LOG=1 BENCH_MENU=1 EXPANSION_BANKS=1 MENU_STAMPS=1 MENU_LABEL_BLOCK=1 FLUID_PROFILE=1 QUEUE_PROFILE=1 ROM=plasmapong-hardware-retained-stages BUILD_DIR=build/hardware_retained_stages

# Diagnostic only: retain simulation/particles/pixel generation, replace the backdrop.
benchmark-hardware-flat-fluid:
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy FLUID_HIGHPRI=0 USB_LOG=1 BENCH_MENU=1 EXPANSION_BANKS=1 MENU_STAMPS=1 MENU_LABEL_BLOCK=1 BENCH_FLAT_FLUID=1 ROM=plasmapong-hardware-flat-fluid BUILD_DIR=build/hardware_flat_fluid

# Stream texture and particle commands together, retain cached menu foreground.
benchmark-hardware-draw-stream:
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy FLUID_HIGHPRI=0 USB_LOG=1 BENCH_MENU=1 EXPANSION_BANKS=1 MENU_STAMPS=1 MENU_LABEL_BLOCK=1 DRAW_STREAM=1 ROM=plasmapong-hardware-draw-stream BUILD_DIR=build/hardware_draw_stream

# Pinned-libdragon diagnostic: retain streaming, allocate foreground in one buffer.
benchmark-hardware-menu-buffer:
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy FLUID_HIGHPRI=0 USB_LOG=1 BENCH_MENU=1 EXPANSION_BANKS=1 MENU_STAMPS=1 MENU_LABEL_BLOCK=1 DRAW_STREAM=1 MENU_BUFFER_KIB=32 ROM=plasmapong-hardware-menu-buffer BUILD_DIR=build/hardware_menu_buffer

# Retain the best cached renderer; signal completion after target cleanup.
benchmark-hardware-tail-sync:
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy FLUID_HIGHPRI=0 USB_LOG=1 BENCH_MENU=1 EXPANSION_BANKS=1 MENU_STAMPS=1 MENU_LABEL_BLOCK=1 RDP_TAIL_SYNC=1 ROM=plasmapong-hardware-tail-sync BUILD_DIR=build/hardware_tail_sync

# Sample RDP/SP status without halting the RSP; legacy recipe name retained.
benchmark-hardware-queue-pc:
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy FLUID_HIGHPRI=0 USB_LOG=1 BENCH_MENU=1 EXPANSION_BANKS=1 MENU_STAMPS=1 MENU_LABEL_BLOCK=1 FLUID_PROFILE=1 QUEUE_PROFILE=1 QUEUE_PC_PROFILE=1 ROM=plasmapong-hardware-queue-pc BUILD_DIR=build/hardware_queue_pc

# Passive markers inside the RSP wait loop; isolated tracing toolchain image.
benchmark-hardware-rdp-trace:
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy FLUID_HIGHPRI=0 USB_LOG=1 BENCH_MENU=1 EXPANSION_BANKS=1 MENU_STAMPS=1 MENU_LABEL_BLOCK=1 FLUID_PROFILE=1 QUEUE_PROFILE=1 QUEUE_PC_PROFILE=1 RDP_WAIT_TRACE=1 ROM=plasmapong-hardware-rdp-trace BUILD_DIR=build/hardware_rdp_trace

# Reuse one menu RDP buffer; update cache-aligned particle slots each frame.
benchmark-hardware-menu-template:
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy FLUID_HIGHPRI=0 USB_LOG=1 BENCH_MENU=1 EXPANSION_BANKS=1 MENU_STAMPS=1 MENU_LABEL_BLOCK=1 MENU_TEMPLATE=1 ROM=plasmapong-hardware-menu-template BUILD_DIR=build/hardware_menu_template

# Test high-res VI point sampling against the cached-menu control.
benchmark-hardware-vi-point:
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy MENU_LABEL_BLOCK=0 FLUID_HIGHPRI=0 USB_LOG=1 BENCH_MENU=1 EXPANSION_BANKS=1 MENU_STAMPS=1 HIRES_VI_POINT=1 ROM=plasmapong-hardware-vi-point BUILD_DIR=build/hardware_vi_point

# Validate the retained candidate in scripted four-player high-res tails gameplay.
benchmark-hardware-gameplay:
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy MENU_LABEL_BLOCK=0 FLUID_HIGHPRI=0 USB_LOG=1 SMOKE=1 SMOKE_PLAYERS=4 SMOKE_HIGH_RES=1 SMOKE_EFFECT=2 SMOKE_STRESS=1 EXPANSION_BANKS=1 MENU_STAMPS=1 ROM=plasmapong-hardware-gameplay BUILD_DIR=build/hardware_gameplay

# Matched four-player stress gameplay with yielding high-priority simulation.
benchmark-hardware-yield-gameplay:
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy MENU_LABEL_BLOCK=0 USB_LOG=1 SMOKE=1 SMOKE_PLAYERS=4 SMOKE_HIGH_RES=1 SMOKE_EFFECT=2 SMOKE_STRESS=1 EXPANSION_BANKS=1 MENU_STAMPS=1 FLUID_HIGHPRI=1 FLUID_HIGHPRI_YIELD=1 ROM=plasmapong-hardware-yield-play BUILD_DIR=build/hardware_yield_play

# Matched gameplay control without bank placement or menu source caching.
benchmark-hardware-gameplay-baseline:
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy MENU_LABEL_BLOCK=0 FLUID_HIGHPRI=0 USB_LOG=1 SMOKE=1 SMOKE_PLAYERS=4 SMOKE_HIGH_RES=1 SMOKE_EFFECT=2 SMOKE_STRESS=1 EXPANSION_BANKS=0 MENU_STAMPS=0 ROM=plasmapong-hardware-gameplay-baseline BUILD_DIR=build/hardware_gameplay_baseline

# Attempt RAM upload; deployer checks console state. Then boot the new ROM.
capture-hardware log="build/hardware/menu.log": benchmark-hardware
    python3 tools/benchmark-hardware.py plasmapong-hardware.z64 {{log}} --upload

# CPU reference backend for comparison with the default RSP pressure, dye, velocity, and confinement.
cpu:
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy FLUID_RSP=0 DYE_RSP=0 CONFINEMENT_RSP=0 VELOCITY_RSP=0 ROM=plasmapong-cpu BUILD_DIR=build/cpu

benchmark-cpu:
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy FLUID_RSP=0 DYE_RSP=0 CONFINEMENT_RSP=0 VELOCITY_RSP=0 SMOKE=1 FLUID_PROFILE=1 ROM=plasmapong-cpu-benchmark BUILD_DIR=build/benchmark_cpu

# Float dye comparison, retaining RSP pressure solving.
float-dye:
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy CONFINEMENT_RSP=0 VELOCITY_RSP=0 DYE_RSP=0 ROM=plasmapong-float BUILD_DIR=build/rsp

# Compatibility recipe for a separately named ROM with the default backend.
rsp:
    ./tools/build-rom.sh -j4 FLUID_RSP=1 ROM=plasmapong-rsp BUILD_DIR=build/rsp_confinement

# Bit-exact pressure checks at boot, then scripted gameplay with stage timings.
benchmark-rsp:
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy FLUID_RSP=1 RSP_TEST=1 SMOKE=1 FLUID_PROFILE=1 ROM=plasmapong-rsp-benchmark BUILD_DIR=build/rsp_confinement_benchmark

# Exact float-advection checks at boot, then profiled scripted gameplay.
benchmark-advection:
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy CONFINEMENT_RSP=0 VELOCITY_RSP=0 DYE_RSP=0 ADVECTION_TEST=1 SMOKE=1 FLUID_PROFILE=1 ROM=plasmapong-advection-benchmark BUILD_DIR=build/advection_benchmark

# Compatibility recipe for a separately named ROM with the default backend.
dye:
    ./tools/build-rom.sh -j4 DYE_RSP=1 ROM=plasmapong-dye BUILD_DIR=build/rsp_confinement

# Power off the N64 before uploading the separately named dye ROM.
deploy-dye: dye
    sc64deployer sd upload plasmapong-dye.z64 /CUSTOM/plasmapong.z64

benchmark-dye:
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy DYE_RSP=1 DYE_TEST=1 RSP_TEST=1 SMOKE=1 FLUID_PROFILE=1 ROM=plasmapong-dye-benchmark BUILD_DIR=build/dye_confinement_benchmark

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
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy VELOCITY_RSP=1 VELOCITY_TEST=1 DYE_TEST=1 RSP_TEST=1 SMOKE=1 FLUID_PROFILE=1 ROM=plasmapong-velocity-benchmark BUILD_DIR=build/velocity_confinement_benchmark

# Previous float-velocity implementation, retaining RSP pressure and dye.
float-velocity:
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy CONFINEMENT_RSP=0 VELOCITY_RSP=0 ROM=plasmapong-float-velocity BUILD_DIR=build/rsp_dye

# Previous float curl/confinement, retaining RSP pressure, dye, and velocity.
float-confinement:
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy CONFINEMENT_RSP=0 ROM=plasmapong-float-confinement BUILD_DIR=build/rsp_velocity

# Compatibility recipe for a separately named ROM with the default backend.
confinement:
    ./tools/build-rom.sh -j4 CONFINEMENT_RSP=1 ROM=plasmapong-confinement

# Power off the N64 before uploading the separately named confinement ROM.
deploy-confinement: confinement
    sc64deployer sd upload plasmapong-confinement.z64 /CUSTOM/plasmapong.z64

benchmark-confinement:
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy CONFINEMENT_RSP=1 CONFINEMENT_TEST=1 VELOCITY_TEST=1 DYE_TEST=1 RSP_TEST=1 SMOKE=1 FLUID_PROFILE=1 ROM=plasmapong-confinement-benchmark BUILD_DIR=build/confinement_benchmark

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

# Exact preparation/pressure/advection/confinement fixtures, then stage timings.
benchmark-prepare:
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy SMOKE=1 FLUID_PROFILE=1 VELOCITY_TEST=1 DYE_TEST=1 RSP_TEST=1 CONFINEMENT_TEST=1 ROM=plasmapong-prepare-benchmark BUILD_DIR=build/prepare_benchmark

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

# Exact RSP pixel fixtures, then all eight effects in high-res stress gameplay.
validate-views:
    ./tools/build-rom.sh -j4 FLUID_PRESET=legacy SMOKE=1 SMOKE_FLOW=1 SMOKE_PLAYERS=4 SMOKE_HIGH_RES=1 SMOKE_STRESS=1 VELOCITY_TEST=1 RDP_VALIDATE=1 ROM=plasmapong-views-validate BUILD_DIR=build/views_cycle_validate

# Navigate the actual Options selector and preview every mode for three seconds.
smoke-view-options:
    ./tools/build-rom.sh -j4 SMOKE_OPTIONS=1 SMOKE_HIGH_RES=1 RDP_VALIDATE=1 ROM=plasmapong-views-options BUILD_DIR=build/views_options

# Select an effect in Options, press B, then hold the matching main-menu preview.
smoke-menu-view effect="7":
    ./tools/build-rom.sh -j4 SMOKE_MENU_VIEW={{effect}} SMOKE_HIGH_RES=1 RDP_VALIDATE=1 ROM=plasmapong-menu-view-{{effect}} BUILD_DIR=build/menu_view_{{effect}}
