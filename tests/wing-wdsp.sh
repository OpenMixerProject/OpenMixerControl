#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
set -euo pipefail
cd "$(dirname "$0")/.."
scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT
umask 077
image=$(printf '%064d' 0)
cat > "$scratch/manifest" <<EOF
{"schema_version":2,"abi_version":131072,"magic":1464421959,"build_id":305419896,"feature_flags":15,
"target":"wing-compact-t55f484-fork","device":"T55F484","bitstream":{"file":"wing.bin","sha256":"$image"},
"transport":{"device":"/dev/spibone0","compatible":"openmixer,wing-spibone","chip_select":1,"mode":0,"max_hz":2000000},
"dsp_runtime":{"device":"/dev/spidev1.0","chip_select":0,"abi":1}}
EOF
digest=$(sha256sum "$scratch/manifest" | cut -d' ' -f1)
touch "$scratch/lock"
${CXX:-c++} -std=c++23 -DTARGET_WING -Isrc -Ilib_ext/glaze/include \
    tests/wing-wdsp.cpp src/wing-wdsp.cpp -lz -pthread \
    -Wl,--wrap=open,--wrap=ioctl,--wrap=lstat,--wrap=fstat,--wrap=statfs,--wrap=getrandom -o "$scratch/test"
"$scratch/test" "$scratch" "$digest" "$image"
