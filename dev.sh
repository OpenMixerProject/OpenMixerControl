#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="${ROOT_DIR}/src"
BIN="${ROOT_DIR}/bin/x32ctrl"

usage() {
    cat <<'EOF'
Usage: ./dev.sh [--clean] [--skip-submodules] [--] [x32ctrl args...]

Builds the SDL2 desktop development simulator, then starts it in bodyless
development mode.

Options:
  --clean            Remove the simulator build output before compiling.
  --skip-submodules  Do not run git submodule update --init --recursive.
  -h, --help         Show this help text.

Any remaining arguments are forwarded to x32ctrl after --bodyless.
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

case "$(uname -s)" in
    Linux)
        platform_ldflags="-lrt"
        ;;
    Darwin)
        platform_ldflags=""
        ;;
    *)
        platform_ldflags=""
        ;;
esac

if [[ "${update_submodules}" -eq 1 ]]; then
    git -C "${ROOT_DIR}" submodule update --init --recursive
fi

if [[ "${clean}" -eq 1 ]]; then
    make -C "${SRC_DIR}" -f Makefile_x64_SDL2 clean
fi

common_flags="-g -I../lib -I../lib/libartnet -I../lib/lv_port_linux -I../lib/glaze/include ${sdl_cflags} -MMD -MP -DBODYLESS_SDL2"

make -C "${SRC_DIR}" -f Makefile_x64_SDL2 -j"$(jobs_count)" \
    CC="${CC:-cc}" \
    CXX="${CXX:-c++}" \
    CFLAGS="${CFLAGS:-${common_flags}}" \
    CXXFLAGS="${CXXFLAGS:-"-std=c++23 ${common_flags}"}" \
    LDFLAGS="${LDFLAGS:-"${sdl_libs} -lm -lpthread ${platform_ldflags}"}"

exec "${BIN}" --bodyless "${app_args[@]}"
