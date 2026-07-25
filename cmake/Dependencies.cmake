# Third-party dependencies.
#
# Everything is pinned to an explicit tag and built from source, so all five
# targets agree on library versions regardless of what the host or device SDK
# happens to ship. Rationale in docs/TARGETS.md § "Pinned dependencies".
#
# The original build's approach — ExternalProject_Add with an unpinned
# GIT_REPOSITORY pointing at an org that has since moved — is what this replaces.

include_guard(GLOBAL)
include(FetchContent)

# Pins. Verified against upstream release tags.
set(WREEL_PIN_SDL2        "release-2.32.10")
set(WREEL_PIN_SDL2_IMAGE  "release-2.8.12")
set(WREEL_PIN_SDL2_TTF    "release-2.24.0")
set(WREEL_PIN_JSON        "v3.12.0")
set(WREEL_PIN_DOCTEST     "v2.5.3")

# Dependency sources land in the build tree by default. Cross builds must not
# share a build directory, but the *downloads* can be shared across presets by
# pointing FETCHCONTENT_SOURCE_DIR_<UPPERCASE_NAME> at a common checkout, or by
# setting FETCHCONTENT_BASE_DIR for a single-arch workflow.

# CMake 4.0 removed compatibility with cmake_minimum_required(VERSION < 3.5), and
# some vendored third-party trees still declare one — SDL2_ttf's bundled FreeType
# asks for 2.8. Without this, configuring under CMake 4.x fails inside a
# dependency we do not control:
#
#   CMake Error at .../external/freetype/CMakeLists.txt:113:
#     Compatibility with CMake < 3.5 has been removed from CMake.
#
# Debian 12's CMake 3.25 never hits this; newer hosts do. Scoped here rather than
# project-wide so it only ever relaxes dependency builds, never our own.
if(CMAKE_VERSION VERSION_GREATER_EQUAL 4.0)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING
        "Floor for third-party cmake_minimum_required under CMake 4.x" FORCE)
endif()

# ---------------------------------------------------------------------------
# SDL2
# ---------------------------------------------------------------------------
#
# WREEL_USE_SYSTEM_SDL2 exists for devices whose firmware ships a *patched* SDL2
# that upstream cannot replace — notably Miyoo Mini stock firmware, which routes
# video through SigmaStar's MI_GFX layer.

if(WREEL_USE_SYSTEM_SDL2)
    find_package(SDL2 REQUIRED)
    message(STATUS "SDL2: using system/sysroot copy")

    if(NOT TARGET SDL2::SDL2)
        message(FATAL_ERROR
            "Found SDL2 but no SDL2::SDL2 imported target. This usually means an\n"
            "  old FindSDL2.cmake is on CMAKE_MODULE_PATH rather than SDL2's own\n"
            "  SDL2Config.cmake. Check CMAKE_PREFIX_PATH points at the sysroot.")
    endif()
    set(WREEL_SDL2_TARGET SDL2::SDL2)
else()
    message(STATUS "SDL2: building pinned ${WREEL_PIN_SDL2} from source")

    # Static everywhere: handhelds have no sane place to drop a shared object,
    # and Steam builds are simpler to ship self-contained.
    set(SDL_SHARED  OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC  ON  CACHE BOOL "" FORCE)
    set(SDL_TEST    OFF CACHE BOOL "" FORCE)
    set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)

    # KMSDRM is how the handhelds actually reach the display.
    set(SDL_KMSDRM  ON  CACHE BOOL "" FORCE)

    if(NOT WREEL_TARGET_HAS_GPU)
        # No GPU: don't drag in GL/GLES/EGL detection at all.
        set(SDL_OPENGL   OFF CACHE BOOL "" FORCE)
        set(SDL_OPENGLES OFF CACHE BOOL "" FORCE)
    endif()

    FetchContent_Declare(SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        ${WREEL_PIN_SDL2}
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(SDL2)

    set(WREEL_SDL2_TARGET SDL2::SDL2-static)
endif()

# ---------------------------------------------------------------------------
# SDL2_image
# ---------------------------------------------------------------------------
#
# Left NOT vendored deliberately: SDL2_image 2.8 decodes PNG and JPEG through
# its bundled stb_image, so it needs no libpng/libjpeg on any target. The tree
# only ships PNG and JPEG assets (data/*.png, data/test-pattern.jpg), so the
# rest of the formats are switched off to keep the build small.

# The SDL satellite libraries must match SDL2's linkage. Left to their defaults
# they build shared and link SDL2::SDL2, while we link SDL2::SDL2-static — which
# CMake catches at generate time as a COMPATIBLE_INTERFACE_BOOL conflict:
#
#   The INTERFACE_SDL2_SHARED property of "SDL2_ttf" does not agree with the
#   value of SDL2_SHARED already determined for "wreel_gfx".
#
# BUILD_SHARED_LIBS is the switch both projects actually consult; the explicit
# per-project options below are belt and braces.
set(BUILD_SHARED_LIBS  OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_SHARED   OFF CACHE BOOL "" FORCE)
set(SDL2TTF_SHARED     OFF CACHE BOOL "" FORCE)

set(SDL2IMAGE_VENDORED    OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_SAMPLES     OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_INSTALL     OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_DEPS_SHARED OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_PNG  ON  CACHE BOOL "" FORCE)
set(SDL2IMAGE_JPG  ON  CACHE BOOL "" FORCE)
set(SDL2IMAGE_WEBP OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_AVIF OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_TIF  OFF CACHE BOOL "" FORCE)

FetchContent_Declare(SDL2_image
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
    GIT_TAG        ${WREEL_PIN_SDL2_IMAGE}
    GIT_SHALLOW    TRUE
    GIT_SUBMODULES "")

# ---------------------------------------------------------------------------
# SDL2_ttf
# ---------------------------------------------------------------------------
#
# Vendored FreeType on every target, not just cross builds. There is no stb
# fallback for font rasterising, so the alternative is requiring libfreetype-dev
# on the host and a matching FreeType in every device sysroot — which defeats
# the point of pinning. Vendoring costs one extra library build and makes all
# five targets identical.
#
# HarfBuzz is off (complex text shaping we don't need — the HUD is ASCII), and
# GIT_SUBMODULES is narrowed to FreeType so its ~30 MB clone is skipped entirely.

set(SDL2TTF_VENDORED ON  CACHE BOOL "" FORCE)
set(SDL2TTF_SAMPLES  OFF CACHE BOOL "" FORCE)
set(SDL2TTF_INSTALL  OFF CACHE BOOL "" FORCE)
set(SDL2TTF_HARFBUZZ OFF CACHE BOOL "" FORCE)

FetchContent_Declare(SDL2_ttf
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
    GIT_TAG        ${WREEL_PIN_SDL2_TTF}
    GIT_SHALLOW    TRUE
    GIT_SUBMODULES "external/freetype")

FetchContent_MakeAvailable(SDL2_image SDL2_ttf)

# Both projects name their target SDL2_x::SDL2_x when shared and
# SDL2_x::SDL2_x-static when static, so the name depends on BUILD_SHARED_LIBS
# above. Resolve it once here instead of hard-coding a guess at each use site.
function(_wreel_resolve_sdl_target out_var base)
    if(TARGET ${base}::${base}-static)
        set(${out_var} ${base}::${base}-static PARENT_SCOPE)
    elseif(TARGET ${base}::${base})
        set(${out_var} ${base}::${base} PARENT_SCOPE)
    else()
        message(FATAL_ERROR
            "${base} was populated but exported neither ${base}::${base} nor "
            "${base}::${base}-static. Upstream target naming may have changed; "
            "check the pinned tag.")
    endif()
endfunction()

_wreel_resolve_sdl_target(WREEL_SDL2_IMAGE_TARGET SDL2_image)
_wreel_resolve_sdl_target(WREEL_SDL2_TTF_TARGET   SDL2_ttf)

message(STATUS "SDL2 satellite targets: ${WREEL_SDL2_IMAGE_TARGET}, "
               "${WREEL_SDL2_TTF_TARGET}")

# ---------------------------------------------------------------------------
# nlohmann/json
# ---------------------------------------------------------------------------
#
# Replaces RapidJSON (dormant since 2025-02, only tag from 2016). Chosen on
# support breadth: GCC 4.8–14.2 is the only published matrix that explicitly
# spans this project's GCC 8.3 floor. See docs/TARGETS.md § "JSON".

set(JSON_BuildTests   OFF CACHE INTERNAL "")
set(JSON_Install      OFF CACHE INTERNAL "")
set(JSON_MultipleHeaders OFF CACHE INTERNAL "")

FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        ${WREEL_PIN_JSON}
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(nlohmann_json)

# ---------------------------------------------------------------------------
# doctest
# ---------------------------------------------------------------------------
#
# Single header, C++11, no per-target build — which is why it beat Catch2 v3 and
# googletest here. Both of those are compiled libraries that would need building
# inside all five toolchain containers.

if(WREEL_BUILD_TESTS)
    set(DOCTEST_WITH_TESTS OFF CACHE BOOL "" FORCE)
    set(DOCTEST_NO_INSTALL ON  CACHE BOOL "" FORCE)

    FetchContent_Declare(doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG        ${WREEL_PIN_DOCTEST}
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(doctest)
endif()

# ---------------------------------------------------------------------------
# OpenGL stack — only for the gl_legacy backend
# ---------------------------------------------------------------------------
#
# The 2016 renderer needs desktop GL, GLU (gluPerspective) and GLEW. None of
# that exists on a Miyoo Mini, which is the whole reason for backend gating.

if(WREEL_GFX_BACKEND STREQUAL "gl_legacy")
    set(OpenGL_GL_PREFERENCE GLVND)
    find_package(OpenGL REQUIRED COMPONENTS OpenGL)
    find_package(GLEW REQUIRED)

    if(NOT OPENGL_GLU_FOUND)
        message(FATAL_ERROR
            "The gl_legacy backend calls gluPerspective(), but GLU was not found.\n"
            "  Install libglu1-mesa-dev, or use -DWREEL_GFX_BACKEND=software.")
    endif()
endif()
