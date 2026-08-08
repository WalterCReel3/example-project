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
set(WREEL_PIN_SDL2_MIXER  "release-2.8.2")
set(WREEL_PIN_JSON        "v3.12.0")
set(WREEL_PIN_PUGIXML     "v1.16")
set(WREEL_PIN_GLM         "1.0.3")
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
# Three ways to get an SDL2, and they are alternatives:
#
#   WREEL_MINI_SDL2       pinned upstream + the Miyoo Mini drivers grafted in
#   WREEL_USE_SYSTEM_SDL2 a copy somebody else built — the escape hatch for a
#                         firmware whose SDL2 genuinely cannot be replaced
#   neither               pinned upstream, unmodified — every other target
#
# WREEL_SDL2_LINKAGE is orthogonal to all three and is resolved in
# cmake/ProjectOptions.cmake. Read the note there before changing it: on
# miyoomini it is a licence term rather than a packaging preference.

if(WREEL_SDL2_LINKAGE STREQUAL "SHARED")
    set(_wreel_sdl_shared ON)
    set(_wreel_sdl_static OFF)
    set(_wreel_sdl_target SDL2::SDL2)
else()
    set(_wreel_sdl_shared OFF)
    set(_wreel_sdl_static ON)
    set(_wreel_sdl_target SDL2::SDL2-static)
endif()

# Keep an already-populated SDL2 tree in step with platform/miyoomini/sdl2/src/.
#
# graft.cmake runs from FetchContent's PATCH_COMMAND, and that runs once — when
# the tree is first populated. Without this, every later edit to a driver source
# is invisible to the compiler and the build keeps linking the drivers as they
# were on the day the tree was fetched, silently.
#
# Configure time rather than a custom target, deliberately: configure_file
# records each source as a dependency of the build system, so editing one makes
# `cmake --build` re-run CMake, re-copy and rebuild libSDL2 with nothing to
# remember. A build-time copy would land too late — Ninja decides what is dirty
# before it runs the first edge, so the rebuild would always be one build
# behind the edit.
function(_wreel_sync_mini_drivers mini_dir)
    if(FETCHCONTENT_SOURCE_DIR_SDL2)
        set(sdl_dir "${FETCHCONTENT_SOURCE_DIR_SDL2}")
    else()
        set(sdl_dir "${FETCHCONTENT_BASE_DIR}/sdl2-src")
    endif()

    file(GLOB_RECURSE sources CONFIGURE_DEPENDS
        RELATIVE "${mini_dir}/src" "${mini_dir}/src/*.c" "${mini_dir}/src/*.h")

    # Registered even when there is nothing to copy into yet. On the first
    # configure the tree is populated by PATCH_COMMAND *after* this runs, so
    # without this the first edit after a fresh configure would not re-run CMake
    # and the sync would never get its chance.
    foreach(source IN LISTS sources)
        set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" APPEND PROPERTY
            CMAKE_CONFIGURE_DEPENDS "${mini_dir}/src/${source}")
    endforeach()

    if(NOT EXISTS "${sdl_dir}/src/video/SDL_video.c")
        return()
    endif()

    # A tree populated without the graft — fetched before this option existed,
    # or with WREEL_MINI_SDL2 OFF — keeps its stamp, so PATCH_COMMAND will not
    # run on it now. Copying the sources in would then produce a libSDL2 that
    # compiles, links and has no video driver for this panel, which is a much
    # worse failure than stopping here.
    file(STRINGS "${sdl_dir}/src/video/SDL_video.c" registered
        REGEX "Mini_VideoDriver")
    if(NOT registered)
        message(FATAL_ERROR
            "'${sdl_dir}' was populated without the Miyoo Mini registration "
            "patch, and FetchContent does not re-run PATCH_COMMAND on a tree "
            "that already exists. Delete it and configure again:\n"
            "    rm -rf ${sdl_dir} ${FETCHCONTENT_BASE_DIR}/sdl2-subbuild")
    endif()

    foreach(source IN LISTS sources)
        configure_file("${mini_dir}/src/${source}" "${sdl_dir}/src/${source}"
            COPYONLY)
    endforeach()
endfunction()

if(WREEL_MINI_SDL2)
    # The Miyoo Mini path: pinned upstream SDL2, with the SSD202D drivers
    # grafted in and registered. See platform/miyoomini/sdl2/PROVENANCE.md and
    # planning/2026-07-31-miyoo-sdl2-fork/.
    message(STATUS "SDL2: building pinned ${WREEL_PIN_SDL2} with the Miyoo Mini drivers")

    set(SDL_SHARED  ${_wreel_sdl_shared} CACHE BOOL "" FORCE)
    set(SDL_STATIC  ${_wreel_sdl_static} CACHE BOOL "" FORCE)
    set(SDL_TEST    OFF CACHE BOOL "" FORCE)
    set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)

    set(SDL_MINI    ON  CACHE BOOL "" FORCE)

    # No GPU, and no GL of any kind. The SSD202D has no 3D block, so the only
    # GL reachable here is SwiftShader through a libEGL of unknown licence —
    # which is what the vendor build linked and never called. See § 1.7 of the
    # fork snapshot.
    set(SDL_OPENGL   OFF CACHE BOOL "" FORCE)
    set(SDL_OPENGLES OFF CACHE BOOL "" FORCE)
    set(SDL_KMSDRM   OFF CACHE BOOL "" FORCE)
    set(SDL_WAYLAND  OFF CACHE BOOL "" FORCE)
    set(SDL_X11      OFF CACHE BOOL "" FORCE)
    set(SDL_VULKAN   OFF CACHE BOOL "" FORCE)

    # Audio is MI_AO through the mini driver. Everything else upstream offers
    # here is either absent from the device or a silent fallback that would
    # make a failure look like success.
    set(SDL_OSS        OFF CACHE BOOL "" FORCE)
    set(SDL_ALSA       OFF CACHE BOOL "" FORCE)
    set(SDL_PULSEAUDIO OFF CACHE BOOL "" FORCE)
    set(SDL_PIPEWIRE   OFF CACHE BOOL "" FORCE)
    set(SDL_SNDIO      OFF CACHE BOOL "" FORCE)
    set(SDL_JACK       OFF CACHE BOOL "" FORCE)
    set(SDL_DISKAUDIO  OFF CACHE BOOL "" FORCE)
    set(SDL_DUMMYAUDIO OFF CACHE BOOL "" FORCE)

    # The pad is a keyboard on this device (MIYOO-MINI.md § 6.3) and the mini
    # video driver reads evdev itself, so SDL's joystick and haptic layers have
    # nothing to bind to.
    set(SDL_HIDAPI     OFF CACHE BOOL "" FORCE)
    set(SDL_LIBUDEV    OFF CACHE BOOL "" FORCE)
    set(SDL_DBUS       OFF CACHE BOOL "" FORCE)
    set(SDL_IME        OFF CACHE BOOL "" FORCE)

    # Left ON deliberately: a dummy video driver is a diagnostic, not a
    # fallback. If the mini driver fails to initialise, SDL_VIDEODRIVER=dummy
    # is how you find out whether the failure is the driver or everything else.
    set(SDL_DUMMYVIDEO ON CACHE BOOL "" FORCE)

    FetchContent_Declare(SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        ${WREEL_PIN_SDL2}
        GIT_SHALLOW    TRUE
        PATCH_COMMAND  ${CMAKE_COMMAND}
            -DWREEL_MINI_DIR=${CMAKE_SOURCE_DIR}/platform/miyoomini/sdl2
            -P ${CMAKE_SOURCE_DIR}/platform/miyoomini/sdl2/graft.cmake)

    # Before MakeAvailable, because SDL's own file(GLOB MINI_SOURCES) runs
    # during add_subdirectory: a driver source added here has to be in place by
    # then or it will not be in the build until the configure after next.
    _wreel_sync_mini_drivers("${CMAKE_SOURCE_DIR}/platform/miyoomini/sdl2")

    FetchContent_MakeAvailable(SDL2)

    set(WREEL_SDL2_TARGET ${_wreel_sdl_target})

elseif(WREEL_USE_SYSTEM_SDL2)
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
    message(STATUS "SDL2: building pinned ${WREEL_PIN_SDL2} from source, "
                   "${WREEL_SDL2_LINKAGE}")

    set(SDL_SHARED  ${_wreel_sdl_shared} CACHE BOOL "" FORCE)
    set(SDL_STATIC  ${_wreel_sdl_static} CACHE BOOL "" FORCE)
    set(SDL_TEST    OFF CACHE BOOL "" FORCE)
    set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)

    # KMSDRM is how the handhelds actually reach the display.
    set(SDL_KMSDRM  ON  CACHE BOOL "" FORCE)

    # Forced in BOTH directions, deliberately. Only forcing the OFF case left the
    # ON case at whatever the cache already held, so a build directory configured
    # while a target was believed to have no GPU kept GL and GLES disabled after
    # that belief was corrected — the flag would change and nothing would happen.
    # Reconfigures should not depend on cache history.
    #
    # WREEL_TARGET_HAS_GPU is device capability. It says nothing about which of
    # our renderers is built; that is WREEL_ENABLE_GLES2. Conflating the two is
    # D18.
    if(WREEL_TARGET_HAS_GPU)
        set(SDL_OPENGL   ON  CACHE BOOL "" FORCE)
        set(SDL_OPENGLES ON  CACHE BOOL "" FORCE)
    else()
        # No GPU: don't drag in GL/GLES/EGL detection at all.
        set(SDL_OPENGL   OFF CACHE BOOL "" FORCE)
        set(SDL_OPENGLES OFF CACHE BOOL "" FORCE)
    endif()

    FetchContent_Declare(SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        ${WREEL_PIN_SDL2}
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(SDL2)

    set(WREEL_SDL2_TARGET ${_wreel_sdl_target})
endif()

# ---------------------------------------------------------------------------
# One SDL2, two spellings
# ---------------------------------------------------------------------------
#
# SDL2_image, SDL2_ttf and SDL2_mixer each choose which SDL2 target to look for
# from their OWN linkage, not from SDL2's: a static satellite asks for
# SDL2::SDL2-static, a shared one for SDL2::SDL2. Ours are static on every
# target (see below), so a SHARED SDL2 leaves them looking for a name that does
# not exist. Their fallback is find_package(SDL2), which on the miyoomini
# sysroot finds SDL 1.2 and reports a missing SDL2_LIBRARY — an error that names
# neither the satellite nor the linkage.
#
# `sdl_find_sdl2()` is guarded by `if(NOT TARGET ${TARGET})`, so supplying the
# target under the name it looks for is the intended way to satisfy it from a
# parent build. This aliases a *name*; it does not link anything, and there is
# exactly one SDL2 in the build either way.
#
# It is safe in the direction it is used, and worth knowing why: the satellites
# link SDL2 `PRIVATE $<BUILD_INTERFACE:...>`, so a static satellite uses SDL2
# for its own compilation and propagates nothing to the final link — our
# executables link SDL2 themselves, through WREEL_SDL2_TARGET. Upstream's
# `SDL2_SHARED` COMPATIBLE_INTERFACE_BOOL guard is only set when the satellite
# itself is shared, so it does not fire here.
#
# What would NOT be safe is building SDL2 both ways and letting a static copy
# reach the final link beside the shared one. Two copies of SDL's global state
# in one process is a different class of problem, and it is why SDL_SHARED and
# SDL_STATIC are set from one variable above rather than independently.
if(TARGET SDL2 AND NOT TARGET SDL2::SDL2-static)
    add_library(SDL2::SDL2-static ALIAS SDL2)
elseif(TARGET SDL2-static AND NOT TARGET SDL2::SDL2)
    add_library(SDL2::SDL2 ALIAS SDL2-static)
endif()

# ---------------------------------------------------------------------------
# SDL2_image
# ---------------------------------------------------------------------------
#
# Left NOT vendored deliberately: SDL2_image 2.8 decodes PNG and JPEG through
# its bundled stb_image, so it needs no libpng/libjpeg on any target. The tree
# only ships PNG and JPEG assets (data/*.png, data/test-pattern.jpg), so the
# rest of the formats are switched off to keep the build small.

# The satellites are static on every target, whatever SDL2's own linkage is.
# They are zlib-licensed and consume the SDL2 API rather than the display, so
# none of the reasoning that makes SDL2 shared on miyoomini applies to them.
#
# Left to their defaults they build SHARED, and a shared satellite does assert
# that SDL2 is shared too — CMake catches the disagreement at generate time as a
# COMPATIBLE_INTERFACE_BOOL conflict:
#
#   The INTERFACE_SDL2_SHARED property of "SDL2_ttf" does not agree with the
#   value of SDL2_SHARED already determined for "wreel_gfx".
#
# Static satellites make no such assertion, which is what lets SDL2's linkage
# vary underneath them — see "One SDL2, two spellings" above.
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
# SDL2_mixer
# ---------------------------------------------------------------------------
#
# Audio is a base requirement, so this is not optional. What IS configurable is
# the codec set, via WREEL_AUDIO_CODECS (see cmake/ProjectOptions.cmake).
#
# An important property that makes the tiers cheap: SDL2_mixer dispatches on file
# type at load time, so a compiled-in decoder that never sees a matching file
# costs nothing at runtime. Extra codecs cost BINARY SIZE, not per-frame CPU.
# Per-frame cost lives in the mixer profile — sample rate, buffer, voice count —
# which is configured independently.
#
# Upstream defaults are wrong for us in three ways, all corrected below:
#   - FLAC, MP3, MIDI, Opus and WavPack all default ON
#   - SDL2MIXER_DEPS_SHARED defaults ON, which dlopen()s codec libraries and so
#     conflicts with linking everything static
#   - SDL2MIXER_MIDI pulls FluidSynth/Timidity, which need soundfonts

set(SDL2MIXER_DEPS_SHARED OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_SAMPLES     OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_INSTALL     OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_VENDORED    ON  CACHE BOOL "" FORCE)

# Uncompressed WAV for sound effects: no decode cost at all. Always on.
set(SDL2MIXER_WAVE ON CACHE BOOL "" FORCE)

# Tracker music via libxmp, which SDL2_mixer vendors. Songs are tens of KB
# rather than megabytes, and playback is sample mixing rather than transform
# decoding — which is what makes it the right default on a 128 MB device.
#
# NOTE: libxmp can report row/pattern/tick position, which would be ideal for
# syncing visuals. SDL2_mixer does NOT expose that — Mix_GetMusicPosition() gives
# seconds only. Row-level sync would mean driving libxmp directly, bypassing the
# mixer. Recorded as an option in planning/2026-07-25-midi-live-visuals/.
set(SDL2MIXER_MOD          ON  CACHE BOOL "" FORCE)
set(SDL2MIXER_MOD_XMP      ON  CACHE BOOL "" FORCE)
set(SDL2MIXER_MOD_MODPLUG  OFF CACHE BOOL "" FORCE)

# Always off. Unlike the codecs above, these pull real external dependencies:
# Opus and WavPack need libogg and friends, and MIDI needs FluidSynth or Timidity
# plus a soundfont, which is tens of megabytes.
#
# SDL2MIXER_MIDI is about *playing* MIDI files through a synthesiser. The
# project's MIDI goal is *input* from a hardware controller via RtMidi — a
# different subsystem entirely. Do not conflate them.
set(SDL2MIXER_OPUS    OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_WAVPACK OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_MIDI    OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_GME     OFF CACHE BOOL "" FORCE)
set(SDL2MIXER_CMD     OFF CACHE BOOL "" FORCE)

# Tiered codecs. Every decoder selected here is header-only or vendored, so no
# tier adds an external dependency:
#   stb_vorbis (Ogg), minimp3 (MP3), dr_flac (FLAC), libxmp (tracker).
if(WREEL_AUDIO_CODECS STREQUAL "minimal")
    set(SDL2MIXER_VORBIS ""  CACHE STRING "" FORCE)
    set(SDL2MIXER_MP3    OFF CACHE BOOL "" FORCE)
    set(SDL2MIXER_FLAC   OFF CACHE BOOL "" FORCE)
elseif(WREEL_AUDIO_CODECS STREQUAL "standard")
    set(SDL2MIXER_VORBIS "STB" CACHE STRING "" FORCE)
    set(SDL2MIXER_MP3    OFF CACHE BOOL "" FORCE)
    set(SDL2MIXER_FLAC   OFF CACHE BOOL "" FORCE)
elseif(WREEL_AUDIO_CODECS STREQUAL "full")
    set(SDL2MIXER_VORBIS "STB" CACHE STRING "" FORCE)
    set(SDL2MIXER_MP3          ON  CACHE BOOL "" FORCE)
    set(SDL2MIXER_MP3_MINIMP3  ON  CACHE BOOL "" FORCE)
    set(SDL2MIXER_MP3_MPG123   OFF CACHE BOOL "" FORCE)
    set(SDL2MIXER_FLAC         ON  CACHE BOOL "" FORCE)
    set(SDL2MIXER_FLAC_DRFLAC  ON  CACHE BOOL "" FORCE)
    set(SDL2MIXER_FLAC_LIBFLAC OFF CACHE BOOL "" FORCE)
endif()

message(STATUS "SDL2_mixer: pinned ${WREEL_PIN_SDL2_MIXER}, "
               "codec tier '${WREEL_AUDIO_CODECS}'")

FetchContent_Declare(SDL2_mixer
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
    GIT_TAG        ${WREEL_PIN_SDL2_MIXER}
    GIT_SHALLOW    TRUE
    GIT_SUBMODULES "external/libxmp")
FetchContent_MakeAvailable(SDL2_mixer)

_wreel_resolve_sdl_target(WREEL_SDL2_MIXER_TARGET SDL2_mixer)
message(STATUS "SDL2_mixer target: ${WREEL_SDL2_MIXER_TARGET}")

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
# pugixml
# ---------------------------------------------------------------------------
#
# Two asset formats in the 2D pipeline are XML and are not ours to redefine:
# Sparrow texture atlases (what TexturePacker and ShoeBox export, and what
# data/jetpackdude.xml already is) and Tiled TMX maps. Chosen over tinyxml2 on
# ergonomics rather than size — 24 KB apart on armv7. See docs/TARGETS.md
# § "XML: why pugixml".
#
# Access goes behind the util::xml facade in include/util/xml.hpp; pugi:: does
# not appear in any project signature, which is why this links PRIVATE to
# wreel_util rather than PUBLIC.

# XPath is roughly a third of pugixml's footprint and nothing here needs it:
# Sparrow is one element type under one root, and TMX is addressed by walking
# named children. Compiling it out is the difference between the 40 KB measured
# in TARGETS.md and appreciably more.
set(PUGIXML_NO_XPATH     ON CACHE BOOL "" FORCE)

# Exceptions stay ON. util::xml reports a malformed document by throwing, in
# keeping with posix::wrap and util::File, and pugixml's own load_* returns a
# status object rather than throwing — so this only affects XPath, which is off.
set(PUGIXML_NO_EXCEPTIONS OFF CACHE BOOL "" FORCE)

# The handhelds are Linux and every asset here is UTF-8, so the wchar_t
# interface is dead weight. Off explicitly rather than by default, because the
# default differs across pugixml's build paths.
set(PUGIXML_NO_STL       OFF CACHE BOOL "" FORCE)
set(PUGIXML_WCHAR_MODE   OFF CACHE BOOL "" FORCE)

# pugixml's install rules default ON, unlike nlohmann/json's. Left alone they
# would add pugixml's headers and static library to this project's install
# component and therefore to the CPack bundles.
set(PUGIXML_INSTALL      OFF CACHE BOOL "" FORCE)

FetchContent_Declare(pugixml
    GIT_REPOSITORY https://github.com/zeux/pugixml.git
    GIT_TAG        ${WREEL_PIN_PUGIXML}
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(pugixml)

# ---------------------------------------------------------------------------
# glm
# ---------------------------------------------------------------------------
#
# Vector and matrix maths for the GLES2 renderer and for gfx::Mesh. Replaces
# include/math/vector.hpp, whose operator+ and operator* mutated their left
# operand and returned a reference (D7) -- so `a + b` modified `a`. Taking glm
# deletes that defect rather than repairing it, and supplies the Matrix4,
# dot/cross/length/normalize that a modern GL path needs and that header never
# had.
#
# Header-only, so this costs a clone and no build. Pinned rather than taken from
# libglm-dev because Debian 12 ships 0.9.9.8 and the current tag is 1.0.3, and
# because every other dependency here is pinned for the same reason: five targets
# should not disagree about a library version.
#
# This is a vendor type in module signatures, which docs/TARGETS.md § "Wrap it,
# don't spread it" argues against. Deliberate exception: a maths vector is a value
# type, not a subsystem, and wrapping vec3 buys indirection and nothing else.
# See planning/2026-07-26-gfx-renderer-and-gles2/.

set(GLM_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        ${WREEL_PIN_GLM}
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(glm)

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
# No OpenGL stack
# ---------------------------------------------------------------------------
#
# There is deliberately no find_package(OpenGL), GLEW or GLU here any more.
#
# Nothing in the project links a GL library. gfx::gles2 resolves its entry points
# with SDL_GL_GetProcAddress and wreel-probe does the same for the two it reports
# with, so the only thing that has to be true is that SDL2 was built with GL/GLES
# support — which cmake/Dependencies.cmake decides above from
# WREEL_TARGET_HAS_GPU.
#
# The removed block found GLEW and GLU for the 2016 fixed-function backend, and
# desktop OpenGL for wreel-probe. Requiring the latter is what compiled the probe's
# GL report *out* on rk3326 and h700: find_package(OpenGL) fails in Debian's cross
# environment, so the two targets the report matters most for could never produce
# one.
