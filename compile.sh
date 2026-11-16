#!/usr/bin/env bash
set -euo pipefail

if [ $# -eq 0 ]; then
    echo "No arguments supplied!"
    echo "Possible Arguments: $0 [TARGET_XM32, TARGET_WING, TARGET_PC_SDL2]"
    exit 1
fi
BUILD_TARGET=$1
case "$BUILD_TARGET" in
    TARGET_XM32|TARGET_WING|TARGET_PC_SDL2) ;;
    *) echo "Unknown build target: $BUILD_TARGET" >&2; exit 1 ;;
esac

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT_DIR}/build"

LIB_EXT_DIR="${ROOT_DIR}/lib_ext"

LVGL_DIR="${LIB_EXT_DIR}/lvgl"
GLAZE_DIR="${LIB_EXT_DIR}/glaze"
LIBARTNET_DIR="${LIB_EXT_DIR}/libartnet"
DOCTEST_DIR="${LIB_EXT_DIR}/doctest"
OSC_DIR="${LIB_EXT_DIR}/small-osc"

mkdir -p "${BUILD_DIR}"

# create folder if not present
if [ ! -d "$LIB_EXT_DIR" ]; then
    mkdir -p "$LIB_EXT_DIR"
fi

# acquire external libraries
if [ ! -d "$LVGL_DIR" ]; then
    git clone --depth 1 --single-branch --branch v9.5.0 https://github.com/lvgl/lvgl.git "$LVGL_DIR"
fi
if [ ! -d "$GLAZE_DIR" ]; then
    git clone --depth 1 --single-branch --branch v7.7.1 https://github.com/stephenberry/glaze.git "$GLAZE_DIR"
fi
if [ ! -d "$LIBARTNET_DIR" ]; then
    git clone --depth 1 --single-branch https://github.com/OpenLightingProject/libartnet.git "$LIBARTNET_DIR"
fi
if [ ! -d "$DOCTEST_DIR" ]; then
    git clone --depth 1 --single-branch --branch v2.5.2 https://github.com/doctest/doctest.git "$DOCTEST_DIR"
fi
if [ ! -d "$OSC_DIR" ]; then
    git clone --depth 1 --single-branch https://github.com/OpenMixerProject/SmallOSC.git "$OSC_DIR"
fi

if command -v nproc >/dev/null 2>&1; then
    jobs=$(nproc)
else
    jobs=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
fi
make -C "$ROOT_DIR" ROOT_DIR="$ROOT_DIR" BUILD_TARGET="$BUILD_TARGET" -j"$jobs"
