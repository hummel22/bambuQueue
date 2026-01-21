#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build}"
CONFIG="${CONFIG:-Release}"
DIST_DIR="${DIST_DIR:-dist}"

APP_NAME="BambuQueue"
APP_DIR="${DIST_DIR}/${APP_NAME}.app"
CONTENTS_DIR="${APP_DIR}/Contents"
MACOS_DIR="${CONTENTS_DIR}/MacOS"
RESOURCES_DIR="${CONTENTS_DIR}/Resources"
EXECUTABLE="bambu_queue"

BIN="${BUILD_DIR}/${EXECUTABLE}"
if [[ -f "${BUILD_DIR}/${CONFIG}/${EXECUTABLE}" ]]; then
  BIN="${BUILD_DIR}/${CONFIG}/${EXECUTABLE}"
fi

if [[ ! -x "${BIN}" ]]; then
  echo "Executable not found at ${BIN}. Run ./scripts/build_macos.sh first." >&2
  exit 1
fi

mkdir -p "${MACOS_DIR}" "${RESOURCES_DIR}"
cp "${BIN}" "${MACOS_DIR}/${EXECUTABLE}"

cat > "${CONTENTS_DIR}/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>en</string>
    <key>CFBundleDisplayName</key>
    <string>BambuQueue</string>
    <key>CFBundleExecutable</key>
    <string>bambu_queue</string>
    <key>CFBundleIdentifier</key>
    <string>com.bambu.queue</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>BambuQueue</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>0.1.0</string>
    <key>CFBundleVersion</key>
    <string>0.1.0</string>
    <key>LSMinimumSystemVersion</key>
    <string>12.0</string>
</dict>
</plist>
PLIST

DMG_NAME="${DIST_DIR}/${APP_NAME}.dmg"
rm -f "${DMG_NAME}"

if command -v create-dmg >/dev/null 2>&1; then
  create-dmg \
    --volname "${APP_NAME}" \
    --window-size 520 320 \
    --icon-size 120 \
    --app-drop-link 360 160 \
    "${DMG_NAME}" \
    "${APP_DIR}"
else
  hdiutil create -volname "${APP_NAME}" -srcfolder "${APP_DIR}" -ov -format UDZO "${DMG_NAME}"
fi

echo "Created DMG: ${DMG_NAME}"
