# librig — the services a realtime program needs from its surroundings.
#
# A game, a demo or a live-visuals patch all need the same handful of things
# that are neither drawing, nor sound, nor parsing a file format: where its
# assets are, where it may write, what time it is, and what the user is
# pressing. Those collect here.
#
# WHY THIS IS NOT IN wreel::util. util is generic to any application on any
# platform — tokenizers, number conversion, logging, file I/O — and it links no
# SDL, deliberately. Asset resolution wants SDL_GetBasePath() and frame pacing
# wants a sleep, so putting either there would have made the tokenizer and
# number tests sit downstream of a windowing toolkit. The split is by what the
# code is FOR, not by how generic it happens to look.
#
# THE CHARTER, because a name this broad invites a dumping ground:
#
#   Belongs here    asset and preference paths, frame timing, input mapping,
#                   the loop scaffolding — things a running program needs from
#                   its environment.
#   Does NOT        anything that draws (wreel::gfx), anything that makes sound
#                   (wreel::audio), anything that parses an asset format
#                   (wreel::loaders), and anything with no realtime character
#                   at all (wreel::util).
#
# Dependencies run one way, as everywhere else: rig -> util -> posix.

add_library(wreel_rig STATIC
    assets.cc
    input.cc
    timing.cc)
add_library(wreel::rig ALIAS wreel_rig)

wreel_set_target_defaults(wreel_rig)

target_include_directories(wreel_rig PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

# SDL2 is PUBLIC rather than PRIVATE, unlike SDL2_mixer in wreel::audio. That
# facade exists to keep Mix_* out of consumers; this module has no such job.
# Every consumer of wreel::rig is an SDL program that is already initialising
# SDL and pumping its event loop, so hiding SDL here would buy nothing and cost
# a wrapper per call.
target_link_libraries(wreel_rig
    PUBLIC  wreel::util
            ${WREEL_SDL2_TARGET})

set_target_properties(wreel_rig PROPERTIES
    OUTPUT_NAME rig
    EXPORT_NAME rig)
