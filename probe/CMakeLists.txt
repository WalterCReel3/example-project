# wreel-probe — device capability diagnostic.
#
# Replaces the original project1/ "Lesson 0" SDL/GLEW smoke test, which was
# abandoned tutorial scaffolding: it called SDL_GL_SetAttribute for the context
# version *after* SDL_GL_CreateContext, where it has no effect, and rendered
# nothing.
#
# This is the one executable that builds for every target, and it is the first
# thing worth running on a new device: it reports what the firmware actually
# offers instead of leaving you to infer it from a crash.

add_executable(wreel-probe
    main.cc)

wreel_set_target_defaults(wreel-probe)

# Deliberately does not link wreel::gfx. The probe reports what the *device*
# offers, so it should not depend on our renderers being able to run there — and
# for GL it needs two entry points, which it resolves itself.
target_link_libraries(wreel-probe PRIVATE
    wreel::util
    wreel::posix
    wreel::audio
    ${WREEL_SDL2_TARGET})

target_compile_definitions(wreel-probe PRIVATE
    WREEL_TARGET_ID="${WREEL_TARGET_ID}"
    WREEL_VERSION="${PROJECT_VERSION}")

# One condition, because nothing is linked: report_gl() resolves glGetString and
# glGetIntegerv with SDL_GL_GetProcAddress, so all it needs is an SDL2 built with
# GL/GLES support. That is what WREEL_TARGET_HAS_GPU decides, via
# cmake/Dependencies.cmake.
#
# It used to also require find_package(OpenGL), because the probe called GL
# directly and needed a library to resolve against. The consequence was that the GL
# section was compiled out on precisely the targets it matters most for: no
# libGL/libGLESv2 exists for aarch64 in Debian's cross environment, so rk3326 and
# h700 reported "not compiled in" and could never answer whether a device's blob
# works.
if(WREEL_TARGET_HAS_GPU)
    target_compile_definitions(wreel-probe PRIVATE WREEL_PROBE_GL=1)
endif()

set_target_properties(wreel-probe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${WREEL_RUNTIME_OUTPUT_DIRECTORY}")

install(TARGETS wreel-probe
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT runtime)
