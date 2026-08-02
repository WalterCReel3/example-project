# wreel-diag — behavioural diagnostics for the SDL2 on the device in hand.
#
# Distinct from wreel-probe, and the difference is the point. The probe asks SDL
# what it offers and prints the answer; this draws known content, reads the
# frame back and reports what actually landed. On a target whose backend
# advertises SDL_RENDERER_TARGETTEXTURE it does not implement, and returns
# success from SDL_RenderClear without clearing, only the second question has a
# trustworthy answer.
#
# Built for every target on purpose. The same binary on desktop-software runs
# against SDL's own software renderer — a correct implementation of the same
# contract — and that report is the control the device run is diffed against.

add_executable(wreel-diag
    main.cc
    report.cc
    readback.cc
    checks.cc
    environment.cc)

wreel_set_target_defaults(wreel-diag)

# Deliberately does NOT link wreel::gfx, for the same reason wreel-probe does
# not: the subject is SDL2's own behaviour. Going through gfx::renderer would
# test our workarounds — Layer composition, the readback path, the output-size
# probing — which are exactly what has to be removed from the picture to see
# what the driver does on its own.
target_link_libraries(wreel-diag PRIVATE
    wreel::util
    wreel::posix
    ${WREEL_SDL2_TARGET})

target_compile_definitions(wreel-diag PRIVATE
    WREEL_TARGET_ID="${WREEL_TARGET_ID}"
    WREEL_VERSION="${PROJECT_VERSION}")

set_target_properties(wreel-diag PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${WREEL_RUNTIME_OUTPUT_DIRECTORY}")

install(TARGETS wreel-diag
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT runtime)
