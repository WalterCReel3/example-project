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

# Device capability, which is what this flag means and all it means. Both SoCs
# carry a Mali GPU exposing GLES 2.0 and above.
#
# cmake/Dependencies.cmake consumes this to decide whether SDL2 is built with
# GL/GLES/EGL at all, so it must not be used to say which of our renderers is
# ready — that is WREEL_ENABLE_GLES2. Setting it from readiness is what left both
# Mali devices compiled as though they had no GPU, with SDL's GLES2 render driver
# compiled out and no diagnostic (D18).
set(WREEL_TARGET_HAS_GPU ON)

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

# The compile-check-only warning is raised once from ProjectOptions.cmake rather
# than here: toolchain files are re-included for every try_compile, so warning
# here prints it four or more times per configure.
if(WREEL_SYSROOT)
    set(CMAKE_SYSROOT        "${WREEL_SYSROOT}")
    set(CMAKE_FIND_ROOT_PATH "${WREEL_SYSROOT}")
    set(WREEL_BUILD_IS_SHIPPABLE ON)
else()
    # Debian multiarch layout; no explicit sysroot needed to compile.
    set(WREEL_BUILD_IS_SHIPPABLE OFF)
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

# qemu needs to be told where the *target's* dynamic loader lives. Without it,
# running a cross-built binary on an x86_64 host fails with:
#
#   qemu-aarch64-static: Could not open '/lib/ld-linux-aarch64.so.1'
#
# because that path only exists on the target. Debian's cross toolchain ships a
# sysroot at /usr/aarch64-linux-gnu containing the loader; a device sysroot has
# its own. Passed as -L rather than QEMU_LD_PREFIX so it travels with the
# emulator command instead of depending on the environment ctest happens to run in.
find_program(_wreel_qemu_aarch64 qemu-aarch64-static qemu-aarch64)
if(_wreel_qemu_aarch64)
    set(WREEL_TEST_EMULATOR "${_wreel_qemu_aarch64}")
    if(CMAKE_SYSROOT)
        list(APPEND WREEL_TEST_EMULATOR -L "${CMAKE_SYSROOT}")
    elseif(EXISTS "/usr/${_wreel_triple}/lib")
        list(APPEND WREEL_TEST_EMULATOR -L "/usr/${_wreel_triple}")
    endif()
endif()
