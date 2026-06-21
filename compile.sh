#!/usr/bin/env bash

if [ $# -eq 0 ]; then
    echo "No arguments supplied!"
    echo "Possible Arguments: $0 [TARGET_XM32, TARGET_WING, TARGET_PC_SDL2]"
    exit
fi
BUILD_TARGET=$1

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
git clone --depth 1 --single-branch --branch v9.5.0 https://github.com/lvgl/lvgl.git ${LVGL_DIR} || git -C ${LVGL_DIR} reset --hard HEAD
git clone --depth 1 --single-branch --branch v7.7.1 https://github.com/stephenberry/glaze.git ${GLAZE_DIR} || git -C ${GLAZE_DIR} reset --hard HEAD
git clone --depth 1 --single-branch https://github.com/OpenLightingProject/libartnet.git ${LIBARTNET_DIR} || git -C ${LIBARTNET_DIR} reset --hard HEAD
git clone --depth 1 --single-branch --branch v2.5.2 https://github.com/doctest/doctest.git ${DOCTEST_DIR} || git -C ${DOCTEST_DIR} reset --hard HEAD
git clone --depth 1 --single-branch https://github.com/OpenMixerProject/SmallOSC.git ${OSC_DIR} || git -C ${OSC_DIR} reset --hard HEAD

make ROOT_DIR=${ROOT_DIR} BUILD_TARGET=${BUILD_TARGET} -j$(nproc)
