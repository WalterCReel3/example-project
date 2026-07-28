# wreel — SDL2 toy game and live-graphics scaffold.
#
# Targets Linux handhelds (Miyoo Mini, Anbernic) and Steam on Linux from one
# tree. See docs/TARGETS.md for the constraints that shape this build, and
# docs/DEVELOPMENT.md for how to drive it.
#
# 3.21 is required for CMakePresets.json schema v3. Note that the Miyoo Mini
# toolchain container ships CMake 3.13 — docker/miyoomini.Dockerfile layers a
# current CMake on top for exactly this reason.

cmake_minimum_required(VERSION 3.21)

project(wreel
    VERSION     0.2.0
    DESCRIPTION "SDL2 toy game and live-graphics scaffold for Linux handhelds and Steam"
    HOMEPAGE_URL "https://github.com/WalterCReel3/example-project"
    LANGUAGES   CXX C)

# ---------------------------------------------------------------------------
# Guards and defaults
# ---------------------------------------------------------------------------

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "In-source builds are not supported. Use a preset:\n"
        "  cmake --preset desktop-debug")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "" FORCE)
    message(STATUS "No build type selected, defaulting to Debug")
endif()

# Identifies the target in binary names, package names and probe output. Set by
# the toolchain files; "desktop" when building natively without one.
if(NOT DEFINED WREEL_TARGET_ID)
    set(WREEL_TARGET_ID "desktop")
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Keep the original bin/ + lib/ output shape.
set(WREEL_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${WREEL_RUNTIME_OUTPUT_DIRECTORY}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Tests resolve fixtures relative to the repo root, as they always have.
set(WREEL_TEST_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

# Speed up rebuilds across five build trees when ccache is present.
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER   "${CCACHE_PROGRAM}")
endif()

# Handy for clangd / clang-tidy.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(ProjectOptions)
include(Dependencies)

# ---------------------------------------------------------------------------
# Components
#
# Flat module directories, one static library each, public headers hoisted into
# include/<module>/. Dependencies run one way:
#     demo -> loaders -> util -> posix
#          -> rig     -> util
# ---------------------------------------------------------------------------

add_subdirectory(posix)
add_subdirectory(util)
add_subdirectory(rig)
add_subdirectory(audio)
add_subdirectory(gfx)
add_subdirectory(loaders)

if(WREEL_BUILD_PROBE)
    add_subdirectory(probe)
endif()

# One condition per demo rather than one for all of them: skratch needs
# gfx::gles2, coppers needs nothing beyond gfx::renderer. See
# WREEL_BUILD_SKRATCH in cmake/ProjectOptions.cmake.
if(WREEL_BUILD_SKRATCH)
    add_subdirectory(skratch)
endif()

if(WREEL_BUILD_DEMOS)
    add_subdirectory(coppers)
endif()

if(WREEL_BUILD_TESTS)
    enable_testing()
    include(Testing)
    add_subdirectory(tests)
endif()

include(Packaging)
wreel_add_handheld_bundle()

wreel_print_summary()
