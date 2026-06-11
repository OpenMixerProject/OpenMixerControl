#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT_DIR}/build/xm32"
OUT_DIR="${ROOT_DIR}/build/output/xm32"
DOCKER_IMAGE="omc-xm32-builder2:trixie"
DOCKER_CONFIG_DIR="${BUILD_DIR}/docker-config"

if [[ -n "${DOCKER_HOST:-}" ]]; then
    DOCKER_HOST_URI="${DOCKER_HOST}"
elif [[ -S "${HOME}/.docker/run/docker.sock" ]]; then
    DOCKER_HOST_URI="unix://${HOME}/.docker/run/docker.sock"
else
    DOCKER_HOST_URI="unix:///var/run/docker.sock"
fi

mkdir -p "${BUILD_DIR}" "${OUT_DIR}" "${DOCKER_CONFIG_DIR}"

DOCKER_CONFIG="${DOCKER_CONFIG_DIR}" DOCKER_HOST="${DOCKER_HOST_URI}" DOCKER_BUILDKIT=0 \
docker build -t "${DOCKER_IMAGE}" - <<'DOCKERFILE'
FROM debian:trixie
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
 && apt-get install -y --no-install-recommends \
            git \
            libncurses-dev \
            gawk \
            flex \
            bison \
            openssl \
            libssl-dev \
            dkms \
            libelf-dev \
            libudev-dev \
            libpci-dev \
            libiberty-dev \
            autoconf \
            llvm \
            fakeroot \
            build-essential \
            devscripts \
            gcc-arm-none-eabi \
            binutils-arm-none-eabi \
            gcc-arm-linux-gnueabi \
            g++-arm-linux-gnueabi \
            binutils-arm-linux-gnueabi \
            u-boot-tools \
            bc \
            cpio \
            dropbear-bin \
  && rm -rf /var/lib/apt/lists/*
DOCKERFILE

DOCKER_CONFIG="${DOCKER_CONFIG_DIR}" DOCKER_HOST="${DOCKER_HOST_URI}" \
docker run --rm \
  -i \
  -u "$(id -u):$(id -g)" \
  -v "${ROOT_DIR}:${ROOT_DIR}" \
  -w "${ROOT_DIR}" \
  -e ROOT_DIR="${ROOT_DIR}" \
  -e BUILD_DIR="${BUILD_DIR}" \
  "${DOCKER_IMAGE}" \
  bash -s <<'BUILD'

set -euo pipefail

export PATH=/opt/cross/bin:$PATH


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
git clone --depth 1 --single-branch --branch v9.5.0 https://github.com/lvgl/lvgl.git ${LVGL_DIR} || git reset --hard HEAD
git clone --depth 1 --single-branch --branch v7.7.1 https://github.com/stephenberry/glaze.git ${GLAZE_DIR} || git reset --hard HEAD
git clone --depth 1 --single-branch https://github.com/OpenLightingProject/libartnet.git ${LIBARTNET_DIR} || git reset --hard HEAD

make BUILD_DIR=${BUILD_DIR} -j$(nproc)

BUILD
