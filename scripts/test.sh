#!/usr/bin/env bash
set -euo pipefail

echo "Running configure/build checks..."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

echo "All checks completed."
