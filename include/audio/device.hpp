#ifndef WREEL_AUDIO_DEVICE_HPP
#define WREEL_AUDIO_DEVICE_HPP

// Audio output device and mixer lifetime.
//
// Facade over SDL2_mixer: Mix_Music, Mix_Chunk and the Mix_* API do not appear
// in any signature here, so the backend can be replaced without touching
// callers. Same reasoning as util::File over POSIX and util::json over
// nlohmann/json.
//
// COST MODEL, because it is easy to get backwards:
//
//   Codecs compiled in cost binary size, not CPU. SDL2_mixer picks a decoder
//   from the file's contents at load time; one that never sees a matching file
//   never executes. A FLAC-capable build does not slow down a game that only
//   plays WAV.
//
//   Per-frame cost is the mixer profile — rate, channels, voices, buffer. That
//   work happens in every callback regardless of what is playing, and is fixed
//   when the Device is constructed.
//
//   A program that constructs no Device pays nothing at all: the audio
//   subsystem is not initialised until one exists.

#include <string>
#include <vector>

#include <util/nocopy.hpp>

namespace audio
{

// Defaults come from the build (WREEL_AUDIO_* in cmake/ProjectOptions.cmake),
// so a handheld build gets 22050/2048/8 and desktop gets 44100/1024/16 without
// callers knowing which target they are on.
struct Spec {
    int frequency;
    int channels; // 1 mono, 2 stereo
    int buffer;   // samples per callback
    int voices;   // simultaneous sound effect channels

    // Build-configured defaults.
    Spec();
};

// Which decoders this binary was compiled with. Reported by wreel-probe so a
// device can be asked rather than assumed.
std::vector<std::string> compiled_codecs();

class Device
{
public:
    Device();
    explicit Device(const Spec& spec);
    ~Device();

private:
    DISALLOW_COPY_AND_ASSIGN(Device);

public:
    // False when no audio hardware could be opened. This is NOT an error: some
    // handheld firmwares expose no audio device, and a game should stay
    // playable in silence rather than refusing to start. Sound and Music become
    // no-ops.
    bool available() const { return _available; }

    // What SDL actually granted, which may differ from what was requested —
    // devices routinely substitute a different rate or channel count.
    const Spec& actual() const { return _actual; }

    std::string driver_name() const;

    // 0.0 .. 1.0, clamped. Applied across all effect voices.
    void set_sound_volume(float volume);
    void set_music_volume(float volume);

private:
    void _open(const Spec& requested);

    bool _available;
    Spec _actual;
};

} // namespace audio

#endif
