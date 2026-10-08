#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build
# Portable execution of the official fixed-point solver; RSP fixtures run in Ares.
fluid_sources="src/fluid.c src/fluid_upwind.c src/fluid_dye_fixed.c"
flags="-std=c11 -O3 -Wall -Wextra -Werror -pedantic -Isrc"
for scenario in game arcade multiplayer fps point_capacity view force suction_lookup; do
    ui_sources=""
    if [ "$scenario" = point_capacity ]; then ui_sources="src/ui.c"; fi
    cc $flags src/game.c src/arcade.c $fluid_sources $ui_sources "tests/${scenario}_test.c" -lm -o "build/${scenario}-test"
    "./build/${scenario}-test"
done
for scenario in upwind reciprocal_limit; do
    cc $flags src/fluid_upwind.c src/fluid_dye_fixed.c "tests/${scenario}_test.c" -lm -o "build/${scenario}-test"
    "./build/${scenario}-test"
done
cc $flags tests/gradient_reciprocal_test.c -o build/gradient-reciprocal-test
./build/gradient-reciprocal-test
cc $flags tests/perf_test.c -o build/perf-test
./build/perf-test
cc $flags src/save.c tests/save_test.c -o build/save-test
./build/save-test
cc $flags -DPLASMAPONG_SOUND_STEADY src/sound.c tests/sound_test.c -o build/sound-test
./build/sound-test
cc $flags src/music.c tests/music_test.c -o build/music-test
./build/music-test
cc $flags -DPLASMAPONG_MENU_STAMPS $fluid_sources tests/menu_stamp_test.c -lm -o build/menu-stamp-test
./build/menu-stamp-test
cc $flags src/game.c src/arcade.c $fluid_sources src/ui.c tools/preview.c -lm -o build/preview
for state in menu menu-4p menu-options options play lobby paused finished arcade gameover scores level overcharge 3p 4p; do
    ./build/preview "$state" > "build/preview-$state.svg"
done
python3 tests/dev_loop_test.py
