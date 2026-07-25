#ifndef WREEL_AUDIO_SOUND_HPP
#define WREEL_AUDIO_SOUND_HPP

// A short sound effect, decoded fully into memory at load time.
//
// Use WAV for these. Decoding is free, and on a 128 MB device the RAM cost of
// an uncompressed effect is far cheaper than the CPU cost of decoding a
// compressed one at play time, when it will be triggered dozens of times per
// second.
//
// Streamed background music is audio::Music instead.

#include <string>

#include <util/nocopy.hpp>

struct Mix_Chunk;

namespace audio
{

class Sound
{
public:
    // Throws std::runtime_error if the file is missing or undecodable. Loading
    // succeeds even with no audio device open — playback simply does nothing,
    // so callers do not need to branch on availability.
    explicit Sound(const std::string& path);
    ~Sound();

private:
    DISALLOW_COPY_AND_ASSIGN(Sound);

public:
    // Returns the voice index the sound was assigned, or -1 if every voice is
    // busy or there is no device. Not an error: dropping a sound effect under
    // load is the correct behaviour.
    int play(int loops = 0);

    // 0.0 .. 1.0, clamped. Per-sound, independent of the device master.
    void set_volume(float volume);

    // Duration in seconds, from the decoded length and the device spec.
    double duration() const;

private:
    Mix_Chunk* _chunk;
    std::string _path;
};

} // namespace audio

#endif
