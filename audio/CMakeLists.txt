# libaudio — sound effects and music.
#
# A base requirement, so this is unconditional: every target builds it. Depends
# on wreel::util for logging only, NOT on wreel::gfx — audio must work on a
# target whose graphics backend is not viable yet, and the probe tool links audio
# without linking gfx.

add_library(wreel_audio STATIC
    device.cc
    sound.cc
    music.cc)
add_library(wreel::audio ALIAS wreel_audio)

wreel_set_target_defaults(wreel_audio)

target_include_directories(wreel_audio PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

# SDL2_mixer is PRIVATE: the facade exists precisely so Mix_* does not leak into
# consumers. The two forward-declared Mix_Chunk/Mix_Music pointers in the public
# headers are opaque and need no SDL2_mixer include.
target_link_libraries(wreel_audio
    PUBLIC  wreel::util
            ${WREEL_SDL2_TARGET}
    PRIVATE ${WREEL_SDL2_MIXER_TARGET})

# Mixer profile baked in as defaults for audio::Spec, so callers never need to
# know which target they were built for.
target_compile_definitions(wreel_audio PUBLIC
    WREEL_AUDIO_RATE=${WREEL_AUDIO_RATE}
    WREEL_AUDIO_CHANNELS=${WREEL_AUDIO_CHANNELS}
    WREEL_AUDIO_BUFFER=${WREEL_AUDIO_BUFFER}
    WREEL_AUDIO_VOICES=${WREEL_AUDIO_VOICES}
    WREEL_AUDIO_CODEC_TIER="${WREEL_AUDIO_CODECS}")

# Lets compiled_codecs() report the truth instead of a guess. Kept in step with
# the tier logic in cmake/Dependencies.cmake.
target_compile_definitions(wreel_audio PUBLIC WREEL_AUDIO_HAVE_MOD=1)

if(NOT WREEL_AUDIO_CODECS STREQUAL "minimal")
    target_compile_definitions(wreel_audio PUBLIC WREEL_AUDIO_HAVE_VORBIS=1)
endif()

if(WREEL_AUDIO_CODECS STREQUAL "full")
    target_compile_definitions(wreel_audio PUBLIC
        WREEL_AUDIO_HAVE_MP3=1
        WREEL_AUDIO_HAVE_FLAC=1)
endif()

set_target_properties(wreel_audio PROPERTIES
    OUTPUT_NAME audio
    EXPORT_NAME audio)
