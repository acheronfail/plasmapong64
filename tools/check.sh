#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build
cc -std=c11 -O2 -Wall -Wextra -Werror -pedantic -Isrc src/game.c src/fluid.c tests/game_test.c -lm -o build/game-test
./build/game-test
cc -std=c11 -O2 -Wall -Wextra -Werror -pedantic -Isrc src/game.c src/fluid.c src/ui.c tools/preview.c -lm -o build/preview
for state in menu play lobby paused finished; do
    ./build/preview "$state" > "build/preview-$state.svg"
done

cc -std=c11 -O2 -Wall -Wextra -Werror -pedantic -Isrc src/sound.c tests/sound_test.c -o build/sound-test
./build/sound-test
