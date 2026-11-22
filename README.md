[![Build for X32/M32](https://github.com/OpenMixerProject/OpenMixerControl/actions/workflows/compile-xm32-docker.yml/badge.svg)](https://github.com/OpenMixerProject/OpenMixerControl/actions/workflows/compile-xm32-docker.yml)
[![Build for WING](https://github.com/OpenMixerProject/OpenMixerControl/actions/workflows/compile-wing-docker.yml/badge.svg)](https://github.com/OpenMixerProject/OpenMixerControl/actions/workflows/compile-wing-docker.yml)
[![Build for PC (SDL2)](https://github.com/OpenMixerProject/OpenMixerControl/actions/workflows/compile-pc_sdl2-docker.yml/badge.svg)](https://github.com/OpenMixerProject/OpenMixerControl/actions/workflows/compile-pc_sdl2-docker.yml)

# General

OpenMixerControl is the main software to run on a digital Audio-Mixing-Console. It is used in:

- [OpenX32](https://github.com/OpenMixerProject/OpenX32)
- [OpenWING](https://github.com/OpenMixerProject/OpenWING)

## OpenX32
Linux and Custom Firmware for the Behringer X32 and Midas M32 Audio-Mixing-Consoles

<img width="4000" height="3000" alt="IMG_20260520_211131" src="https://github.com/user-attachments/assets/3de539e1-a078-476b-a97a-3dda304b144d" />

<img width="1920" height="2560" alt="IMG_1998" src="https://github.com/user-attachments/assets/2b54524f-3dc2-481a-8f43-dbc0ac0b3ec7" />

<img width="1920" height="2560" alt="IMG_2001" src="https://github.com/user-attachments/assets/526c40a5-20a3-4d98-9bcb-a256bbed57de" />

## OpenWING
Linux and Custom Firmware for the Behringer WING Audio-Mixing-Consoles

<img width="3000" height="4000" alt="IMG_20260604_020730" src="https://github.com/user-attachments/assets/189e2af7-1664-4385-b29f-81b094274c69" />

### T55 bench DSP identification

On a muted WING Compact running the matching OpenWING T55 loader and WDSP v1
firmware, use `omc --wing-dsp-identify 4` (DSP 1, 2 or 4). This one-shot
command exits before mixer startup. It requires the loader's boot-local,
per-core qualification record, matching live FPGA identity and reset readback,
and FS and return-valid both off. A successful Identify means SPORT is armed,
not that audio is valid. DSP3, X32 probing, boot/reset and audio enable are
outside this command's scope.

With the matching DSP4 Overdrive firmware, `omc --wing-overdrive-set 2 0.25
-0.1 0.03 0.8 0.6` sends one atomic update to stereo pair 0 after a fresh
Identify. Arguments are WDSP units, not dB/Hz: linear pregain [1, 16], Q
[-1, 0.999999], bias [-1, 1], then the input HPF, input LPF and output LPF
coefficients [0, 1]. `omc --wing-overdrive-bypass` bypasses the same pair.
Both require an acknowledged command. A missing ACK leaves the outcome unknown;
the command is not retried. Neither command enables audio or changes routing.

The WING transport uses [Mala's SPIBone UAPI](https://github.com/malarisch/OpenAudioFPGA/commit/7d2869acdcff4a0a1952b74bf9b5fe45e37ff7dd)
and follows [Mala's OMC Wishbone read design](https://github.com/malarisch/OpenMixerControl/commit/a136388a92dc73c9d0d983eed1386260553d9d91).
Run `bash tests/wing-wdsp.sh` with the fetched Glaze headers and Linux zlib
development package for device-mocked verification.


# Some Screenshots

<img width="798" height="478" alt="grafik" src="https://github.com/user-attachments/assets/e2e2a1b8-3d5a-4539-a872-d4400db0bee9" />

<img width="801" height="510" alt="grafik" src="https://github.com/user-attachments/assets/45106b8c-7368-4e58-b05c-e0742c127dad" />

# Social, Web, Community

- [OpenX32 Website](https://openx32.com/)
- [OpenMixerProject on Github](https://github.com/OpenMixerProject)
- [Discourse Community](https://discourse.openmixerproject.de/)
- [Sponsor us](https://buymeacoffee.com/chrisnoeding)

# Development Stuff

## Branches

- develop - Development of all the new stuff, unstable, sometimes it does not compile. Default branch of the repository.
- stable - Stable version of OMC and used for regular releases.
- dev-XXX - Development of a feature or fix, only created if needed.

## Folder Structure

- **build:** build artifacts
- **files:** patches, configs, …
- **lib:** libraries
- **lib_ext:** libraries we load from external sources in the build process, folder is created on buildtime
- **src:** our sources

## WING Compact T55 control

Build with `./compile.sh TARGET_WING` on Debian 13 with the `arm-linux-gnueabihf`
toolchain. The static executable is `build/wing/omc`. Package it with
`transport=spibone`, the SHA-256 of `src/spibone_uapi.h` and the matching
`dsp_board_profile_sha256` in OpenWING's OMC provenance.

`wing-t55 check` verifies fabric readiness; `wing-t55 run` starts the local UI
server. OMC requires the loader's boot-local receipt, shared loader lock,
manifest digest and matching live SPIBone ONLINE, identity and routing Ready.
It does not program the FPGA, boot a DSP, start legacy services or enable audio.
The separate WDSP bench commands above are unchanged.

An operator can set one route with `omc --wing-t55-loaded /run/omc-wing-fpga.json
--wing-t55-route DEST SOURCE`. Use the zero-based manifest destination and the
native source selector (`0` is OFF). OMC checks exported endpoint IDs and reads
the selector back after writing; it never retries an uncertain write. Routes
are not part of scenes. Raw SPI and `--wing-t55-config` require explicit
`--wing-t55-recovery` and a matching schema-1 Recovery receipt; there is no fallback.

The OpenWING-ui profile uses UDP loopback ports 10032 (OMC) and 10033 (UI).
It serves a 32-input software snapshot with name, fader and mute values. Only
`/set/ch_name/<1–32>` is writable: up to 32 UTF-8 bytes, nonblank and without
control characters. `/result/ch_name/<channel>` reports the stored name;
invalid names return `/result/ch_name_error/<channel>`. Names are not persisted.
`/get/t55_matrix/1` reports DSP and AES50 B endpoints and passive selector reads.
Unreadable or unmapped selectors are unknown, not OFF. `--wing-t55-ui-offline`
can serve the installed catalog but never opens a device.

`local_io` catalogs stock Compact ADC/DAC pads and slots (24 LOCAL IN, eight
LOCAL OUT, AUX OUT 7/8). Stock AE IDs are references, not T55 selectors or
verified jack numbers. The offline encoder updates a 27-byte head-amp image;
it has no runtime caller or verified dB conversion. Current gateware has no
ADDA transport.

The UAPI is from [Mala's SPIBone driver](https://github.com/malarisch/OpenAudioFPGA/commit/7d2869acdcff4a0a1952b74bf9b5fe45e37ff7dd)
plus the OpenWING loader's `WING_ENABLE` ioctl definition. Register access follows
[Mala's OMC Wishbone design](https://github.com/malarisch/OpenMixerControl/commit/a136388a92dc73c9d0d983eed1386260553d9d91).

Run the device-mocked tests as root on isolated Linux with `/run` on tmpfs,
kernel AF_ALG SHA-256 and the fetched Glaze headers:

```sh
g++ -std=c++23 -DTARGET_WING -Ilib_ext/glaze/include \
    tests/wing-t55-control.cpp src/wing-t55-control.cpp \
    -Wl,--wrap=open,--wrap=lstat,--wrap=close,--wrap=ioctl -o build/wing-t55-control-test
build/wing-t55-control-test
c++ -std=c++23 -DTARGET_WING tests/wing-local-io.cpp src/wing-local-io.cpp -o build/wing-local-io-test
build/wing-local-io-test
```

With no installed T55 manifest, test the UI wire contract with
`python3 tests/wing-t55-ui.py qemu-arm build/wing/omc` (omit `qemu-arm` on ARM).

## Plans, Vision, Future

The big plan on one page :-)

<img width="666" height="500" alt="grafik" src="https://github.com/user-attachments/assets/4a6cf4b3-30f2-4f33-a738-9d827689afaf" />

                        
## Stargazers over time
[![Stargazers over time](https://starchart.cc/OpenMixerProject/OpenMixerControl.svg?variant=adaptive)](https://starchart.cc/OpenMixerProject/OpenMixerControl)
