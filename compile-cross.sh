#!/usr/bin/env bash
# Download the required cross toolchains and build OpenMixerControl targets.
# Toolchains are cached outside the repository and reused on later runs.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CACHE_DIR="${OMC_CROSS_CACHE:-$HOME/.cache/omc-cross}"
JOBS="${JOBS:-4}"

usage() {
    cat <<'EOF'
Usage: ./compile-cross.sh [TARGET_PC_SDL2] [TARGET_XM32] [TARGET_WING] [all]

With no arguments, all three targets are built from clean. Environment:
  OMC_CROSS_CACHE  Toolchain cache directory (default: $HOME/.cache/omc-cross)
  JOBS             Parallel make jobs (default: 4)
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    usage
    exit 0
fi

targets=()
if [[ $# -eq 0 || " $* " == *" all "* ]]; then
    targets=(TARGET_PC_SDL2 TARGET_XM32 TARGET_WING)
else
    targets=("$@")
    for target in "${targets[@]}"; do
        case "$target" in
            TARGET_PC_SDL2|TARGET_XM32|TARGET_WING) ;;
            *) echo "Unknown target: $target" >&2; usage >&2; exit 2 ;;
        esac
    done
fi

fetch() {
    local url="$1" dest="$2" sha="${3:-}"
    mkdir -p "$(dirname "$dest")"

    if [[ -f "$dest" ]]; then
        if [[ -z "$sha" ]] || echo "$sha  $dest" | sha256sum -c - >/dev/null 2>&1; then
            return 0
        fi
        echo "Re-downloading $dest (checksum mismatch)" >&2
    fi

    echo "Downloading $(basename "$dest")"
    curl -fsSL --retry 3 -o "$dest" "$url"
    if [[ -n "$sha" ]]; then
        echo "$sha  $dest" | sha256sum -c - >/dev/null
    fi
}

extract_deb() {
    local deb="$1" root="$2" tmp data
    tmp="$(mktemp -d)"
    (
        cd "$tmp"
        ar x "$deb"
    )
    data="$(find "$tmp" -maxdepth 1 -name 'data.tar.*' -print -quit)"
    if [[ -z "$data" ]]; then
        echo "No data archive in $deb" >&2
        rm -rf "$tmp"
        exit 1
    fi
    tar -xf "$data" -C "$root"
    rm -rf "$tmp"
}

setup_xm32() {
    local dir="$CACHE_DIR/xm32" root debs packages ver tool link
    root="$dir/root"
    debs="$dir/debs"
    packages="$dir/Packages.gz"
    mkdir -p "$debs" "$root"

    # GCC 14 is required because glaze includes <format>, which is missing
    # from the libstdc++ shipped with GCC 12 cross packages.
    fetch "https://deb.debian.org/debian/dists/trixie/main/binary-amd64/Packages.gz" "$packages"

    python3 - "$packages" > "$dir/packages.tsv" <<'PY'
import gzip
import sys

wanted = {
    "binutils-arm-linux-gnueabi",
    "binutils-common",
    "libbinutils",
    "libsframe1",
    "cpp-14-arm-linux-gnueabi",
    "g++-14-arm-linux-gnueabi",
    "gcc-14-arm-linux-gnueabi",
    "gcc-14-arm-linux-gnueabi-base",
    "gcc-14-cross-base",
    "libgcc-14-dev-armel-cross",
    "libstdc++-14-dev-armel-cross",
    "libgcc-s1-armel-cross",
    "libgomp1-armel-cross",
    "libatomic1-armel-cross",
    "libasan8-armel-cross",
    "libubsan1-armel-cross",
    "libstdc++6-armel-cross",
    "libc6-armel-cross",
    "libc6-dev-armel-cross",
    "linux-libc-dev-armel-cross",
}

seen = set()
with gzip.open(sys.argv[1], "rt") as stream:
    for block in stream.read().split("\n\n"):
        fields = {}
        for line in block.splitlines():
            if ": " in line:
                key, value = line.split(": ", 1)
                fields[key] = value
        name = fields.get("Package")
        if name not in wanted or name in seen:
            continue
        if fields.get("Architecture") not in ("amd64", "all"):
            continue
        seen.add(name)
        print(fields["Filename"], fields["SHA256"], sep="\t")

missing = wanted - seen
if missing:
    raise SystemExit("Missing packages: " + ", ".join(sorted(missing)))
PY

    while IFS=$'\t' read -r path sha; do
        deb="$debs/$(basename "$path")"
        fetch "https://deb.debian.org/debian/$path" "$deb" "$sha"
        extract_deb "$deb" "$root"
    done < "$dir/packages.tsv"

    # Debian's libc.so linker script uses absolute target paths. Make it
    # relative so the extracted sysroot works from the cache directory.
    cat > "$root/usr/arm-linux-gnueabi/lib/libc.so" <<'EOF'
/* GNU ld script */
OUTPUT_FORMAT(elf32-littlearm)
GROUP ( libc.so.6 libc_nonshared.a AS_NEEDED ( ld-linux.so.3 ) )
EOF

    ver="$(find "$root/usr/bin" -maxdepth 1 -type f \
        -name 'arm-linux-gnueabi-gcc-[0-9]*' -printf '%f\n' \
        | sed 's/.*gcc-//' | sort -V | tail -1)"
    if [[ -z "$ver" ]]; then
        echo "Could not determine the arm-linux-gnueabi GCC version" >&2
        exit 1
    fi

    for tool in gcc g++ cc c++ cpp gcc-ar gcc-nm gcc-ranlib gcov lto-dump; do
        link="$root/usr/bin/arm-linux-gnueabi-$tool"
        if [[ ! -e "$link" && ! -L "$link" ]]; then
            ln -s "arm-linux-gnueabi-$tool-$ver" "$link"
        fi
    done

    export PATH="$root/usr/bin:$PATH"
    export LD_LIBRARY_PATH="$root/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
}

setup_wing() {
    local dir root archive url sha tool target
    dir="$CACHE_DIR/wing"
    root="$dir/toolchain"
    archive="$dir/arm-gnu-toolchain.tar.xz"
    url="https://developer.arm.com/-/media/Files/downloads/gnu/15.2.rel1/binrel/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-linux-gnueabihf.tar.xz"
    sha="3c65d820a6b8f677f8f6fbfc749fe00a4f16dde12341436c9df5b7092a47c0fb"

    fetch "$url" "$archive" "$sha"
    if [[ ! -x "$root/bin/arm-none-linux-gnueabihf-g++" ]]; then
        rm -rf "$root"
        mkdir -p "$root"
        echo "Extracting Arm GNU Toolchain"
        tar -xf "$archive" -C "$root" --strip-components=1
    fi

    for tool in gcc g++ cc c++ ar ld as nm objcopy objdump ranlib readelf strip; do
        target="arm-none-linux-gnueabihf-$tool"
        ln -sf "$target" "$root/bin/arm-linux-gnueabihf-$tool"
    done

    export PATH="$root/bin:$PATH"
}

build_target() {
    local target="$1" output

    echo "==> Building $target"
    (
        case "$target" in
            TARGET_XM32) setup_xm32 ;;
            TARGET_WING) setup_wing ;;
        esac

        make -C "$ROOT_DIR" ROOT_DIR="$ROOT_DIR" BUILD_TARGET="$target" clean
        make -C "$ROOT_DIR" ROOT_DIR="$ROOT_DIR" BUILD_TARGET="$target" -j"$JOBS"
    )

    case "$target" in
        TARGET_PC_SDL2) output="build/pc_sdl2/omc" ;;
        TARGET_XM32) output="build/xm32/omc" ;;
        TARGET_WING) output="build/wing/omc" ;;
    esac
    file "$ROOT_DIR/$output"
}

for target in "${targets[@]}"; do
    build_target "$target"
done

echo "All requested targets built successfully."
