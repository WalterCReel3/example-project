# Steam on Linux — Steam Runtime 3.x "sniper"
#
# This is NOT a cross-compile. It is a native x86_64 build that must happen
# *inside* the sniper container so it links against that runtime's glibc (2.31,
# Debian 11 based) rather than your host's. Building on the host and shipping to
# Steam is how you get GLIBC-version crashes on other people's machines.
#
#   docker pull registry.gitlab.steamos.cloud/steamrt/sniper/sdk:latest
#   docker run --rm -it -v "$PWD":/src -w /src \
#       registry.gitlab.steamos.cloud/steamrt/sniper/sdk:latest bash
#
# See docs/DEVELOPMENT.md § "Steam on Linux".

set(WREEL_TARGET_ID          "steam-linux")
set(WREEL_TARGET_IS_HANDHELD OFF)
set(WREEL_TARGET_HAS_GPU     ON)

# Baseline ISA. Steam's hardware survey floor is well above SSE2, but targeting
# generic x86-64 costs nothing here and avoids excluding older machines; the
# Steam Deck's Zen 2 gains nothing from -march=native in a blitting workload.
set(CMAKE_C_FLAGS_INIT   "-m64")
set(CMAKE_CXX_FLAGS_INIT "-m64")

set(CMAKE_C_FLAGS_RELEASE_INIT   "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-O2 -DNDEBUG")

# Carry our own C++ runtime so the binary does not depend on the user's
# libstdc++ being at least as new as the build container's.
set(WREEL_STATIC_CXX ON CACHE BOOL "" FORCE)

# Warn loudly if this is being used outside the runtime container. The sniper
# images identify themselves in /etc/os-release.
if(EXISTS "/etc/os-release")
    file(READ "/etc/os-release" _wreel_os_release)
    if(NOT _wreel_os_release MATCHES "steamrt|sniper")
        message(WARNING
            "steam-linux.cmake is being used outside a Steam Runtime container.\n"
            "  The resulting binary will carry this host's glibc requirements and\n"
            "  may fail to launch on other users' systems.\n"
            "  See docs/DEVELOPMENT.md § 'Steam on Linux'.")
    endif()
endif()
