#!/usr/bin/env bash
# Thin launcher for C++ runner; netem itself stays Linux-specific.
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ ! -x build/scripts/b_experiment_runner ]]; then
  echo "Build first: cmake -S . -B build && cmake --build build -j" >&2
  exit 1
fi
exec ./build/scripts/b_experiment_runner "$@"
