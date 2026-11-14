#!/bin/sh
# configure, build and run (same as the VS Code "Build & Run" task)
set -e
cd "$(dirname "$0")"
cmake -S . -B build
cmake --build build -j
./build/IsoMateric/bin/IsoMateric "$@"
