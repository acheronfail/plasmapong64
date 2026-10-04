#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")/.."
image=n64-util-toolchain:e356bf3
# The image and library revision are pinned in tools/Dockerfile.
docker build -t "$image" -f tools/Dockerfile tools
for arg in "$@"; do
    if [ "$arg" = RDP_WAIT_TRACE=1 ]; then
        image=n64-util-toolchain:e356bf3-rdp-trace
        docker build -t "$image" -f tools/Dockerfile.trace tools
    fi
done
docker run --rm --user "$(id -u):$(id -g)" \
    --mount "type=bind,source=$(pwd),target=/work" "$image" make "$@"
