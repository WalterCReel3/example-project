# coppers — the gfx::renderer demo, and a measuring instrument.
#
# Unlike skratch this needs no GPU and no gfx::gles2, so it builds on every
# target including miyoomini. That is the reason WREEL_BUILD_DEMOS stopped
# doubling as skratch's renderer requirement: this is the demo the GPU-less
# targets exist for, and the old gating switched it off precisely there.
#
# It links wreel::gfx for the renderer and the layer, and wreel::rig for asset
# paths and frame timing. No wreel::loaders yet — that arrives with the glyph
# sheet in stage 2.
#
# See planning/2026-07-26-coppers-cracktro/.

add_executable(coppers
    main.cc
    demo.cc
    bars.cc
    glyphs.cc
    greetz.cc
    scroller_texture.cc
    scroller_cpu.cc
    playlist.cc)

wreel_set_target_defaults(coppers)

# wreel::loaders arrives with the glyph sheet, and wreel::audio because the
# greetz report the granted mixer spec — the demo asks the device rather than
# reciting a constant.
target_link_libraries(coppers PRIVATE
    wreel::loaders
    wreel::gfx
    wreel::rig
    wreel::audio
    wreel::util
    wreel::posix)

# The scrolling message names the target it was built for, the same string
# wreel-probe reports, so the two agree about which build you are looking at.
target_compile_definitions(coppers PRIVATE
    WREEL_TARGET_ID="${WREEL_TARGET_ID}")

set_target_properties(coppers PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${WREEL_RUNTIME_OUTPUT_DIRECTORY}")

install(TARGETS coppers
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT runtime)
