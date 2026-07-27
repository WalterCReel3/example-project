# skratch — the demo application, and the worked example of how GL is structured
# now versus in 2016.
#
# Needs gfx::gles2, so ProjectOptions.cmake leaves WREEL_BUILD_SKRATCH off on a
# target with no GPU rather than failing the configure — and, since 2026-07-27,
# without disabling the other demos along with it. It contains no gl* calls of its
# own any more: everything goes through gfx::gles2, which is what lets the demo
# follow the renderer instead of being welded to one pipeline.

add_executable(skratch
    main.cc
    application.cc
    input.cc)

wreel_set_target_defaults(skratch)

target_link_libraries(skratch PRIVATE
    wreel::loaders
    wreel::gfx
    wreel::rig
    wreel::util
    wreel::posix)

# Assets resolve through rig::asset_path() rather than by working directory, so
# this no longer has to be launched from a directory containing data/. The log
# goes under rig::pref_path().
set_target_properties(skratch PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${WREEL_RUNTIME_OUTPUT_DIRECTORY}")

install(TARGETS skratch
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT runtime)
