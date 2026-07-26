# skratch — the demo application, and the worked example of how GL is structured
# now versus in 2016.
#
# Needs gfx::gles2, so ProjectOptions.cmake disables WREEL_BUILD_DEMOS on a target
# with no GPU rather than failing the configure. It contains no gl* calls of its
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
    wreel::util
    wreel::posix)

# Loads data/ico.obj and data/Speedy.fon by relative path, so it must run from a
# directory containing data/. The log no longer lands in the working directory —
# main.cc writes it under SDL_GetPrefPath.
set_target_properties(skratch PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${WREEL_RUNTIME_OUTPUT_DIRECTORY}")

install(TARGETS skratch
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT runtime)
