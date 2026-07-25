#include <audio/device.hpp>

#include <SDL.h>
#include <SDL_mixer.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

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

Spec::Spec()
    : frequency(WREEL_AUDIO_RATE)
    , channels(WREEL_AUDIO_CHANNELS)
    , buffer(WREEL_AUDIO_BUFFER)
    , voices(WREEL_AUDIO_VOICES)
{
}

std::vector<std::string> compiled_codecs()
{
    std::vector<std::string> codecs;

    // WAV is unconditional; the rest track the codec tier set at configure
    // time.
    codecs.push_back("wav");
#ifdef WREEL_AUDIO_HAVE_MOD
    codecs.push_back("mod/xm/it/s3m");
#endif
#ifdef WREEL_AUDIO_HAVE_VORBIS
    codecs.push_back("ogg-vorbis");
#endif
#ifdef WREEL_AUDIO_HAVE_MP3
    codecs.push_back("mp3");
#endif
#ifdef WREEL_AUDIO_HAVE_FLAC
    codecs.push_back("flac");
#endif

    return codecs;
}

Device::Device()
    : _available(false)
    , _actual()
{
    _open(Spec());
}

Device::Device(const Spec& spec)
    : _available(false)
    , _actual(spec)
{
    _open(spec);
}

void Device::_open(const Spec& requested)
{
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        // Deliberately not fatal. Some handheld firmwares expose no audio
        // device, and refusing to start would be worse than running silent.
        util::logging.warning()
            << "audio: SDL_INIT_AUDIO unavailable (" << SDL_GetError()
            << "); continuing without sound" << std::endl;
        return;
    }

    if (Mix_OpenAudio(requested.frequency, MIX_DEFAULT_FORMAT,
                      requested.channels, requested.buffer) != 0) {
        util::logging.warning()
            << "audio: Mix_OpenAudio failed (" << Mix_GetError()
            << "); continuing without sound" << std::endl;
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        return;
    }

    // What we asked for and what we got routinely differ; record the truth.
    int frequency = 0;
    int channels = 0;
    Uint16 format = 0;
    if (Mix_QuerySpec(&frequency, &format, &channels) != 0) {
        _actual.frequency = frequency;
        _actual.channels = channels;
    }
    _actual.buffer = requested.buffer;
    _actual.voices = Mix_AllocateChannels(requested.voices);

    _available = true;

    util::logging.info() << "audio: " << _actual.frequency << " Hz, "
                         << _actual.channels << " ch, " << _actual.buffer
                         << " sample buffer, " << _actual.voices
                         << " voices, driver " << driver_name() << std::endl;
}

Device::~Device()
{
    if (!_available) {
        return;
    }

    // Halt before closing: SDL2_mixer will otherwise free chunks out from under
    // a running callback.
    Mix_HaltMusic();
    Mix_HaltChannel(-1);
    Mix_CloseAudio();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

std::string Device::driver_name() const
{
    const char* name = SDL_GetCurrentAudioDriver();
    return name ? name : "(none)";
}

void Device::set_sound_volume(float volume)
{
    if (!_available) {
        return;
    }
    Mix_Volume(-1, clamp_volume(volume));
}

void Device::set_music_volume(float volume)
{
    if (!_available) {
        return;
    }
    Mix_VolumeMusic(clamp_volume(volume));
}

} // namespace audio
