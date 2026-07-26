#include <audio/sound.hpp>

#include <SDL_mixer.h>

#include <algorithm>
#include <stdexcept>
#include <string>

#include <audio/device.hpp>
#include <util/logging.hpp>

namespace audio
{

namespace
{

int clamp_volume(float volume)
{
    const float clamped = std::max(0.0f, std::min(1.0f, volume));
    return static_cast<int>(clamped * static_cast<float>(MIX_MAX_VOLUME));
}

} // namespace

Sound::Sound(const std::string& path)
    : _chunk(nullptr)
    , _path(path)
{
    _chunk = Mix_LoadWAV(path.c_str());
    if (!_chunk) {
        // Mix_LoadWAV also fails when no device is open, which is not the
        // caller's mistake. Distinguish the two so the message is useful.
        if (!Mix_QuerySpec(nullptr, nullptr, nullptr)) {
            util::log_warning("audio: no device open, '%s' will be silent",
                              path.c_str());
            return;
        }
        throw std::runtime_error("could not load sound '" + path +
                                 "': " + Mix_GetError());
    }
}

Sound::~Sound()
{
    if (_chunk) {
        Mix_FreeChunk(_chunk);
    }
}

int Sound::play(int loops)
{
    if (!_chunk) {
        return -1;
    }
    // -1 asks for the first free voice. A -1 return means all voices are busy,
    // which under load is the right outcome — dropping an effect beats
    // stalling.
    return Mix_PlayChannel(-1, _chunk, loops);
}

void Sound::set_volume(float volume)
{
    if (!_chunk) {
        return;
    }
    Mix_VolumeChunk(_chunk, clamp_volume(volume));
}

double Sound::duration() const
{
    if (!_chunk) {
        return 0.0;
    }

    int frequency = 0;
    int channels = 0;
    Uint16 format = 0;
    if (!Mix_QuerySpec(&frequency, &format, &channels) || frequency <= 0) {
        return 0.0;
    }

    // SDL_AUDIO_BITSIZE gives bits per sample; alen is total bytes.
    const int bytes_per_sample = SDL_AUDIO_BITSIZE(format) / 8;
    const int frame_bytes = bytes_per_sample * channels;
    if (frame_bytes <= 0) {
        return 0.0;
    }

    const double frames =
        static_cast<double>(_chunk->alen) / static_cast<double>(frame_bytes);
    return frames / static_cast<double>(frequency);
}

} // namespace audio
