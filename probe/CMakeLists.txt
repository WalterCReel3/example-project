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

# Deliberately does not link wreel::gfx — the probe must build and run on
# targets where no graphics backend is viable yet.
target_link_libraries(wreel-probe PRIVATE
    wreel::util
    wreel::posix
    wreel::audio
    ${WREEL_SDL2_TARGET})

target_compile_definitions(wreel-probe PRIVATE
    WREEL_TARGET_ID="${WREEL_TARGET_ID}"
    WREEL_VERSION="${PROJECT_VERSION}")

# SDL2 is configured with SDL_OPENGL=OFF on GPU-less targets, so GL probing is
# compiled out rather than failing at runtime.
if(WREEL_TARGET_HAS_GPU)
    target_compile_definitions(wreel-probe PRIVATE WREEL_PROBE_GL=1)
endif()

set_target_properties(wreel-probe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${WREEL_RUNTIME_OUTPUT_DIRECTORY}")

install(TARGETS wreel-probe
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT runtime)
