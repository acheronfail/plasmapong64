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
    ./tools/build-rom.sh -j4 SMOKE=1 ROM=plasmapong-smoke BUILD_DIR=build/smoke_rsp

# Per-stage fluid timings in the emulator debug log; no production overhead.
benchmark:
    ./tools/build-rom.sh -j4 SMOKE=1 FLUID_PROFILE=1 ROM=plasmapong-benchmark BUILD_DIR=build/benchmark_rsp

# CPU reference backend for comparison with the default RSP pressure solver.
cpu:
    ./tools/build-rom.sh -j4 FLUID_RSP=0 ROM=plasmapong-cpu BUILD_DIR=build/cpu

benchmark-cpu:
    ./tools/build-rom.sh -j4 FLUID_RSP=0 SMOKE=1 FLUID_PROFILE=1 ROM=plasmapong-cpu-benchmark BUILD_DIR=build/benchmark_cpu

# Compatibility recipe for a separately named ROM with the default backend.
rsp:
    ./tools/build-rom.sh -j4 FLUID_RSP=1 ROM=plasmapong-rsp BUILD_DIR=build/rsp

# Bit-exact pressure checks at boot, then scripted gameplay with stage timings.
benchmark-rsp:
    ./tools/build-rom.sh -j4 FLUID_RSP=1 RSP_TEST=1 SMOKE=1 FLUID_PROFILE=1 ROM=plasmapong-rsp-benchmark BUILD_DIR=build/rsp_benchmark

# Exercise one-controller arcade mode against the real AI in Ares.
smoke-arcade:
    ./tools/build-rom.sh -j4 SMOKE=1 SMOKE_ARCADE=1 ROM=plasmapong-arcade-smoke BUILD_DIR=build/arcade_smoke_rsp

# Dedicated EEPROM fixture ROM: run twice to verify persistence across boots.
smoke-save:
    ./tools/build-rom.sh -j4 SMOKE_SAVE=1 ROM=plasmapong-save-smoke BUILD_DIR=build/save_smoke_rsp
