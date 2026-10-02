#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build
cc -std=c11 -O3 -Wall -Wextra -Werror -pedantic -Isrc src/fluid_advection.c src/fluid_dye_fixed.c tests/dye_test.c -lm -o build/dye-test
./build/dye-test
cc -std=c11 -O3 -Wall -Wextra -Werror -pedantic -Isrc src/fluid_advection.c tests/advection_test.c -o build/advection-test
./build/advection-test
cc -std=c11 -O2 -Wall -Wextra -Werror -pedantic -Isrc src/fluid.c src/fluid_advection.c tests/fluid_reference.c tests/fluid_test.c -lm -o build/fluid-test
./build/fluid-test
cc -std=c11 -O2 -Wall -Wextra -Werror -pedantic -Isrc src/game.c src/arcade.c src/fluid.c src/fluid_advection.c tests/game_test.c -lm -o build/game-test
./build/game-test > build/game-check.log
cat build/game-check.log
cc -std=c11 -O2 -Wall -Wextra -Werror -pedantic -Isrc src/game.c src/arcade.c src/fluid.c src/fluid_advection.c tests/arcade_test.c -lm -o build/arcade-test
./build/arcade-test
cc -std=c11 -O2 -Wall -Wextra -Werror -pedantic -Isrc src/save.c tests/save_test.c -o build/save-test
./build/save-test
cc -std=c11 -O2 -Wall -Wextra -Werror -pedantic -Isrc src/game.c src/arcade.c src/fluid.c src/fluid_advection.c src/ui.c tools/preview.c -lm -o build/preview
for state in menu play lobby paused finished arcade gameover scores level overcharge; do
    ./build/preview "$state" > "build/preview-$state.svg"
done

cc -std=c11 -O2 -Wall -Wextra -Werror -pedantic -Isrc src/sound.c tests/sound_test.c -o build/sound-test
./build/sound-test

# Exercise persistent fixed-point storage with the portable integer oracle.
cc -std=c11 -O3 -Wall -Wextra -Werror -pedantic -DPLASMAPONG_DYE_FIXED -Isrc src/fluid.c src/fluid_advection.c src/fluid_dye_fixed.c tests/fluid_reference.c tests/fluid_test.c -lm -o build/fluid-fixed-test
./build/fluid-fixed-test
cc -std=c11 -O3 -Wall -Wextra -Werror -pedantic -DPLASMAPONG_DYE_FIXED -Isrc src/game.c src/arcade.c src/fluid.c src/fluid_advection.c src/fluid_dye_fixed.c tests/game_test.c -lm -o build/game-fixed-test
./build/game-fixed-test > build/game-fixed-check.log
cat build/game-fixed-check.log
test "$(sed -n '/^Physics trace hash:/p' build/game-check.log)" = "$(sed -n '/^Physics trace hash:/p' build/game-fixed-check.log)"
echo "PASS: float and fixed dye produce identical 120-second physics traces"
cc -std=c11 -O3 -Wall -Wextra -Werror -pedantic -DPLASMAPONG_DYE_FIXED -Isrc src/game.c src/arcade.c src/fluid.c src/fluid_advection.c src/fluid_dye_fixed.c tests/arcade_test.c -lm -o build/arcade-fixed-test
./build/arcade-fixed-test

# Experimental velocity: signed interpolation plus quantization-aware gameplay.
cc -std=c11 -O3 -Wall -Wextra -Werror -pedantic -Isrc src/fluid_advection.c src/fluid_dye_fixed.c src/fluid_velocity_fixed.c tests/velocity_test.c -lm -o build/velocity-test
./build/velocity-test
cc -std=c11 -O3 -Wall -Wextra -Werror -pedantic -DPLASMAPONG_DYE_FIXED -DPLASMAPONG_VELOCITY_FIXED -Isrc src/game.c src/arcade.c src/fluid.c src/fluid_advection.c src/fluid_dye_fixed.c src/fluid_velocity_fixed.c tests/game_test.c -lm -o build/game-velocity-test
./build/game-velocity-test
cc -std=c11 -O3 -Wall -Wextra -Werror -pedantic -DPLASMAPONG_DYE_FIXED -DPLASMAPONG_VELOCITY_FIXED -Isrc src/game.c src/arcade.c src/fluid.c src/fluid_advection.c src/fluid_dye_fixed.c src/fluid_velocity_fixed.c tests/arcade_test.c -lm -o build/arcade-velocity-test
./build/arcade-velocity-test
