# sprites — the 2D game-path demo.
#
# Where coppers exercises the raster/layer path, this exercises the atlas path:
# gfx::Atlas, gfx::AnimatedSprite and loaders::load_sparrow, all through
# gfx::renderer.
#
# Like coppers it needs no GPU and no gfx::gles2, so it builds on every target
# including miyoomini. That matters beyond tidiness: the fill-rate risk in
# planning/2026-07-25-software-2d-sprites-tiling/ is about per-sprite blitting
# on two Cortex-A7 cores, and this is the binary that will answer it.

add_executable(sprites
    main.cc
    demo.cc
    entities.cc)

wreel_set_target_defaults(sprites)

target_link_libraries(sprites PRIVATE
    wreel::loaders
    wreel::gfx
    wreel::rig
    wreel::util
    wreel::posix)

set_target_properties(sprites PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${WREEL_RUNTIME_OUTPUT_DIRECTORY}")

# Everything under data/ this demo opens, and nothing else, so a firmware bundle
# carries these three rather than all of data/. Has to change whenever the code
# opens a new file — see demo.cc's load().
set_property(TARGET sprites PROPERTY WREEL_BUNDLE_ASSETS
    "foxy.png"
    "foxy.xml"
    "foxy.anim.xml"
    "tileset.png"
    "sunnyland.tmx"
    "sunnyland.tsx")

install(TARGETS sprites
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT runtime)
