# Shared base for the aarch64 handhelds (RK3326, H700).
#
# Not used directly — rk3326.cmake and h700.cmake set WREEL_SOC_* and include
# this. Both run real Linux distributions (ArkOS, ROCKNIX, muOS, Batocera) with
# Mali GPUs, so they differ from the Miyoo Mini only in CPU tuning and sysroot.
#
# TWO MODES:
#
#   Debian cross-GCC (default) — COMPILE-CHECK ONLY.
#       Debian 12's aarch64-linux-gnu-g++ links against glibc 2.36. The
#       resulting binary will NOT load on any handheld:
#           ./game: /lib/libc.so.6: version `GLIBC_2.36' not found
#       Use it to catch -Werror and ABI breakage in seconds, never to ship.
#
#   Device sysroot — SHIPPABLE.
#       Point -DWREEL_SYSROOT=/path/to/device/rootfs at a copy pulled off a
#       device image. Add -DWREEL_USE_SYSTEM_SDL2=ON to link the firmware's SDL2.
#
# See docs/TARGETS.md § 2 for how to check a device's actual glibc version.

set(CMAKE_SYSTEM_NAME      Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(WREEL_TARGET_IS_HANDHELD ON)

# Mali GPUs: GLES 2.0 / 3.x is available. The gles2 backend is not written yet,
# so these presets currently build with the software backend; the flag records
# device capability, not backend readiness.
set(WREEL_TARGET_HAS_GPU OFF)

if(NOT WREEL_SOC_TUNE)
    message(FATAL_ERROR "WREEL_SOC_TUNE must be set before including this file")
endif()

# ---------------------------------------------------------------------------
# Compiler selection
# ---------------------------------------------------------------------------

set(_wreel_triple "aarch64-linux-gnu")

if(WREEL_TOOLCHAIN_PREFIX)
    set(_wreel_cc  "${WREEL_TOOLCHAIN_PREFIX}gcc")
    set(_wreel_cxx "${WREEL_TOOLCHAIN_PREFIX}g++")
else()
    set(_wreel_cc  "${_wreel_triple}-gcc")
    set(_wreel_cxx "${_wreel_triple}-g++")
endif()

find_program(_wreel_cc_path  "${_wreel_cc}")
find_program(_wreel_cxx_path "${_wreel_cxx}")

if(NOT _wreel_cc_path OR NOT _wreel_cxx_path)
    message(FATAL_ERROR
        "aarch64 cross compiler not found ('${_wreel_cc}').\n"
        "  Install it:  sudo apt install crossbuild-essential-arm64\n"
        "  or point at another toolchain with\n"
        "  -DWREEL_TOOLCHAIN_PREFIX=/path/to/aarch64-linux-gnu-")
endif()

set(CMAKE_C_COMPILER   "${_wreel_cc_path}")
set(CMAKE_CXX_COMPILER "${_wreel_cxx_path}")

# ---------------------------------------------------------------------------
# Sysroot
# ---------------------------------------------------------------------------

if(WREEL_SYSROOT)
    set(CMAKE_SYSROOT        "${WREEL_SYSROOT}")
    set(CMAKE_FIND_ROOT_PATH "${WREEL_SYSROOT}")
    set(WREEL_BUILD_IS_SHIPPABLE ON)
else()
    # Debian multiarch layout; no explicit sysroot needed.
    set(WREEL_BUILD_IS_SHIPPABLE OFF)
    message(WARNING
        "No WREEL_SYSROOT set — using the host cross-GCC.\n"
        "  This build is COMPILE-CHECK ONLY. Its glibc requirements are newer\n"
        "  than any handheld's, so it will not run on device.\n"
        "  See docs/TARGETS.md § 2.")
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)

# ---------------------------------------------------------------------------
# Codegen
# ---------------------------------------------------------------------------

set(CMAKE_C_FLAGS_INIT   "-mcpu=${WREEL_SOC_TUNE}")
set(CMAKE_CXX_FLAGS_INIT "-mcpu=${WREEL_SOC_TUNE}")

# Handhelds are RAM- and storage-constrained; -Os over -O3.
set(CMAKE_C_FLAGS_RELEASE_INIT   "-Os -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-Os -DNDEBUG")

set(WREEL_STATIC_CXX ON CACHE BOOL "" FORCE)

# ---------------------------------------------------------------------------
# Running cross-built tests
# ---------------------------------------------------------------------------

find_program(_wreel_qemu_aarch64 qemu-aarch64-static qemu-aarch64)
if(_wreel_qemu_aarch64)
    set(WREEL_TEST_EMULATOR "${_wreel_qemu_aarch64}")
endif()
