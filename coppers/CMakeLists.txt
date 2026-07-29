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

# The texture scroller is not built for the Miyoo Mini.
#
# Its SDL2's render backend draws every sub-rectangle rotated 180 degrees with
# x mirrored, so that path does not render text there — it renders debris. The
# runtime selector in demo.cc already defaults away from it on that driver, but
# a path that can only ever be wrong should not be in the binary at all: the B
# button cannot select it, and 128 MB of RAM has better uses than code that
# must not run.
#
# Gated on the target rather than on WREEL_TARGET_HAS_GPU, deliberately.
# desktop-software is also GPU-less and its texture path works perfectly — it is
# SDL's own software renderer — and the comparison between the two mechanisms is
# exactly what that preset exists to make. The defect is this device's driver,
# not the absence of a GPU.
set(_coppers_sources
    main.cc
    demo.cc
    bars.cc
    glyphs.cc
    greetz.cc
    scroller_cpu.cc
    playlist.cc)

if(WREEL_TARGET_ID STREQUAL "miyoomini")
    message(STATUS "coppers: texture scroller omitted "
                   "(this target's renderer cannot draw sub-rectangles)")
else()
    list(APPEND _coppers_sources scroller_texture.cc)
endif()

add_executable(coppers ${_coppers_sources})

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

if(NOT WREEL_TARGET_ID STREQUAL "miyoomini")
    target_compile_definitions(coppers PRIVATE WREEL_COPPERS_TEXTURE_SCROLLER=1)
endif()

set_target_properties(coppers PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${WREEL_RUNTIME_OUTPUT_DIRECTORY}")

# Everything under data/ this demo opens, and nothing else — a firmware bundle
# carries exactly this list rather than all twenty files in data/. Kept here
# because it has to change whenever the code below opens a new file: the glyph
# sheet in demo.cc, the HUD font in demo.cc, and the playlist in playlist.cc.
set_property(TARGET coppers PROPERTY WREEL_BUNDLE_ASSETS
    "glyphs-16x16.png"
    "Speedy.fon"
    "complications.mod"
    "complications ii.mod"
    "her bloody weekend.mod")

install(TARGETS coppers
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT runtime)
