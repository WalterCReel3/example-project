# orchard — the game demo the game/ layer is shaped around.
#
# Where sprites exercises the atlas path and coppers the raster path, this is
# the one that puts game::Level, game::collide, game::Camera and game::Entities
# together and finds out whether they fit.
#
# It needs no GPU and no gfx::gles2, so it builds on every target including
# miyoomini. The motion in player.cc and actors.cc and the rules in rules.cc are
# deliberately free of gfx and rig, which is what lets tests/test_orchard.cc
# compile them directly and cross the level — under the rules, and reading the
# shipped spawn table — headlessly.

add_executable(orchard
    main.cc
    actors.cc
    cues.cc
    demo.cc
    player.cc
    rules.cc
    verbs.cc)

wreel_set_target_defaults(orchard)

# wreel::audio arrives with cues.cc and with nothing else. Decision 17 keeps the
# cue table here rather than in wreel::game precisely so that this line is the
# demo's and not the shared layer's: `game/`'s charter excludes anything that
# makes sound, and its dependency line reads `game -> gfx -> util -> posix`.
target_link_libraries(orchard PRIVATE
    wreel::game
    wreel::loaders
    wreel::gfx
    wreel::rig
    wreel::audio
    wreel::util
    wreel::posix)

target_compile_definitions(orchard PRIVATE
    WREEL_TARGET_ID="${WREEL_TARGET_ID}"
    WREEL_VERSION="${PROJECT_VERSION}")

set_target_properties(orchard PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${WREEL_RUNTIME_OUTPUT_DIRECTORY}")

# Everything under data/ this demo opens, and nothing else, so a firmware bundle
# carries these rather than all of data/. Has to change whenever the code opens
# a new file — see demo.cc's load_level() and load_actors().
#
# The five WAVs are the whole cue table — orchard::Cue has five enumerators and
# cue_name() is the filename stem, so this list and that enum are the same five
# names. data/sfx.xml is NOT here: it is make_sfx.py's input and no line of the
# game reads it, so a bundle carrying it would carry a file nothing opens.
#
# emerald-droplets.xm is the music decision 6 settled. A bundle without it still
# runs in silence, the same call Speedy.fon gets, but it is 174 KB the demo does
# open and a firmware bundle is what this list exists to fill.
#
# Speedy.fon is the HUD's face, loaded through SDL_ttf's winfnt driver. A bundle
# without it still runs and simply has no score line, which is why it is loaded
# with a warning rather than a throw.
#
# tileset.png is named although no line of code spells it: orchard.tmx
# references sunnyland.tsx, which names the image, and a bundle without it draws
# nothing.
set_property(TARGET orchard PROPERTY WREEL_BUNDLE_ASSETS
    "orchard.tmx"
    "sunnyland.tsx"
    "tileset.png"
    "foxy.png"
    "foxy.xml"
    "foxy.anim.xml"
    "critters.png"
    "critters.xml"
    "critters.anim.xml"
    "Speedy.fon"
    "sfx/jump.wav"
    "sfx/land.wav"
    "sfx/pickup.wav"
    "sfx/bump.wav"
    "sfx/level-complete.wav"
    "emerald-droplets.xm")

install(TARGETS orchard
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
        COMPONENT runtime)
