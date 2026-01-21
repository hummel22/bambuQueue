#!/usr/bin/env bash
set -euo pipefail

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "This setup script is intended for macOS." >&2
  exit 1
fi

if ! command -v brew >/dev/null 2>&1; then
  echo "Homebrew is required. Install from https://brew.sh/ and re-run." >&2
  exit 1
fi

brew update
brew install cmake wxwidgets sqlite curl mosquitto create-dmg

echo "Done. You can now run: make build"
