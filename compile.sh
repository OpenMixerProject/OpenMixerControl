#!/usr/bin/env bash
set -euo pipefail

export PATH=/opt/cross/bin:$PATH

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

BUILD_DIR="${ROOT_DIR}/build"
LIB_EXT_DIR="${ROOT_DIR}/lib_ext"

LVGL_DIR="${LIB_EXT_DIR}/lvgl"
GLAZE_DIR="${LIB_EXT_DIR}/glaze"
LIBARTNET_DIR="${LIB_EXT_DIR}/libartnet"

# Ordner anlegen
if [ ! -d "$BUILD_DIR" ]; then
    mkdir -p "$BUILD_DIR"
fi

if [ ! -d "$LIB_EXT_DIR" ]; then
    mkdir -p "$LIB_EXT_DIR"
fi

# externe Bibliotheken holen
git clone --depth 1 --single-branch --branch v9.5.0 https://github.com/lvgl/lvgl.git ${LVGL_DIR} || git -C ${LVGL_DIR} reset --hard HEAD
git clone --depth 1 --single-branch --branch v7.7.1 https://github.com/stephenberry/glaze.git ${GLAZE_DIR} || git -C ${GLAZE_DIR} reset --hard HEAD
git clone --depth 1 --single-branch https://github.com/OpenLightingProject/libartnet.git ${LIBARTNET_DIR} || git -C ${LIBARTNET_DIR} reset --hard HEAD
git clone --depth 1 --single-branch --branch v2.5.2 https://github.com/doctest/doctest.git ${DOCTEST_DIR} || git -C ${DOCTEST_DIR} reset --hard HEAD

make BUILD_DIR=${BUILD_DIR} -j$(nproc)