# Graft the Miyoo Mini drivers into a freshly-populated SDL2 source tree.
#
# Run as `cmake -P` from FetchContent's PATCH_COMMAND, whose working directory
# is the populated SDL2 source directory. Two steps, in order:
#
#   1. copy platform/miyoomini/sdl2/src/ over SDL2's src/, adding
#      {video,render,audio}/mini/ and touching nothing else
#   2. apply patches/0001-register-mini-drivers.patch, which is the only edit
#      to code we did not write
#
# Both steps are idempotent, because PATCH_COMMAND is not guaranteed to run
# exactly once: a reconfigure after an interrupted populate can repeat it.
#
# WREEL_MINI_DIR must be passed with -D; the SDL tree is the working directory.

cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED WREEL_MINI_DIR)
    message(FATAL_ERROR "graft.cmake: -DWREEL_MINI_DIR=<platform/miyoomini/sdl2> is required")
endif()

set(sdl_dir "${CMAKE_CURRENT_SOURCE_DIR}")

# Refuse to graft into anything that is not SDL2. A wrong working directory
# would otherwise scatter driver sources somewhere harmless-looking and fail
# much later, during the build, with a confusing error.
if(NOT EXISTS "${sdl_dir}/include/SDL_version.h"
   OR NOT EXISTS "${sdl_dir}/src/render/SDL_sysrender.h")
    message(FATAL_ERROR
        "graft.cmake: '${sdl_dir}' does not look like an SDL2 source tree.\n"
        "  Expected include/SDL_version.h and src/render/SDL_sysrender.h.")
endif()

# ---------------------------------------------------------------------------
# 1. The driver sources
# ---------------------------------------------------------------------------

file(COPY "${WREEL_MINI_DIR}/src" DESTINATION "${sdl_dir}")

foreach(expected
        src/video/mini/SDL_video_mini.c
        src/render/mini/SDL_render_mini.c
        src/audio/mini/SDL_audio_mini.c)
    if(NOT EXISTS "${sdl_dir}/${expected}")
        message(FATAL_ERROR "graft.cmake: ${expected} is missing after the copy.")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# 2. The registration patch
# ---------------------------------------------------------------------------
#
# Checked with --reverse first. `git apply` fails on an already-applied patch
# the same way it fails on a patch that does not fit, and those two need
# different responses: the first is fine and the second must stop the build.

set(patch "${WREEL_MINI_DIR}/patches/0001-register-mini-drivers.patch")

find_program(GIT_EXECUTABLE git REQUIRED)

execute_process(
    COMMAND "${GIT_EXECUTABLE}" apply --reverse --check "${patch}"
    WORKING_DIRECTORY "${sdl_dir}"
    RESULT_VARIABLE already_applied
    OUTPUT_QUIET ERROR_QUIET)

if(already_applied EQUAL 0)
    message(STATUS "SDL2 mini drivers: registration patch already applied")
    return()
endif()

execute_process(
    COMMAND "${GIT_EXECUTABLE}" apply --verbose "${patch}"
    WORKING_DIRECTORY "${sdl_dir}"
    RESULT_VARIABLE applied
    ERROR_VARIABLE apply_error)

if(NOT applied EQUAL 0)
    # Two very different faults land here, and they need opposite responses.
    # A tree that already carries the *registration* but does not match this
    # patch was populated with an earlier revision of it — editing the patch
    # while a populated tree exists is enough, and the patch step does re-run.
    # That is not a pin problem and regenerating the patch would make it worse.
    if(EXISTS "${sdl_dir}/src/video/SDL_video.c")
        file(STRINGS "${sdl_dir}/src/video/SDL_video.c" registered
            REGEX "Mini_VideoDriver")
    endif()

    if(registered)
        message(FATAL_ERROR
            "graft.cmake: '${sdl_dir}' was populated with a different revision "
            "of ${patch}, so it neither reverses nor re-applies. Delete the "
            "fetched tree and configure again:\n"
            "    rm -rf ${sdl_dir} ${sdl_dir}/../sdl2-subbuild\n"
            "  The patch itself is fine; nothing needs regenerating.")
    endif()

    message(FATAL_ERROR
        "graft.cmake: could not apply ${patch}\n"
        "  ${apply_error}\n"
        "  The SDL2 pin has probably moved. Regenerate the patch against the new\n"
        "  tag — platform/miyoomini/sdl2/README.md says how — rather than\n"
        "  loosening this check.")
endif()

message(STATUS "SDL2 mini drivers: grafted and registered")
