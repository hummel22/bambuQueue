#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build}"
CONFIG="${CONFIG:-Release}"

APP="${BUILD_DIR}/bambu_queue"
if [[ -f "${BUILD_DIR}/${CONFIG}/bambu_queue" ]]; then
  APP="${BUILD_DIR}/${CONFIG}/bambu_queue"
fi

if [[ ! -x "${APP}" ]]; then
  echo "Executable not found at ${APP}. Run ./scripts/build_macos.sh first." >&2
  exit 1
fi

exec "${APP}"
