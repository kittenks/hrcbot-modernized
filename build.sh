#!/usr/bin/env bash
# =============================================================================
# HRCBot modernized - local Linux build script.
#
# Builds the Metamod:Source 1.12 plugin for Half-Life 2: Deathmatch on Linux,
# targeting both x86 (32-bit) and x86_64 (64-bit) in one pass.
#
# Requirements:
#   - python3 with AMBuild 2.x installed (pip install ambuild)
#   - gcc/g++ (multilib for 32-bit: gcc-multilib g++-multilib libc6-dev-i386)
#   - hl2sdk hl2dm branch and metamod-source 1.12-dev (see --setup)
#
# Usage:
#   ./build.sh [all|x86|x64] [--setup]
#
# Environment overrides:
#   HL2SDK_HL2DM   path to the hl2sdk hl2dm checkout
#   MMS_PATH       path to the metamod-source 1.12 checkout
#   BUILD_DIR      build directory (default: obj-linux)
# =============================================================================
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

ARCH_ARG="${1:-all}"
SETUP=0
for a in "$@"; do
	[ "$a" = "--setup" ] && SETUP=1
done

BUILD_DIR="${BUILD_DIR:-$ROOT/obj-linux}"
DEPS_DIR="$ROOT/deps"
HL2SDK_HL2DM="${HL2SDK_HL2DM:-$DEPS_DIR/hl2sdk-hl2dm}"
MMS_PATH="${MMS_PATH:-$DEPS_DIR/metamod-source}"

HL2SDK_BRANCH="hl2dm"
MMS_BRANCH="1.12-dev"

setup_deps() {
	mkdir -p "$DEPS_DIR"
	if [ ! -d "$HL2SDK_HL2DM" ]; then
		echo "[setup] cloning hl2sdk ($HL2SDK_BRANCH)..."
		git clone --depth 1 -b "$HL2SDK_BRANCH" \
			https://github.com/alliedmodders/hl2sdk.git "$HL2SDK_HL2DM"
	fi
	if [ ! -d "$MMS_PATH" ]; then
		echo "[setup] cloning metamod-source ($MMS_BRANCH)..."
		git clone --depth 1 -b "$MMS_BRANCH" \
			https://github.com/alliedmodders/metamod-source.git "$MMS_PATH"
	fi
}

if [ "$SETUP" -eq 1 ]; then
	setup_deps
fi

# Validate dependencies.
if [ ! -f "$HL2SDK_HL2DM/lib/public/linux64/tier1.a" ]; then
	echo "ERROR: hl2sdk hl2dm not found at $HL2SDK_HL2DM"
	echo "Run '$0 --setup' to clone it, or set HL2SDK_HL2DM to your checkout."
	exit 1
fi
if [ ! -d "$MMS_PATH/core" ]; then
	echo "ERROR: metamod-source not found at $MMS_PATH"
	echo "Run '$0 --setup' to clone it, or set MMS_PATH to your checkout."
	exit 1
fi

# Ensure AMBuild is importable (it is not on PyPI; install from upstream).
if ! python3 -c "import ambuild2" >/dev/null 2>&1; then
	echo "AMBuild not found; installing from upstream..."
	python3 -m pip install git+https://github.com/alliedmodders/ambuild
fi

# Select the architecture list passed to configure.py (AMBuild then emits one
# plugin binary per requested architecture).
case "$ARCH_ARG" in
	x86)   AM_ARCH="x86" ;;
	x64)   AM_ARCH="x64" ;;
	all|"") AM_ARCH="x86,x64" ;;
	*) echo "Unknown architecture '$ARCH_ARG' (use x86, x64 or all)"; exit 1 ;;
esac

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "[configure] arch=$AM_ARCH"
python3 "$ROOT/configure.py" \
	--sdks hl2dm \
	--hl2sdk-hl2dm "$HL2SDK_HL2DM" \
	--mms-path "$MMS_PATH" \
	--target-arch "$AM_ARCH"

echo "[build] ambuild"
ambuild

echo
echo "Build finished. Artifacts:"
find . -maxdepth 2 -name 'hrcbot_mm*.so' -type f -print
