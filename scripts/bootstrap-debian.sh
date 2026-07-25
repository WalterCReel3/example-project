#!/usr/bin/env bash
#
# bootstrap-debian.sh — install the host development environment.
#
# Every package name here was verified against Debian 12 (bookworm) apt
# metadata. Ubuntu 22.04+ carries the same names. See docs/DEVELOPMENT.md for
# what each group is for and why.
#
# Usage:
#   ./scripts/bootstrap-debian.sh                 # core + sdl + tools (default)
#   ./scripts/bootstrap-debian.sh --all           # everything
#   ./scripts/bootstrap-debian.sh --cross --midi  # add specific groups
#   ./scripts/bootstrap-debian.sh --dry-run       # print, install nothing
#   ./scripts/bootstrap-debian.sh --list          # show groups and exit

set -euo pipefail

# ---------------------------------------------------------------------------
# Package groups
# ---------------------------------------------------------------------------

# Compiler, build driver, and the bits CMake itself leans on.
PKGS_CORE=(
    build-essential
    cmake
    ninja-build
    git
    pkg-config
    ccache
    file
)

# SDL2 is built from source via FetchContent, so we need SDL's *build*
# dependencies rather than libsdl2-dev. Missing one of these does not fail the
# build — SDL silently drops the corresponding backend, which then shows up much
# later as "no available video device". Install the lot.
PKGS_SDL=(
    # Audio backends
    libasound2-dev
    libpulse-dev
    libjack-dev
    libsndio-dev
    # X11 video backend
    libx11-dev
    libxext-dev
    libxrandr-dev
    libxcursor-dev
    libxfixes-dev
    libxi-dev
    libxss-dev
    libxkbcommon-dev
    # Wayland video backend
    libwayland-dev
    wayland-protocols
    libdecor-0-dev
    # KMSDRM video backend — this is the one the handhelds actually use, so
    # keeping it working on the dev box is how you catch device breakage early.
    libdrm-dev
    libgbm-dev
    # Desktop GL / GLES / EGL headers.
    # NOTE: libgles2-mesa-dev is a transitional dummy on bookworm; the real
    # package is libgles-dev.
    libgl1-mesa-dev
    libglu1-mesa-dev
    libegl1-mesa-dev
    libgles-dev
    libegl-dev
    # GLEW and GLU are needed by the gl_legacy backend specifically, not by SDL:
    # gfx/context.cc calls glewInit() and gluPerspective(). Without these,
    # -DWREEL_GFX_BACKEND=gl_legacy fails at find_package().
    libglew-dev
    # Input plumbing and hotplug
    libudev-dev
    libdbus-1-dev
    libibus-1.0-dev
)

# Codecs for SDL2_image / SDL2_ttf. These are optional: both projects can
# vendor their own copies (SDLIMAGE_VENDORED / SDLTTF_VENDORED), which is what
# the cross builds do. Installing them makes host builds faster and smaller.
PKGS_CODECS=(
    libpng-dev
    libjpeg-dev
    libfreetype-dev
    libharfbuzz-dev
)

# Static analysis, formatting, debugging.
PKGS_TOOLS=(
    clang-format
    clang-tidy
    gdb
    cppcheck
    shellcheck
)

# Cross compilation to the handheld architectures.
#
# IMPORTANT: Debian's cross-gcc is for compile-checking only. It links against
# bookworm's glibc 2.36, which is far newer than any handheld's. Shippable
# device binaries come from the device SDK containers — see docs/TARGETS.md.
# qemu-user-static + binfmt lets ctest run cross-built test binaries in place.
PKGS_CROSS=(
    crossbuild-essential-armhf
    crossbuild-essential-arm64
    qemu-user-static
    binfmt-support
    patchelf
)

# Device SDKs and the Steam Runtime ship as container images.
PKGS_CONTAINER=(
    docker.io
)

# Secondary goal: MIDI-driven live visuals.
PKGS_MIDI=(
    librtmidi-dev
    alsa-utils
)

# Header-only math, handy once the software/GLES backends need real matrices.
PKGS_MATH=(
    libglm-dev
)

# ---------------------------------------------------------------------------
# Argument handling
# ---------------------------------------------------------------------------

DRY_RUN=0
declare -a SELECTED=()

usage() {
    sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'
    exit "${1:-0}"
}

list_groups() {
    cat <<'EOF'
Groups:
  core       compiler, cmake, ninja, git, ccache        (default)
  sdl        SDL2 source-build dependencies             (default)
  codecs     libpng/jpeg/freetype/harfbuzz              (default)
  tools      clang-format, clang-tidy, gdb, cppcheck    (default)
  cross      armhf/arm64 cross gcc, qemu-user-static
  container  docker.io, for device SDKs + Steam Runtime
  midi       librtmidi, alsa-utils
  math       libglm-dev
EOF
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --all)       SELECTED=(core sdl codecs tools cross container midi math) ;;
        --core)      SELECTED+=(core) ;;
        --sdl)       SELECTED+=(sdl) ;;
        --codecs)    SELECTED+=(codecs) ;;
        --tools)     SELECTED+=(tools) ;;
        --cross)     SELECTED+=(cross) ;;
        --container) SELECTED+=(container) ;;
        --midi)      SELECTED+=(midi) ;;
        --math)      SELECTED+=(math) ;;
        --dry-run|-n) DRY_RUN=1 ;;
        --list|-l)   list_groups ;;
        --help|-h)   usage 0 ;;
        *) echo "error: unknown option '$1'" >&2; usage 1 ;;
    esac
    shift
done

# No group flags given: install the sensible default set.
if [[ ${#SELECTED[@]} -eq 0 ]]; then
    SELECTED=(core sdl codecs tools)
fi

# ---------------------------------------------------------------------------
# Host sanity checks
# ---------------------------------------------------------------------------

if ! command -v apt-get >/dev/null 2>&1; then
    echo "error: this script targets Debian-based systems (no apt-get found)." >&2
    echo "       See docs/DEVELOPMENT.md for the dependency list to translate." >&2
    exit 1
fi

DISTRO_ID="unknown"; DISTRO_VER="unknown"
if [[ -r /etc/os-release ]]; then
    # shellcheck disable=SC1091
    . /etc/os-release
    DISTRO_ID="${ID:-unknown}"
    DISTRO_VER="${VERSION_ID:-unknown}"
fi
echo "==> Host: ${DISTRO_ID} ${DISTRO_VER} ($(uname -m))"

SUDO=""
if [[ ${EUID} -ne 0 ]]; then
    if command -v sudo >/dev/null 2>&1; then
        SUDO="sudo"
    else
        echo "error: need root or sudo to install packages." >&2
        exit 1
    fi
fi

# ---------------------------------------------------------------------------
# Assemble and install
# ---------------------------------------------------------------------------

declare -a WANTED=()
for g in "${SELECTED[@]}"; do
    case "$g" in
        core)      WANTED+=("${PKGS_CORE[@]}") ;;
        sdl)       WANTED+=("${PKGS_SDL[@]}") ;;
        codecs)    WANTED+=("${PKGS_CODECS[@]}") ;;
        tools)     WANTED+=("${PKGS_TOOLS[@]}") ;;
        cross)     WANTED+=("${PKGS_CROSS[@]}") ;;
        container) WANTED+=("${PKGS_CONTAINER[@]}") ;;
        midi)      WANTED+=("${PKGS_MIDI[@]}") ;;
        math)      WANTED+=("${PKGS_MATH[@]}") ;;
    esac
done

# De-duplicate while preserving order.
declare -a PKGS=()
for p in "${WANTED[@]}"; do
    [[ " ${PKGS[*]-} " == *" $p "* ]] || PKGS+=("$p")
done

echo "==> Groups: ${SELECTED[*]}"
echo "==> ${#PKGS[@]} packages:"
printf '      %s\n' "${PKGS[@]}"

if [[ ${DRY_RUN} -eq 1 ]]; then
    echo "==> --dry-run given, stopping before install."
    exit 0
fi

echo "==> apt-get update"
${SUDO} apt-get update -qq

echo "==> apt-get install"
${SUDO} apt-get install -y --no-install-recommends "${PKGS[@]}"

# ---------------------------------------------------------------------------
# Post-install verification
# ---------------------------------------------------------------------------

echo
echo "==> Verifying toolchain"

# CMakePresets.json requires schema v3, which landed in CMake 3.21.
CMAKE_MIN="3.21"
if command -v cmake >/dev/null 2>&1; then
    CMAKE_VER="$(cmake --version | head -1 | awk '{print $3}')"
    if printf '%s\n%s\n' "$CMAKE_MIN" "$CMAKE_VER" | sort -V -C; then
        echo "    cmake     ${CMAKE_VER}  (>= ${CMAKE_MIN} ok)"
    else
        cat >&2 <<EOF
    cmake     ${CMAKE_VER}  TOO OLD — need >= ${CMAKE_MIN} for CMakePresets v3.

    Debian 12 / Ubuntu 22.04+ are fine. On Debian 11 or Ubuntu 20.04, get a
    newer CMake from Kitware's APT repo or a virtualenv:

        python3 -m venv ~/.venv/cmake && ~/.venv/cmake/bin/pip install cmake ninja
        export PATH="\$HOME/.venv/cmake/bin:\$PATH"
EOF
    fi
else
    echo "    cmake     NOT FOUND" >&2
fi

for tool in ninja g++ pkg-config ccache; do
    if command -v "$tool" >/dev/null 2>&1; then
        printf '    %-9s %s\n' "$tool" "$($tool --version 2>&1 | head -1 | cut -c1-48)"
    else
        printf '    %-9s not installed\n' "$tool"
    fi
done

for tool in arm-linux-gnueabihf-g++ aarch64-linux-gnu-g++; do
    if command -v "$tool" >/dev/null 2>&1; then
        printf '    %-9s %s\n' "cross" "$tool $("$tool" -dumpversion)"
    fi
done

# binfmt registration is what makes `ctest` work on cross-built binaries.
if [[ " ${SELECTED[*]} " == *" cross "* ]]; then
    if [[ -e /proc/sys/fs/binfmt_misc/qemu-arm ]]; then
        echo "    binfmt    qemu-arm registered (cross tests runnable)"
    else
        echo "    binfmt    qemu-arm NOT registered; try:"
        echo "                ${SUDO} systemctl restart systemd-binfmt"
    fi
fi

echo
echo "==> Done. Next: docs/DEVELOPMENT.md → 'Building'"
