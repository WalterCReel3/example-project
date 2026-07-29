# Install layout and packaging.
#
# The original build installed to an in-source dist/ with binaries under bin/.
# That shape is kept — it is convenient for copying onto an SD card — but the
# hard-coded CMAKE_INSTALL_PREFIX override is gone, since it made installing
# anywhere else impossible.
#
# One firmware is defined here — OnionOS, because that is the device in hand.
# The other three (muOS, ArkOS, ROCKNIX) are still NOT defined, for the reason
# this file has always given: confirming a layout needs a real SD card rather
# than documentation, and writing a plausible-looking launcher is inventing a
# decision instead of making one.
#
# The Onion layout here was taken from real packages in OnionUI/Onion and
# OnionUI/Ports-Collection, not from a guide. Reasoning, and what remains
# unverified on hardware, in planning/2026-07-27-onion-bundle/.

include_guard(GLOBAL)
include(GNUInstallDirs)

# ---------------------------------------------------------------------------
# Assets
# ---------------------------------------------------------------------------
#
# data/ is installed next to the binary rather than under share/, because that
# is the layout rig::asset_path() resolves: $WREEL_DATA_DIR, then data/ beside
# the executable, then the working directory. A firmware launcher that cd's
# elsewhere is the case the second rule exists for, so this rule is load-bearing
# and states why rather than being a workaround for a path bug.

install(DIRECTORY "${CMAKE_SOURCE_DIR}/data/"
        DESTINATION "${CMAKE_INSTALL_BINDIR}/data"
        COMPONENT runtime)

# ---------------------------------------------------------------------------
# Handheld bundles
# ---------------------------------------------------------------------------

# WREEL_TARGET_FIRMWARE selects the bundle layout. It is separate from
# WREEL_TARGET_ID because one target id can boot more than one firmware: a Miyoo
# Mini runs stock or OnionOS, and the Mini Flip may well run something else again
# on the same SSD202D. `none` produces the plain prefix tree and nothing else.
set(WREEL_TARGET_FIRMWARE "" CACHE STRING
    "Handheld bundle layout: onion, or none")
set_property(CACHE WREEL_TARGET_FIRMWARE PROPERTY STRINGS none onion)

# Which assets a bundle carries is a property of the program, not of packaging:
# coppers opens five files out of the twenty in data/. Shipping the rest would
# put assets of unknown provenance on an SD card for no reason — see
# data/PROVENANCE.md. The list lives on the target, beside the code that opens
# them, and is read back here.
#
#   set_property(TARGET coppers PROPERTY WREEL_BUNDLE_ASSETS glyphs-16x16.png ...)
define_property(TARGET PROPERTY WREEL_BUNDLE_ASSETS
    BRIEF_DOCS "Files under data/ this target opens at run time"
    FULL_DOCS  "Asset filenames, relative to data/, copied into a firmware bundle")

# ---------------------------------------------------------------------------
# OnionOS — App/<Name>/, verified against OnionUI/Onion's own packages
# ---------------------------------------------------------------------------
#
# An App is a self-contained directory holding config.json and launch.sh, which
# is a better fit for coppers than the Ports layout: Ports scatter a payload, a
# shortcut script and cover art across three trees and are built around
# launch_retroarch.sh. An App directory is also exactly what rig::asset_path()
# resolves against without help.
#
# The layout is named for Onion because that is where it was read from, but it
# is stock MainUI's: the same staged directory, unmodified, was listed and
# launched by the stock firmware's Apps menu on 2026-07-30. So `onion` here is
# narrower than what this bundle actually runs on. Renaming the firmware value
# is deferred until a second firmware needs a genuinely different layout —
# see docs/MIYOO-MINI.md § 6.1.
#
# The bundle is a directory to copy onto an SD card, plus a tarball of it. Not
# CPack: its value is metadata and generators, and this firmware consumes
# neither — Onion's own distribution format is a .7z of precisely this tree.

# The vendored SDL2 declares libGLESv2.so as NEEDED and references not one
# symbol from it, so the loader maps 21.8 MB of SwiftShader before main() to
# satisfy a reference that does not exist. Dropping the entry is what
# `ld --as-needed` would have done at link time had the vendor passed the flag.
#
# ON by default because the bundle is otherwise three quarters GL that nothing
# on a device with no GPU can call. Turn it off to ship the runtime exactly as
# the vendor built it.
option(WREEL_ONION_DROP_GLES
       "Drop the unused libGLESv2.so dependency from the bundled SDL2" ON)

# Appends the strip step to a bundle, and takes the library out of the list of
# files to stage. Both halves are conditional on the same thing, so they cannot
# disagree: a bundle either carries libGLESv2.so and depends on it, or carries
# neither.
#
# The removal is guarded by the symbol comparison that justifies it, evaluated
# at build time against the library actually being shipped — see
# scripts/drop-unused-needed.sh and planning/2026-07-29-gles-free-runtime/.
function(_wreel_onion_drop_gles app_dir libs_var out_command)
    set(${out_command} "" PARENT_SCOPE)

    if(NOT WREEL_ONION_DROP_GLES)
        return()
    endif()

    # Absolute, because the strip step runs from the build directory rather than
    # from wherever WREEL_ONION_SDL2_RUNTIME was written relative to.
    get_filename_component(runtime_dir "${WREEL_ONION_SDL2_RUNTIME}" ABSOLUTE)
    set(provider "${runtime_dir}/libGLESv2.so")
    if(NOT EXISTS "${provider}")
        # A runtime that never carried it. Nothing to do, and not a problem.
        return()
    endif()

    # Optional tools rather than requirements: a host without them ships the
    # larger bundle, which is correct, just bigger. readelf is as load-bearing
    # as patchelf here — without it the check cannot run, and the removal stays
    # conditional on the check rather than on the tool that performs it.
    find_program(WREEL_READELF NAMES readelf)
    find_program(WREEL_PATCHELF NAMES patchelf)
    if(NOT WREEL_READELF OR NOT WREEL_PATCHELF)
        message(STATUS
            "  keeping libGLESv2.so (21.8 MB) — needs readelf and patchelf, and "
            "one of them is missing. The toolchain image carries both; rebuild "
            "it if it predates docker/miyoomini.Dockerfile's patchelf layer.")
        return()
    endif()

    # By name rather than by path: the glob and the normalisation above need not
    # produce the same spelling of the same file.
    list(FILTER ${libs_var} EXCLUDE REGEX "/libGLESv2\\.so$")
    set(${libs_var} "${${libs_var}}" PARENT_SCOPE)

    set(${out_command}
        COMMAND "${CMAKE_COMMAND}" -E env
                "READELF=${WREEL_READELF}" "PATCHELF=${WREEL_PATCHELF}"
                "${CMAKE_SOURCE_DIR}/scripts/drop-unused-needed.sh"
                "${app_dir}/lib/libSDL2-2.0.so.0" "libGLESv2.so"
                "${provider}" "${app_dir}/lib/libGLESv2.so"
        PARENT_SCOPE)

    message(STATUS "  dropping the unused libGLESv2.so dependency (21.8 MB)")
endfunction()

function(_wreel_add_onion_bundle target)
    set(bundle_root "${CMAKE_BINARY_DIR}/bundle/onion")
    set(app_dir     "${bundle_root}/App/Coppers")

    set(WREEL_BUNDLE_BINARY      "${target}")
    set(WREEL_BUNDLE_LABEL       "Coppers")
    set(WREEL_BUNDLE_DESCRIPTION
        "Copper bars, and a fill-rate measurement")

    # The theme-relative convention every Onion app uses. Nothing installs this
    # file — an icon has not been drawn, and writing into the user's theme
    # directory to add one would be overreach. A missing icon is cosmetic.
    set(WREEL_BUNDLE_ICON "../../Icons/Default/app/coppers.png")

    configure_file("${CMAKE_SOURCE_DIR}/cmake/templates/onion/config.json.in"
                   "${app_dir}/config.json" @ONLY)
    configure_file("${CMAKE_SOURCE_DIR}/cmake/templates/onion/launch.sh.in"
                   "${app_dir}/launch.sh" @ONLY)
    file(CHMOD "${app_dir}/launch.sh"
         PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                     GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

    get_target_property(assets ${target} WREEL_BUNDLE_ASSETS)
    if(NOT assets)
        message(FATAL_ERROR
            "Target '${target}' has no WREEL_BUNDLE_ASSETS property, so a bundle "
            "cannot know which files under data/ to carry. Set it in "
            "${target}/CMakeLists.txt.")
    endif()

    set(asset_commands)
    foreach(asset IN LISTS assets)
        if(NOT EXISTS "${CMAKE_SOURCE_DIR}/data/${asset}")
            message(FATAL_ERROR
                "Target '${target}' lists bundle asset 'data/${asset}', which "
                "does not exist.")
        endif()
        list(APPEND asset_commands
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${CMAKE_SOURCE_DIR}/data/${asset}" "${app_dir}/data/${asset}")
    endforeach()

    # The SDL2 runtime, and whatever it drags in with it.
    #
    # Vendored rather than borrowed from the firmware: the copy Onion ships under
    # .tmp_update/lib/parasyte needs a Mesa/X11/libdrm stack and its own loader
    # and libc, which is an alternate userland rather than a library. Onion's own
    # native SDL2 ports vendor theirs the same way.
    #
    # Not fetched automatically. It is a binary of a specific panel geometry —
    # 640x480 is compiled into the mmiyoo driver — and downloading one silently
    # would be guessing at the device on the other end.
    if(WREEL_ONION_SDL2_RUNTIME)
        if(NOT EXISTS "${WREEL_ONION_SDL2_RUNTIME}/libSDL2-2.0.so.0")
            message(FATAL_ERROR
                "WREEL_ONION_SDL2_RUNTIME='${WREEL_ONION_SDL2_RUNTIME}' holds no "
                "libSDL2-2.0.so.0.")
        endif()
        file(GLOB runtime_libs "${WREEL_ONION_SDL2_RUNTIME}/*.so*")

        _wreel_onion_drop_gles("${app_dir}" runtime_libs gles_command)

        list(APPEND asset_commands
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    ${runtime_libs} "${app_dir}/lib/"
            ${gles_command})
        message(STATUS "  runtime from ${WREEL_ONION_SDL2_RUNTIME}")
    else()
        message(WARNING
            "No WREEL_ONION_SDL2_RUNTIME set, so the bundle carries no "
            "libSDL2-2.0.so.0 and launch.sh will refuse to start. Point it at a "
            "directory holding an mmiyoo SDL2 built for this panel.")
    endif()

    # The tarball is built from the parent of App/ so it unpacks straight over
    # the root of an SD card.
    set(tarball "${CMAKE_SOURCE_DIR}/pkg/coppers-${PROJECT_VERSION}-onion.tar.gz")

    add_custom_target(bundle-onion
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${app_dir}/data" "${app_dir}/lib"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:${target}>" "${app_dir}/${target}"
        ${asset_commands}
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_SOURCE_DIR}/pkg"
        COMMAND "${CMAKE_COMMAND}" -E chdir "${bundle_root}"
                "${CMAKE_COMMAND}" -E tar czf "${tarball}" App
        DEPENDS ${target}
        COMMENT "Staging ${app_dir} and writing ${tarball}"
        VERBATIM)

    message(STATUS "Onion bundle: `cmake --build . --target bundle-onion`")
    message(STATUS "  stages ${app_dir}")

    # Said plainly because it is the one thing about this bundle that is not
    # like the others: this target's SDL2 is a shared object the firmware
    # supplies, so the bundle is not self-contained the way every other preset's
    # output is.
    if(NOT WREEL_USE_SYSTEM_SDL2)
        message(WARNING
            "Onion bundle built with the pinned static SDL2. That build has no "
            "video driver for the SSD202D — dummy, offscreen and wayland only — "
            "so it will run headless and draw nothing on the device. Configure "
            "with -DWREEL_USE_SYSTEM_SDL2=ON against the device's SDL2. See "
            "planning/2026-07-27-onion-bundle/.")
    endif()
endfunction()

function(wreel_add_handheld_bundle)
    if(NOT WREEL_TARGET_IS_HANDHELD)
        return()
    endif()

    if(WREEL_TARGET_FIRMWARE STREQUAL "onion")
        if(NOT TARGET coppers)
            message(FATAL_ERROR
                "WREEL_TARGET_FIRMWARE=onion needs the coppers target, which is "
                "not being built. Check WREEL_BUILD_DEMOS.")
        endif()
        _wreel_add_onion_bundle(coppers)
        return()
    endif()

    if(NOT WREEL_TARGET_FIRMWARE STREQUAL "" AND
       NOT WREEL_TARGET_FIRMWARE STREQUAL "none")
        message(FATAL_ERROR
            "Unknown WREEL_TARGET_FIRMWARE '${WREEL_TARGET_FIRMWARE}'. "
            "Implemented: onion. muOS, ArkOS and ROCKNIX layouts are "
            "unconfirmed — see planning/2026-07-25-packaging-distribution/.")
    endif()

    # Reported rather than silently skipped, so it is obvious that `install`
    # produces a prefix tree and not a firmware-ready bundle.
    message(STATUS
        "Handheld target '${WREEL_TARGET_ID}': `install` gives a plain "
        "bin/ + bin/data/ tree.")

    # Onion is a Miyoo firmware, so suggesting it on an Anbernic would be
    # pointing at the wrong SD card.
    if(WREEL_TARGET_ID STREQUAL "miyoomini")
        message(STATUS
            "  No firmware bundle — pass -DWREEL_TARGET_FIRMWARE=onion for the "
            "OnionOS layout.")
    else()
        message(STATUS
            "  No firmware bundle — muOS, ArkOS and ROCKNIX layouts are "
            "unconfirmed. See planning/2026-07-25-packaging-distribution/")
    endif()
endfunction()

# ---------------------------------------------------------------------------
# CPack
# ---------------------------------------------------------------------------
#
# TGZ only. A tarball of the install tree is genuinely useful for getting a build
# onto a device; DEB/RPM are not, since no handheld firmware uses a package
# manager and Steam takes a depot directory.

set(CPACK_PACKAGE_NAME "wreel-${WREEL_TARGET_ID}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_FILE_NAME "wreel-${PROJECT_VERSION}-${WREEL_TARGET_ID}")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_SOURCE_DIR}/pkg")
set(CPACK_GENERATOR "TGZ")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_VERBATIM_VARIABLES ON)

include(CPack)
