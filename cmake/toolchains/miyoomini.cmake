# Miyoo Mini / Miyoo Mini Plus — SigmaStar SSD202D
#
# Paths and the triple below are taken from union-miyoomini-toolchain's own
# support/setup-env.sh, not guessed:
#
#     PATH          /opt/miyoomini-toolchain/usr/bin
#     CROSS_COMPILE /opt/miyoomini-toolchain/usr/bin/arm-linux-gnueabihf-
#     PREFIX        /opt/miyoomini-toolchain/usr/arm-linux-gnueabihf/sysroot/usr
#
# The bundled compiler is the GNU A-profile toolchain 8.3-2019.03 — GCC 8.3 —
# which is what fixes C++17 as this project's ceiling.
#
#   https://github.com/shauninman/union-miyoomini-toolchain
#
# Usage: run inside the toolchain container (`make shell`), or point
# MIYOOMINI_TOOLCHAIN_ROOT at an extracted copy.

set(CMAKE_SYSTEM_NAME      Linux)
set(CMAKE_SYSTEM_PROCESSOR armv7-a)

set(WREEL_TARGET_ID          "miyoomini")
set(WREEL_TARGET_IS_HANDHELD ON)

# The SSD202D has two Cortex-A7 cores and NO 3D block at all. There is no GL, no
# GLES, no EGL — the software renderer is the only option, not a fallback.
set(WREEL_TARGET_HAS_GPU OFF)

# ---------------------------------------------------------------------------
# Toolchain location
# ---------------------------------------------------------------------------

# TWO MODES, matching cmake/toolchains/aarch64-handheld.cmake:
#
#   Device toolchain (shippable) — the union-miyoomini-toolchain container, or an
#       extracted copy pointed at by MIYOOMINI_TOOLCHAIN_ROOT. GCC 8.3 against a
#       device-matched sysroot. This is the only mode whose output runs on
#       hardware.
#
#   Debian armhf cross-GCC (compile-check ONLY) — used automatically when the
#       device toolchain is absent. Its glibc is far newer than the device's, so
#       binaries will not load there. It is still valuable: it exercises 32-bit
#       ARM codegen, the Cortex-A7 flags and off_t width in seconds, without a
#       279 MB toolchain download.
#
# Falling back rather than hard-failing keeps `cmake --preset miyoomini` useful
# outside the container and consistent with the aarch64 presets.

if(NOT MIYOOMINI_TOOLCHAIN_ROOT)
    if(DEFINED ENV{MIYOOMINI_TOOLCHAIN_ROOT})
        set(MIYOOMINI_TOOLCHAIN_ROOT "$ENV{MIYOOMINI_TOOLCHAIN_ROOT}")
    else()
        set(MIYOOMINI_TOOLCHAIN_ROOT "/opt/miyoomini-toolchain")
    endif()
endif()

set(_wreel_triple "arm-linux-gnueabihf")
set(_wreel_tc     "${MIYOOMINI_TOOLCHAIN_ROOT}")

if(EXISTS "${_wreel_tc}/usr/bin/${_wreel_triple}-gcc")
    set(CMAKE_C_COMPILER   "${_wreel_tc}/usr/bin/${_wreel_triple}-gcc")
    set(CMAKE_CXX_COMPILER "${_wreel_tc}/usr/bin/${_wreel_triple}-g++")
    set(CMAKE_SYSROOT      "${_wreel_tc}/usr/${_wreel_triple}/sysroot")
    set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}")
    set(WREEL_BUILD_IS_SHIPPABLE ON)
else()
    find_program(_wreel_armhf_cc  "${_wreel_triple}-gcc")
    find_program(_wreel_armhf_cxx "${_wreel_triple}-g++")

    if(NOT _wreel_armhf_cc OR NOT _wreel_armhf_cxx)
        message(FATAL_ERROR
            "No armv7 toolchain available.\n"
            "  Device toolchain not found at '${_wreel_tc}'\n"
            "    (run inside union-miyoomini-toolchain, or pass\n"
            "     -DMIYOOMINI_TOOLCHAIN_ROOT=/path/to/miyoomini-toolchain)\n"
            "  and Debian's cross compiler is not installed either:\n"
            "    sudo apt install crossbuild-essential-armhf\n"
            "  See docs/DEVELOPMENT.md § 'Miyoo Mini (shippable)'.")
    endif()

    set(CMAKE_C_COMPILER   "${_wreel_armhf_cc}")
    set(CMAKE_CXX_COMPILER "${_wreel_armhf_cxx}")
    set(WREEL_BUILD_IS_SHIPPABLE OFF)
endif()

# ---------------------------------------------------------------------------
# Codegen
# ---------------------------------------------------------------------------
#
# Cortex-A7 with NEON and VFPv4, hard float ABI.

set(_wreel_arch_flags
    "-march=armv7-a -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard")

set(CMAKE_C_FLAGS_INIT   "${_wreel_arch_flags}")
set(CMAKE_CXX_FLAGS_INIT "${_wreel_arch_flags}")

# 128 MB of RAM total, shared with the OS: optimise for size over speed.
set(CMAKE_C_FLAGS_RELEASE_INIT   "-Os -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-Os -DNDEBUG")

# The device's libstdc++ is older than the toolchain's; carrying our own avoids
# the C++ half of the glibc-version problem. See docs/TARGETS.md § 2.
set(WREEL_STATIC_CXX ON CACHE BOOL "" FORCE)

# ---------------------------------------------------------------------------
# find() behaviour — look in the sysroot, not on the host
# ---------------------------------------------------------------------------

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# ---------------------------------------------------------------------------
# Running cross-built tests
# ---------------------------------------------------------------------------
#
# qemu-user-static lets ctest execute armhf binaries on the dev box. Not present
# inside the toolchain container, so this is best-effort.

# -L points qemu at the target's dynamic loader, which does not exist at its
# native path on an x86_64 host. See the equivalent note in
# cmake/toolchains/aarch64-handheld.cmake.
find_program(_wreel_qemu_arm qemu-arm-static qemu-arm)
if(_wreel_qemu_arm)
    set(WREEL_TEST_EMULATOR "${_wreel_qemu_arm}")
    if(CMAKE_SYSROOT)
        list(APPEND WREEL_TEST_EMULATOR -L "${CMAKE_SYSROOT}")
    elseif(EXISTS "/usr/${_wreel_triple}/lib")
        list(APPEND WREEL_TEST_EMULATOR -L "/usr/${_wreel_triple}")
    endif()
endif()
