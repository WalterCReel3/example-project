#include "cues.hpp"

#include <audio/device.hpp>
#include <audio/music.hpp>
#include <audio/sound.hpp>
#include <rig/assets.hpp>
#include <util/logging.hpp>

#include <cstddef>
#include <exception>

namespace orchard
{

namespace
{

// The module decision 6 settled: malmen's `emerald-droplets.xm`, 24 channels.
// Attributed in data/PROVENANCE.md, which is what its licence asks for.
const char* const music_file = "emerald-droplets.xm";

// Under the effects, so a cue is still audible over it. Chosen rather than
// measured: the WAVs carry their own gain from `data/sfx.xml` (0.5 to 0.55) and
// a tracker module at full scale sits on top of that. Not verified by ear.
const float music_volume = 0.6f;

std::string cue_path(Cue which)
{
    return rig::asset_path(std::string("sfx/") + cue_name(which) + ".wav");
}

} // namespace

const char* cue_name(Cue cue)
{
    switch (cue) {
    case Cue::Jump:
        return "jump";
    case Cue::Land:
        return "land";
    case Cue::Pickup:
        return "pickup";
    case Cue::Bump:
        return "bump";
    case Cue::LevelComplete:
        return "level-complete";
    case Cue::Count:
        break;
    }
    return "unknown";
}

Cues::Cues(bool mute)
    : _mute(mute)
{
    if (_mute) {
        util::log_info("orchard: muted, so no audio device is opened at all");
        return;
    }

    // One Device for the process. A second one calls Mix_OpenAudio underneath
    // whatever is already playing, and coppers/greetz.hpp records what that
    // costs — SDL_mixer survives it only while both ask for the same format.
    _device.reset(new audio::Device());

    // Device logs the granted spec itself, so there is nothing to repeat here.

    if (!_device->available()) {
        // Not an error. The chunks below still load — Sound's constructor says
        // so — and every play() is a no-op, so the demo runs in silence.
        util::log_warning("orchard: no audio device, running in silence");
    }

    _sounds.resize(static_cast<std::size_t>(Cue::Count));
    for (std::size_t i = 0; i < _sounds.size(); ++i) {
        const Cue which = static_cast<Cue>(i);
        const std::string path = cue_path(which);
        try {
            _sounds[i].reset(new audio::Sound(path));
        } catch (const std::exception& e) {
            // The slot stays null and that cue is silent for the run.
            util::log_warning("orchard: no %s cue: %s", cue_name(which),
                              e.what());
        }
    }

    try {
        _music.reset(new audio::Music(rig::asset_path(music_file)));
        audio::Music::set_volume(music_volume);
        _music->play();
        util::log_info("orchard: music %s", music_file);
    } catch (const std::exception& e) {
        util::log_warning("orchard: no music: %s", e.what());
    }
}

Cues::~Cues() = default;

void Cues::play(Cue which)
{
    const std::size_t slot = static_cast<std::size_t>(which);
    if (slot >= _sounds.size() || !_sounds[slot]) {
        return;
    }

    // One chunk, one voice, no loop — decision 6's "one chunk per event".
    const int voice = _sounds[slot]->play();
    if (voice < 0) {
        ++_dropped;
        return;
    }

    if (voice + 1 > _peak) {
        _peak = voice + 1;
    }
}

bool Cues::available() const
{
    return _device && _device->available();
}

int Cues::voices() const
{
    return _device ? _device->actual().voices : 0;
}

} // namespace orchard
