#!/usr/bin/env bash
# =============================================================================
# HRCBot modernized - packaging script.
#
# Modes:
#   (default)              build Linux (if needed) and stage the Linux package
#   --prebuilt <dir>       reuse binaries already built under <dir>
#   --windows  <dir>       stage a Windows zip from binaries built under <dir>
#   --source                create the source tarball only
#
# Outputs go to dist/.
# =============================================================================
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

VERSION="$(tr -d '[:space:]' < product.version)"
NAME="hrcbot-${VERSION}"
DIST="$ROOT/dist"
STAGE="$DIST/.stage"
mkdir -p "$DIST"

MODE="linux"
BUILD_DIR=""

while [ $# -gt 0 ]; do
	case "$1" in
		--source)   MODE="source"; shift ;;
		--prebuilt) MODE="linux";  BUILD_DIR="$2"; shift 2 ;;
		--windows)  MODE="windows"; BUILD_DIR="$2"; shift 2 ;;
		-h|--help)  grep '^#' "$0" | head -20; exit 0 ;;
		*) echo "Unknown option: $1"; exit 1 ;;
	esac
done

# Locate a built binary under a directory tree.
find_bin() {
	local dir="$1" want="$2"
	find "$dir" -type f \( -name "$want" \) 2>/dev/null | head -1
}

stage_common() {
	local dest="$1"
	mkdir -p "$dest/addons/hrcbot_server_plugin"
	# Metamod plugin registry + bot data.
	cp assets/addons/hrcbot_mm.vdf "$dest/addons/"
	cp assets/addons/hrcbot_server_plugin/hrcbot_names.txt "$dest/addons/hrcbot_server_plugin/"
	# Preserve the original (legacy) waypoint files for archival/credit; the
	# modern build writes fresh .hrcbot2 data at runtime.
	cp assets/addons/hrcbot_server_plugin/*.hrcbot "$dest/addons/hrcbot_server_plugin/" 2>/dev/null || true
	# Documentation.
	cp README.md README.zh-CN.md LICENCE "$dest/"
	mkdir -p "$dest/docs"
	cp -r docs/original "$dest/docs/"
}

if [ "$MODE" = "source" ]; then
	echo "[package] creating source tarball"
	OUT="$DIST/${NAME}-source.tar.gz"
	tar --exclude='./obj-*' --exclude='./deps' --exclude='./dist' \
	    --exclude='./.git' --exclude='./.github' --exclude='*.pyc' \
	    --exclude='__pycache__' \
	    -czf "$OUT" -C "$ROOT" \
	    --transform "s,^\.,$NAME," \
	    .
	ls -lh "$OUT"
	exit 0
fi

if [ "$MODE" = "linux" ]; then
	if [ -z "$BUILD_DIR" ] || [ ! -d "$BUILD_DIR" ]; then
		BUILD_DIR="$ROOT/obj-linux"
		if [ ! -d "$BUILD_DIR" ]; then
			echo "[package] no prebuilt tree, running ./build.sh all"
			./build.sh all
		fi
	fi
	X64="$(find_bin "$BUILD_DIR" 'hrcbot_mm.x64.so')"
	X86="$(find_bin "$BUILD_DIR" 'hrcbot_mm_i486.so')"
	[ -n "$X64" ] || { echo "ERROR: x64 .so not found under $BUILD_DIR"; exit 1; }
	[ -n "$X86" ] || { echo "ERROR: x86 .so not found under $BUILD_DIR"; exit 1; }

	rm -rf "$STAGE"; mkdir -p "$STAGE/$NAME"
	stage_common "$STAGE/$NAME"
	cp "$X64" "$STAGE/$NAME/addons/hrcbot_mm.x64.so"
	cp "$X86" "$STAGE/$NAME/addons/hrcbot_mm_i486.so"

	echo "[package] creating Linux tarball"
	OUT="$DIST/${NAME}-linux.tar.gz"
	tar -czf "$OUT" -C "$STAGE" "$NAME"
	rm -rf "$STAGE"
	ls -lh "$OUT"
	exit 0
fi

if [ "$MODE" = "windows" ]; then
	[ -n "$BUILD_DIR" ] || BUILD_DIR="$ROOT/obj-windows"
	DLL="$(find_bin "$BUILD_DIR" 'hrcbot_mm.dll')"
	DLL64="$(find_bin "$BUILD_DIR" 'hrcbot_mm.x64.dll')"
	[ -n "$DLL" ] || { echo "ERROR: win32 .dll not found under $BUILD_DIR"; exit 1; }
	[ -n "$DLL64" ] || { echo "ERROR: win64 .dll not found under $BUILD_DIR"; exit 1; }

	rm -rf "$STAGE"; mkdir -p "$STAGE/$NAME"
	stage_common "$STAGE/$NAME"
	cp "$DLL" "$STAGE/$NAME/addons/hrcbot_mm.dll"
	cp "$DLL64" "$STAGE/$NAME/addons/hrcbot_mm.x64.dll"

	echo "[package] creating Windows zip"
	OUT="$DIST/${NAME}-windows.zip"
	( cd "$STAGE" && zip -rq "$OUT" "$NAME" )
	rm -rf "$STAGE"
	ls -lh "$OUT"
	exit 0
fi
