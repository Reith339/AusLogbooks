#!/bin/bash
# Builds dist/PilotLogbook-<version>-macOS.dmg (universal: Apple Silicon + Intel).
# Run on a Mac with Xcode command-line tools, CMake, Ninja and Qt 6 (qmake/macdeployqt on PATH,
# e.g. via the Qt online installer or GitHub Actions' install-qt-action).
# Set ARCHS=arm64 to build for Apple Silicon only (needed if your Qt isn't universal, e.g. Homebrew).
set -euo pipefail
cd "$(dirname "$0")/.."

BUILD=build-macos
mkdir -p "$BUILD" dist

# ---- App icon (.icns) from the 1024px PNG ----
ICONSET="$BUILD/LogBook.iconset"
rm -rf "$ICONSET"
mkdir -p "$ICONSET"
for s in 16 32 128 256 512; do
  sips -z $s $s resources/icon_1024.png --out "$ICONSET/icon_${s}x${s}.png" >/dev/null
  sips -z $((s * 2)) $((s * 2)) resources/icon_1024.png --out "$ICONSET/icon_${s}x${s}@2x.png" >/dev/null
done
iconutil -c icns "$ICONSET" -o resources/LogBook.icns

# ---- Build ----
# Google Sheets backup credentials come from GOOGLE_CLIENT_ID / GOOGLE_CLIENT_SECRET (or google-oauth.cmake).
GOOGLE_OPTS=()
if [ -n "${GOOGLE_CLIENT_ID:-}" ]; then
  GOOGLE_OPTS=(-DGOOGLE_CLIENT_ID="$GOOGLE_CLIENT_ID" -DGOOGLE_CLIENT_SECRET="${GOOGLE_CLIENT_SECRET:-}")
fi

cmake -S . -B "$BUILD" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  ${GOOGLE_OPTS[@]+"${GOOGLE_OPTS[@]}"} \
  -DCMAKE_OSX_ARCHITECTURES="${ARCHS:-arm64;x86_64}" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0
cmake --build "$BUILD"
VERSION=$(cat "$BUILD/version.txt")

# ---- Bundle Qt into the app ----
APP="$BUILD/Pilot Logbook.app"
rm -rf "$APP"
cp -R "$BUILD/LogBook.app" "$APP"
macdeployqt "$APP" -always-overwrite
# Only SQLite is used; drop other SQL drivers (they need libraries that aren't bundled).
find "$APP/Contents/PlugIns/sqldrivers" -type f ! -name 'libqsqlite*' -delete 2>/dev/null || true

# Ad-hoc signature: required for the app to launch on Apple Silicon. (Without an Apple
# Developer ID + notarisation, users must approve the app the first time they open it.)
codesign --force --deep --sign - "$APP"

# ---- Disk image with an Applications shortcut for drag-to-install ----
STAGING="$BUILD/dmg"
rm -rf "$STAGING"
mkdir -p "$STAGING"
cp -R "$APP" "$STAGING/"
ln -s /Applications "$STAGING/Applications"
DMG="dist/PilotLogbook-$VERSION-macOS.dmg"
rm -f "$DMG"
hdiutil create -volname "Pilot Logbook" -srcfolder "$STAGING" -ov -format UDZO "$DMG"
echo "Built: $DMG"
