#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
env_dir=$(CDPATH= cd -- "$script_dir/.." && pwd)
image=${LINUX_X86_64_IMAGE:-tcc-linux-x86-64:trixie}

docker build --platform linux/amd64 --tag "$image" "$env_dir"
