# Install layout and packaging.
#
# The original build installed to an in-source dist/ with binaries under bin/.
# That shape is kept — it is convenient for copying onto an SD card — but the
# hard-coded CMAKE_INSTALL_PREFIX override is gone, since it made installing
# anywhere else impossible.
#
# Deliberately thin. Per-firmware bundle layouts are NOT defined here because
# they are not known yet: they differ across OnionOS, muOS, ArkOS and ROCKNIX,
# and confirming them needs a real SD card rather than documentation. Writing a
# plausible-looking launcher now would be inventing a decision instead of making
# one. See planning/2026-07-25-packaging-distribution/.

include_guard(GLOBAL)
include(GNUInstallDirs)

# ---------------------------------------------------------------------------
# Assets
# ---------------------------------------------------------------------------
#
# Both consumers currently open assets by path *relative to the working
# directory* — skratch/application.cc does TTF_OpenFontIndex("data/Speedy.fon")
# and load_obj_model("data/ico.obj"). So data/ is installed next to the binary
# rather than under share/, because anything else breaks the moment a firmware
# launcher cd's elsewhere.
#
# The real fix is SDL_GetBasePath(), tracked in the packaging snapshot. Until
# that lands, this layout is load-bearing, not cosmetic.

install(DIRECTORY "${CMAKE_SOURCE_DIR}/data/"
        DESTINATION "${CMAKE_INSTALL_BINDIR}/data"
        COMPONENT runtime)

# ---------------------------------------------------------------------------
# Handheld bundles
# ---------------------------------------------------------------------------

function(wreel_add_handheld_bundle)
    if(NOT WREEL_TARGET_IS_HANDHELD)
        return()
    endif()

    # Reported rather than silently skipped, so it is obvious that `install`
    # produces a prefix tree and not a firmware-ready bundle.
    message(STATUS
        "Handheld target '${WREEL_TARGET_ID}': `install` gives a plain "
        "bin/ + bin/data/ tree.")
    message(STATUS
        "  No firmware bundle is generated yet — layouts differ per firmware "
        "and are unconfirmed.")
    message(STATUS
        "  See planning/2026-07-25-packaging-distribution/README.md")
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
