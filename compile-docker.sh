#!/usr/bin/env bash
set -euo pipefail

if [ $# -eq 0 ]; then
    echo "No arguments supplied!"
    echo "Possible Arguments: $0 [TARGET_XM32, TARGET_WING, TARGET_PC_SDL2]"
    exit
fi
BUILD_TARGET=$1

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
DOCKER_IMAGE="omc-builder:trixie"
DOCKER_CONFIG_DIR="${BUILD_DIR}/docker-config"

if [[ -n "${DOCKER_HOST:-}" ]]; then
    DOCKER_HOST_URI="${DOCKER_HOST}"
elif [[ -S "${HOME}/.docker/run/docker.sock" ]]; then
    DOCKER_HOST_URI="unix://${HOME}/.docker/run/docker.sock"
else
    DOCKER_HOST_URI="unix:///var/run/docker.sock"
fi

mkdir -p "${BUILD_DIR}" "${DOCKER_CONFIG_DIR}"

DOCKER_CONFIG="${DOCKER_CONFIG_DIR}" DOCKER_HOST="${DOCKER_HOST_URI}" DOCKER_BUILDKIT=0 \
docker build -t "${DOCKER_IMAGE}" - <<'DOCKERFILE'
FROM debian:trixie
ENV DEBIAN_FRONTEND=noninteractive
RUN dpkg --add-architecture armhf \
 && dpkg --add-architecture armel \
 && apt-get update \
 && apt-get install -y --no-install-recommends \
    bc \
    bison \
    bzip2 \
    ca-certificates \
    cpio \
    curl \
    device-tree-compiler \
    flex \
    gcc \
    gcc-arm-linux-gnueabihf \
    g++-arm-linux-gnueabihf \
    libc6-dev \
    libc6-dev-armhf-cross \
    libcrypt-dev:armhf \
    libncurses-dev:armhf \
    libssl-dev \
    make \
    patch \
    pkg-config \
    python3 \
    u-boot-tools \
    xz-utils \
    git \
    build-essential \
    libsdl2-dev \
    wget \
    xz-utils \
    zlib1g-dev:armel libz-dev:armel libudev-dev:armel \
  && rm -rf /var/lib/apt/lists/*


# MUSL Toolchain
RUN wget https://raw.githubusercontent.com/OpenMixerProject/OpenX32/refs/heads/main/toolchains/cross-arm-arm926ej.tar.xz \
        && tar -xf cross-arm-arm926ej.tar.xz -C /

# download, compile and install libz into ${BUILD_DIR}/opt/cross
RUN cd ${BUILD_DIR} \
        && wget https://zlib.net/zlib-1.3.2.tar.gz \
        && tar -xf zlib-1.3.2.tar.gz \
        && rm zlib-1.3.2.tar.gz \
        && cd zlib-1.3.2 \
        && export PATH=/opt/cross/bin:$PATH \
        && export CHOST=arm-openwrt-linux-muslgnueabi \
        && export CC=${CHOST}-gcc \
        && export AR=${CHOST}-ar \
        && export RANLIB=${CHOST}-ranlib \
        && ./configure --prefix=/opt/cross/arm-openwrt-linux-muslgnueabi --shared \
        && make \
        && make install

DOCKERFILE

DOCKER_CONFIG="${DOCKER_CONFIG_DIR}" DOCKER_HOST="${DOCKER_HOST_URI}" \
docker run --rm \
  -i \
  -u "$(id -u):$(id -g)" \
  -v "${ROOT_DIR}:${ROOT_DIR}" \
  -w "${ROOT_DIR}" \
  -e ROOT_DIR="${ROOT_DIR}" \
  -e BUILD_DIR="${BUILD_DIR}" \
  -e BUILD_TARGET="${BUILD_TARGET}" \
  "${DOCKER_IMAGE}" \
  bash -s <<'BUILD'
set -euo pipefail

export PATH=/opt/cross/bin:$PATH

LIB_EXT_DIR="${ROOT_DIR}/lib_ext"

LVGL_DIR="${LIB_EXT_DIR}/lvgl"
GLAZE_DIR="${LIB_EXT_DIR}/glaze"
LIBARTNET_DIR="${LIB_EXT_DIR}/libartnet"
DOCTEST_DIR="${LIB_EXT_DIR}/doctest"

# Ordner anlegen
if [ ! -d "$LIB_EXT_DIR" ]; then
    mkdir -p "$LIB_EXT_DIR"
fi

# externe Bibliotheken holen
git clone --depth 1 --single-branch --branch v9.5.0 https://github.com/lvgl/lvgl.git ${LVGL_DIR} || git -C ${LVGL_DIR} reset --hard HEAD
git clone --depth 1 --single-branch --branch v7.7.1 https://github.com/stephenberry/glaze.git ${GLAZE_DIR} || git -C ${GLAZE_DIR} reset --hard HEAD
git clone --depth 1 --single-branch https://github.com/OpenLightingProject/libartnet.git ${LIBARTNET_DIR} || git -C ${LIBARTNET_DIR} reset --hard HEAD
git clone --depth 1 --single-branch --branch v2.5.2 https://github.com/doctest/doctest.git ${DOCTEST_DIR} || git -C ${DOCTEST_DIR} reset --hard HEAD

make ROOT_DIR=${ROOT_DIR} BUILD_TARGET=${BUILD_TARGET} -j$(nproc)

BUILD
