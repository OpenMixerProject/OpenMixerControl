#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN="${ROOT_DIR}/build/pc_sdl2/omc"

usage() {
    cat <<'EOF'
Usage: ./dev.sh [--clean] [--skip-submodules] [--] [omc args...]

Builds the SDL2 desktop development simulator, then starts it in bodyless
development mode.

Options:
  --clean            Remove the simulator build output before compiling.
  --skip-submodules  Use existing lib_ext dependencies without downloading them.
  -h, --help         Show this help text.

Any remaining arguments are forwarded to omc after --bodyless.
EOF
}

clean=0
update_submodules=1
app_args=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --clean)
            clean=1
            shift
            ;;
        --skip-submodules)
            update_submodules=0
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        --)
            shift
            app_args+=("$@")
            break
            ;;
        *)
            app_args+=("$1")
            shift
            ;;
    esac
done

jobs_count() {
    if command -v nproc >/dev/null 2>&1; then
        nproc 2>/dev/null || echo 4
    elif command -v sysctl >/dev/null 2>&1; then
        sysctl -n hw.ncpu 2>/dev/null || echo 4
    else
        echo 4
    fi
}

sdl_cflags=""
sdl_libs=""

if command -v sdl2-config >/dev/null 2>&1; then
    sdl_cflags="$(sdl2-config --cflags)"
    sdl_libs="$(sdl2-config --libs)"
elif command -v pkg-config >/dev/null 2>&1 && pkg-config --exists sdl2; then
    sdl_cflags="$(pkg-config --cflags sdl2)"
    sdl_libs="$(pkg-config --libs sdl2)"
else
    cat >&2 <<'EOF'
SDL2 development files were not found.

Install SDL2, then run this script again:
  macOS:  brew install sdl2
  Debian/Ubuntu: sudo apt install libsdl2-dev
  Fedora: sudo dnf install SDL2-devel
EOF
    exit 1
fi

if [[ "${clean}" -eq 1 ]]; then
    make -C "${ROOT_DIR}" BUILD_TARGET=TARGET_PC_SDL2 clean
fi

export CC="${CC:-cc}" CXX="${CXX:-c++}"
export SDL_CFLAGS="${sdl_cflags}" SDL_LIBS="${sdl_libs}"
if [[ "${update_submodules}" -eq 1 ]]; then
    "${ROOT_DIR}/compile.sh" TARGET_PC_SDL2
else
    make -C "${ROOT_DIR}" BUILD_TARGET=TARGET_PC_SDL2 -j"$(jobs_count)"
fi

exec "${BIN}" --bodyless "${app_args[@]}"
