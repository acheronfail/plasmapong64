#!/usr/bin/env sh
# Separate objects/ROMs for repeatable effect comparisons; no deployment.
set -eu
cd "$(dirname "$0")/.."
mode=${1:-benchmark}
choices=${2:-"0 3 4 5 6 7"}
mkdir -p build/views
for effect in $choices; do
    case "$effect" in
        0) name=dye ;; 3) name=speed ;; 4) name=vortex ;;
        5) name=relief ;; 6) name=bands ;; 7) name=pressure ;;
        *) echo "Expected effect 0, 3, 4, 5, 6 or 7" >&2; exit 1 ;;
    esac
    case "$mode" in
        benchmark)
            ./tools/build-rom.sh -j4 SMOKE=1 SMOKE_PLAYERS=4 SMOKE_HIGH_RES=1 SMOKE_STRESS=1 SMOKE_EFFECT="$effect" USB_LOG=1 \
                ROM="plasmapong-view-$name" BUILD_DIR="build/view_benchmark_$name" > "build/views/build-benchmark-$name.log" 2>&1 ;;
        showcase)
            ./tools/build-rom.sh -j4 SMOKE=1 SMOKE_PLAYERS=2 SMOKE_EFFECT="$effect" RDP_VALIDATE=1 \
                ROM="plasmapong-showcase-$name" BUILD_DIR="build/view_showcase_$name" > "build/views/build-showcase-$name.log" 2>&1 ;;
        *) echo "Expected benchmark or showcase" >&2; exit 1 ;;
    esac
    echo "Built $mode $name"
done
