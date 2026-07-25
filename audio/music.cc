#include <audio/music.hpp>

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

// Turns "unsupported format" into something a developer can act on, by naming
// what this build can actually decode.
std::string codec_hint()
{
    std::string hint = " (this build decodes:";
    for (const std::string& codec : compiled_codecs()) {
        hint += " " + codec;
    }
    hint += "; see WREEL_AUDIO_CODECS)";
    return hint;
}

} // namespace

Music::Music(const std::string& path)
    : _music(nullptr)
    , _path(path)
{
    _music = Mix_LoadMUS(path.c_str());
    if (!_music) {
        if (!Mix_QuerySpec(nullptr, nullptr, nullptr)) {
            util::logging.warning() << "audio: no device open, '" << path
                                    << "' will be silent" << std::endl;
            return;
        }
        throw std::runtime_error("could not load music '" + path +
                                 "': " + Mix_GetError() + codec_hint());
    }
}

Music::~Music()
{
    if (_music) {
        // Halting first avoids freeing a track the mixer callback is reading.
        if (Mix_PlayingMusic()) {
            Mix_HaltMusic();
        }
        Mix_FreeMusic(_music);
    }
}

void Music::play(int loops)
{
    if (!_music) {
        return;
    }
    if (Mix_PlayMusic(_music, loops) != 0) {
        util::logging.error() << "audio: could not play '" << _path
                              << "': " << Mix_GetError() << std::endl;
    }
}

void Music::stop()
{
    Mix_HaltMusic();
}

void Music::pause()
{
    Mix_PauseMusic();
}

void Music::resume()
{
    Mix_ResumeMusic();
}

bool Music::playing() const
{
    return _music && Mix_PlayingMusic() != 0;
}

bool Music::paused() const
{
    return Mix_PausedMusic() != 0;
}

double Music::position() const
{
    if (!_music) {
        return -1.0;
    }
    // Mix_GetMusicPosition returns seconds, or -1 for formats that cannot
    // report. Notably this is NOT tracker row/pattern — see music.hpp.
    return Mix_GetMusicPosition(_music);
}

double Music::duration() const
{
    if (!_music) {
        return -1.0;
    }
    return Mix_MusicDuration(_music);
}

void Music::set_volume(float volume)
{
    Mix_VolumeMusic(clamp_volume(volume));
}

} // namespace audio
