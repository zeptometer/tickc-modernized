#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "$script_dir/../../.." && pwd)
image=${LINUX_X86_64_IMAGE:-tcc-linux-x86-64:trixie}
profile=${1:-debug}

case "$profile" in
    debug|optimized|sanitizer|valgrind) ;;
    *)
        echo "usage: $0 [debug|optimized|sanitizer|valgrind]" >&2
        exit 2
        ;;
esac

if [ "${LINUX_X86_64_SKIP_IMAGE_BUILD:-0}" != 1 ]; then
    "$script_dir/build-image.sh"
fi

docker run --rm --platform linux/amd64 \
    --network none \
    --cap-drop ALL \
    --security-opt no-new-privileges \
    --volume "$repo_dir:/workspace:ro" \
    "$image" \
    /workspace/env/linux-x86_64/scripts/run-tests-in-container.sh "$profile"
