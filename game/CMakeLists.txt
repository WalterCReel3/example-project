# libgame — the rules layer that sits between the engine modules and a demo.
#
# gfx draws, rig tells a program about its surroundings, loaders parse. None of
# them know what a level is, what is solid, or what the camera should be looking
# at. That knowledge is what a game is, and it collects here so that more than
# one executable can share it — which is the trigger this module was created on,
# rather than a guess that a second consumer would eventually appear.
#
# THE CHARTER, because a name this broad invites a dumping ground:
#
#   Belongs here    the entity store, collision against a map and against other
#                   entities, camera framing, input actions, level spawn tables
#                   — simulation state and the rules over it.
#   Does NOT        anything that draws (wreel::gfx), anything that makes sound
#                   (wreel::audio), anything that parses an asset format
#                   (wreel::loaders), anything that ASKS THE ENVIRONMENT what is
#                   attached or what the user is pressing (wreel::rig), and the
#                   rules of any ONE game, which live in that game's executable
#                   directory.
#
# The last of those is the line that matters. A demo's own rules — what its
# pickups are worth, when it is over — belong to the demo. What lands here is
# what a second demo would otherwise copy.
#
# THE rig CLAUSE EXCLUDES A JOB, NOT A NAME, and it has to: "input actions"
# three lines above is in the belongs-here list, so a clause reading "anything
# about the program's environment (wreel::rig)" would contradict it. The
# distinction that resolves them is job versus vocabulary. Opening a device,
# enumerating its buttons and pumping SDL's event queue is rig's job and is
# excluded — nothing here does any of it. include/game/action.hpp names
# rig::Button, which is a plain enum in a header, and maps game actions onto
# it; its query surface is a template over a documented source concept and
# never mentions rig::Pad. A module may speak a neighbour's vocabulary without
# doing its work.
#
# Dependencies run one way, as everywhere else:
# game -> {gfx, rig} -> util -> posix.

add_library(wreel_game STATIC
    action.cc
    camera.cc
    collide.cc
    entities.cc
    level.cc)
add_library(wreel::game ALIAS wreel_game)

wreel_set_target_defaults(wreel_game)

target_include_directories(wreel_game PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

# gfx is PUBLIC because it is in this module's interface, not merely its
# implementation: Entity holds a gfx::AnimatedSprite by value and draw() takes a
# gfx::renderer::Context&, so every consumer compiles those headers anyway.
#
# rig is PUBLIC for the same reason and for that reason only: include/game/
# action.hpp includes <rig/input.hpp> for the rig::Button enum, so every
# consumer compiles that header. No symbol from wreel::rig is referenced — the
# enum is a header-only vocabulary and nothing here calls rig::button_name or
# constructs a rig::Pad — so this pulls no object out of the archive. It is
# named anyway rather than left to the shared include directory, because a
# cross-module include that works only because two targets happen to share an
# include path is coupling nobody wrote down.
target_link_libraries(wreel_game
    PUBLIC  wreel::gfx
            wreel::rig
            wreel::util)

set_target_properties(wreel_game PROPERTIES
    OUTPUT_NAME game
    EXPORT_NAME game)
