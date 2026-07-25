# Project-wide options and the interface targets that carry them.
#
# Everything here is expressed as INTERFACE targets rather than global
# CMAKE_CXX_FLAGS mutation, so that vendored dependency sources (SDL2 et al.)
# never inherit our warning settings.

include_guard(GLOBAL)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------

set(WREEL_GFX_BACKEND_VALUES software gl_legacy)
set(WREEL_GFX_BACKEND "" CACHE STRING
    "Graphics backend: ${WREEL_GFX_BACKEND_VALUES}")
set_property(CACHE WREEL_GFX_BACKEND PROPERTY STRINGS ${WREEL_GFX_BACKEND_VALUES})

option(WREEL_BUILD_TESTS  "Build the doctest suite"              ON)
option(WREEL_BUILD_DEMOS  "Build the skratch demo application"   ON)
option(WREEL_BUILD_PROBE  "Build the wreel-probe device tool"    ON)
option(WREEL_USE_SYSTEM_SDL2
       "Link the sysroot's SDL2 instead of building a pinned copy" OFF)

# Legacy 2016 sources do not survive -Wall -Wextra -Werror yet. This flips to ON
# as the C++17 cleanup lands; see README.md's checklist.
option(WREEL_WERROR "Treat warnings as errors" OFF)

option(WREEL_STATIC_CXX
       "Static-link libstdc++/libgcc (recommended for device builds)" OFF)

# ---------------------------------------------------------------------------
# Audio
# ---------------------------------------------------------------------------
#
# Audio is a base requirement: wreel::audio is always built and every target gets
# it. What varies is the codec set and the mixer profile, and it is worth being
# precise about which of those costs what:
#
#   WREEL_AUDIO_CODECS  affects BINARY SIZE. SDL2_mixer dispatches on file type
#                       at load time, so a decoder that never sees a matching
#                       file never runs. Compiling in FLAC costs bytes on disk,
#                       not cycles per frame.
#
#   The mixer profile    affects PER-FRAME CPU. Sample rate, channel count, voice
#                       count and buffer size determine how much mixing work
#                       happens in every audio callback, whatever is playing.
#
# So "support a FLAC-capable audio player without slowing the game down" is
# satisfied by codec tier 'full' plus a modest mixer profile — the two are
# independent.

set(WREEL_AUDIO_CODEC_VALUES minimal standard full)
set(WREEL_AUDIO_CODECS "" CACHE STRING
    "Audio codec tier: ${WREEL_AUDIO_CODEC_VALUES}")
set_property(CACHE WREEL_AUDIO_CODECS PROPERTY STRINGS ${WREEL_AUDIO_CODEC_VALUES})

# Handhelds default to 'standard' to keep binaries small; desktop and Steam get
# 'full'. Either can be overridden freely — 'full' adds no external dependency,
# so a handheld audio-player build is a one-flag change.
if(NOT WREEL_AUDIO_CODECS)
    if(WREEL_TARGET_IS_HANDHELD)
        set(WREEL_AUDIO_CODECS "standard" CACHE STRING "" FORCE)
    else()
        set(WREEL_AUDIO_CODECS "full" CACHE STRING "" FORCE)
    endif()
endif()

if(NOT WREEL_AUDIO_CODECS IN_LIST WREEL_AUDIO_CODEC_VALUES)
    message(FATAL_ERROR
        "WREEL_AUDIO_CODECS='${WREEL_AUDIO_CODECS}' is not one of: "
        "${WREEL_AUDIO_CODEC_VALUES}\n"
        "  minimal   WAV + tracker (MOD/XM/IT)\n"
        "  standard  + Ogg Vorbis\n"
        "  full      + MP3 + FLAC   (all header-only decoders, no extra deps)")
endif()

# Mixer profile — this is where per-frame cost lives.
#
# 22050 Hz halves mixing work versus 44100 and is ample for tracker music, which
# is why the handhelds default to it. A 2048-sample buffer is ~93 ms at 22050 Hz;
# smaller buffers underrun on two Cortex-A7 cores that are also software-
# rasterising. Desktop can afford 44100 with a 1024-sample buffer (~23 ms).
if(WREEL_TARGET_IS_HANDHELD)
    set(_wreel_rate 22050)
    set(_wreel_buffer 2048)
    set(_wreel_voices 8)
else()
    set(_wreel_rate 44100)
    set(_wreel_buffer 1024)
    set(_wreel_voices 16)
endif()

set(WREEL_AUDIO_RATE     "${_wreel_rate}"   CACHE STRING "Mixer sample rate (Hz)")
set(WREEL_AUDIO_BUFFER   "${_wreel_buffer}" CACHE STRING "Mixer buffer in samples")
set(WREEL_AUDIO_CHANNELS "2"                CACHE STRING "Output channels: 1 mono, 2 stereo")
set(WREEL_AUDIO_VOICES   "${_wreel_voices}" CACHE STRING "Simultaneous sound effect voices")

# ---------------------------------------------------------------------------
# Backend defaulting and validation
# ---------------------------------------------------------------------------
#
# The Miyoo Mini (SigmaStar SSD202D) has no GPU at all, so `software` is the only
# possible backend there. Toolchain files set WREEL_TARGET_HAS_GPU to say whether
# a GL context is even conceivable.

if(NOT DEFINED WREEL_TARGET_HAS_GPU)
    set(WREEL_TARGET_HAS_GPU ON)
endif()

if(NOT WREEL_GFX_BACKEND)
    if(WREEL_TARGET_HAS_GPU)
        set(WREEL_GFX_BACKEND "gl_legacy" CACHE STRING "" FORCE)
    else()
        set(WREEL_GFX_BACKEND "software" CACHE STRING "" FORCE)
    endif()
    message(STATUS "WREEL_GFX_BACKEND not set, defaulting to '${WREEL_GFX_BACKEND}'")
endif()

if(NOT WREEL_GFX_BACKEND IN_LIST WREEL_GFX_BACKEND_VALUES)
    message(FATAL_ERROR
        "WREEL_GFX_BACKEND='${WREEL_GFX_BACKEND}' is not one of: "
        "${WREEL_GFX_BACKEND_VALUES}")
endif()

if(WREEL_GFX_BACKEND STREQUAL "gl_legacy" AND NOT WREEL_TARGET_HAS_GPU)
    message(FATAL_ERROR
        "WREEL_GFX_BACKEND=gl_legacy was requested, but this target has no GPU.\n"
        "  The gl_legacy backend needs desktop OpenGL, GLU and GLEW. Use\n"
        "  -DWREEL_GFX_BACKEND=software instead. See docs/TARGETS.md.")
endif()

# The 2016 demo drives fixed-function OpenGL directly from
# skratch/application.cc, so it cannot follow the software backend.
if(WREEL_BUILD_DEMOS AND NOT WREEL_GFX_BACKEND STREQUAL "gl_legacy")
    message(STATUS
        "skratch demo needs the gl_legacy backend; disabling it for "
        "backend='${WREEL_GFX_BACKEND}'")
    set(WREEL_BUILD_DEMOS OFF)
endif()

# ---------------------------------------------------------------------------
# wreel_options — language level and per-config codegen
# ---------------------------------------------------------------------------

# Raised here rather than in the toolchain file, which is re-included for every
# try_compile and so would print this four or more times per configure.
if(CMAKE_CROSSCOMPILING AND DEFINED WREEL_BUILD_IS_SHIPPABLE
        AND NOT WREEL_BUILD_IS_SHIPPABLE)
    message(WARNING
        "No WREEL_SYSROOT set — using the host cross-GCC.\n"
        "  This build is COMPILE-CHECK ONLY. Its glibc requirements are newer\n"
        "  than any handheld's, so it will not run on device.\n"
        "  Pass -DWREEL_SYSROOT=/path/to/device/rootfs for a shippable build.\n"
        "  See docs/TARGETS.md § 2.")
endif()

add_library(wreel_options INTERFACE)
add_library(wreel::options ALIAS wreel_options)

# C++17 is the ceiling, not a preference: the Miyoo Mini toolchain is GCC 8.3.
# See docs/TARGETS.md § "C++17 is the ceiling".
target_compile_features(wreel_options INTERFACE cxx_std_17)

set_target_properties(wreel_options PROPERTIES
    INTERFACE_CXX_EXTENSIONS OFF)

target_compile_definitions(wreel_options INTERFACE
    $<$<CONFIG:Debug>:WREEL_DEBUG=1>
    $<$<NOT:$<CONFIG:Debug>>:NDEBUG>)

# GCC 8 and 9 need std::filesystem pulled in explicitly.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 10)
    target_link_libraries(wreel_options INTERFACE stdc++fs)
endif()

if(WREEL_STATIC_CXX)
    target_link_options(wreel_options INTERFACE
        -static-libstdc++ -static-libgcc)
endif()

# ---------------------------------------------------------------------------
# wreel_warnings — diagnostics, kept off dependency sources
# ---------------------------------------------------------------------------

add_library(wreel_warnings INTERFACE)
add_library(wreel::warnings ALIAS wreel_warnings)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(wreel_warnings INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wdouble-promotion
        -Wformat=2)

    # The original CMakeLists tested `if(GCC)` — an undefined variable — so
    # -Wall -Werror was silently never applied for nine years.
    if(WREEL_WERROR)
        target_compile_options(wreel_warnings INTERFACE -Werror)
    endif()
endif()

# ---------------------------------------------------------------------------
# Convenience: apply both to a target
# ---------------------------------------------------------------------------

function(wreel_set_target_defaults target)
    target_link_libraries(${target} PUBLIC wreel::options)
    target_link_libraries(${target} PRIVATE wreel::warnings)
endfunction()

# ---------------------------------------------------------------------------
# Configuration summary
# ---------------------------------------------------------------------------

function(wreel_print_summary)
    message(STATUS "")
    message(STATUS "=== ${PROJECT_NAME} ${PROJECT_VERSION} ===")
    message(STATUS "  target id .......... ${WREEL_TARGET_ID}")
    message(STATUS "  system ............. ${CMAKE_SYSTEM_NAME} / ${CMAKE_SYSTEM_PROCESSOR}")
    message(STATUS "  build type ......... ${CMAKE_BUILD_TYPE}")
    message(STATUS "  compiler ........... ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
    message(STATUS "  C++ standard ....... 17")
    message(STATUS "  gfx backend ........ ${WREEL_GFX_BACKEND}")
    message(STATUS "  target has GPU ..... ${WREEL_TARGET_HAS_GPU}")
    message(STATUS "  audio codecs ....... ${WREEL_AUDIO_CODECS}")
    message(STATUS "  audio mixer ........ ${WREEL_AUDIO_RATE} Hz, "
                   "${WREEL_AUDIO_CHANNELS} ch, ${WREEL_AUDIO_BUFFER} buf, "
                   "${WREEL_AUDIO_VOICES} voices")
    message(STATUS "  system SDL2 ........ ${WREEL_USE_SYSTEM_SDL2}")
    message(STATUS "  warnings as errors . ${WREEL_WERROR}")
    message(STATUS "  static libstdc++ ... ${WREEL_STATIC_CXX}")
    message(STATUS "  tests .............. ${WREEL_BUILD_TESTS}")
    message(STATUS "  skratch demo ....... ${WREEL_BUILD_DEMOS}")
    message(STATUS "  wreel-probe ........ ${WREEL_BUILD_PROBE}")
    message(STATUS "  install prefix ..... ${CMAKE_INSTALL_PREFIX}")
    message(STATUS "")
endfunction()
