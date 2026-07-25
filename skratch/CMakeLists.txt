# skratch — the demo application.
#
# Only buildable under the gl_legacy backend: application.cc calls glClear,
# glRotatef, glTranslatef and glLoadIdentity directly, so it is welded to the
# fixed-function pipeline. ProjectOptions.cmake disables WREEL_BUILD_DEMOS
# automatically for other backends rather than failing the configure.

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

# main.cc writes runlog.txt and loads data/ico.obj by relative path, so it must
# run from a directory containing data/.
set_target_properties(skratch PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${WREEL_RUNTIME_OUTPUT_DIRECTORY}")

install(TARGETS skratch
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT runtime)
