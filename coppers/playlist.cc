#include "playlist.hpp"

#include <exception>

#include <rig/assets.hpp>
#include <util/logging.hpp>

namespace coppers
{

Playlist::Playlist(const std::vector<std::string>& names)
    : _device()
    , _music()
    , _names(names)
    , _index(0)
    , _current()
{
    if (!_device.available()) {
        util::log_warning("music: no audio device, running in silence");
        return;
    }
    if (_names.empty()) {
        util::log_warning("music: no tracks");
        return;
    }

    if (!load_from(0, 1)) {
        util::log_warning("music: none of the %zu tracks could be played",
                          _names.size());
    }
}

Playlist::~Playlist() = default;

bool Playlist::load_from(std::size_t start, int direction)
{
    if (!_device.available() || _names.empty()) {
        return false;
    }

    const std::size_t count = _names.size();

    // Every track is tried before giving up, so one missing file steps past
    // rather than stopping the music for the rest of the run.
    for (std::size_t attempt = 0; attempt < count; ++attempt) {
        const std::size_t index =
            direction >= 0 ? (start + attempt) % count
                           : (start + count - (attempt % count)) % count;

        try {
            // Constructed before the old one is released, so a failure leaves
            // the current track playing rather than dropping into silence.
            std::unique_ptr<audio::Music> next(
                new audio::Music(rig::asset_path(_names[index])));
            next->play();

            _music = std::move(next);
            _index = index;
            _current = _names[index];
            util::log_info("music: playing %s", _current.c_str());
            return true;
        } catch (const std::exception& e) {
            util::log_warning("music: skipping %s: %s", _names[index].c_str(),
                              e.what());
        }
    }

    return false;
}

bool Playlist::playing() const
{
    return _music && _music->playing();
}

void Playlist::next()
{
    if (_names.empty()) {
        return;
    }
    load_from((_index + 1) % _names.size(), 1);
}

void Playlist::previous()
{
    if (_names.empty()) {
        return;
    }
    load_from((_index + _names.size() - 1) % _names.size(), -1);
}

} // namespace coppers
