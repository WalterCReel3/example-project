# Install layout and per-target packaging.
#
# The original build installed to an in-source dist/ with binaries under bin/.
# That shape is kept — it's convenient for copying onto an SD card — but the
# hard-coded CMAKE_INSTALL_PREFIX override is gone, since it broke any attempt
# to install elsewhere.

include_guard(GLOBAL)
include(GNUInstallDirs)

# Assets the runtime actually opens. The demo loads data/ico.obj and
# data/Speedy.fon by relative path, so data/ ships next to the binary.
install(DIRECTORY "${CMAKE_SOURCE_DIR}/data/"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/wreel/data"
        COMPONENT runtime)

# ---------------------------------------------------------------------------
# Handheld bundle
# ---------------------------------------------------------------------------
#
# Handheld firmwares (OnionOS, muOS, ArkOS, ROCKNIX) launch apps from a
# self-contained directory with a small shell entrypoint, rather than from a
# system prefix. This produces that shape:
#
#     pkg/<target>/
#       launch.sh
#       bin/...
#       data/...

function(wreel_add_handheld_bundle)
    if(NOT WREEL_TARGET_IS_HANDHELD)
        return()
    endif()

    set(_bundle "${CMAKE_BINARY_DIR}/bundle")

    file(WRITE "${_bundle}/launch.sh"
"#!/bin/sh
# Entry point for handheld firmware launchers.
DIR=\"$(dirname \"$0\")\"
cd \"$DIR\" || exit 1
export LD_LIBRARY_PATH=\"$DIR/lib:$LD_LIBRARY_PATH\"
exec ./bin/wreel-probe \"$@\"
")

    install(PROGRAMS "${_bundle}/launch.sh"
            DESTINATION "."
            COMPONENT handheld)

    message(STATUS "Handheld bundle enabled for '${WREEL_TARGET_ID}'")
endfunction()

# ---------------------------------------------------------------------------
# CPack
# ---------------------------------------------------------------------------

set(CPACK_PACKAGE_NAME "wreel-${WREEL_TARGET_ID}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_FILE_NAME
    "wreel-${PROJECT_VERSION}-${WREEL_TARGET_ID}")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_SOURCE_DIR}/pkg")
set(CPACK_GENERATOR "TGZ")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_VERBATIM_VARIABLES ON)

include(CPack)
