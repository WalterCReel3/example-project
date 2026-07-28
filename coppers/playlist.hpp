#pragma once

#include <memory>
#include <string>
#include <vector>

#include <audio/device.hpp>
#include <audio/music.hpp>

//============================================================================
//
// The tracker playlist
//
// Three .mod files stepped with the D-pad. Tracker formats rather than anything
// decoded because a song is tens of kilobytes and playback is sample mixing —
// see docs/TARGETS.md § "Prefer tracker formats for music". On a 128 MB device
// sharing RAM with the OS, that is not a stylistic choice.
//
// NOTHING HERE IS FATAL. A missing file, an unsupported codec or a device with
// no audio output at all leaves the demo running in silence rather than
// refusing to start, which is the same call audio::Device makes and for the
// same reason: a firmware with no working audio should still show the bars.
// So the constructor does not throw and next()/prev() skip what will not load.
//
//============================================================================
namespace coppers
{

class Playlist
{
public:
    // `names` are asset-relative, as passed to rig::asset_path(). Starts
    // playing the first track that loads.
    explicit Playlist(const std::vector<std::string>& names);
    ~Playlist();

    Playlist(const Playlist&) = delete;
    Playlist& operator=(const Playlist&) = delete;

    void next();
    void previous();

    // Empty when nothing is playing — no tracks loaded, or no audio device.
    const std::string& current() const { return _current; }

    // Whether the mixer is actually playing. Distinct from current() being
    // non-empty, which only says which track was last loaded: a regression in
    // 2026-07-27 changed tracks correctly and left the mixer silent, and a test
    // that checked only the name passed straight through it.
    bool playing() const;

    bool audio_available() const { return _device.available(); }

    // Exposed so the scrolling message can report the granted mixer spec
    // without constructing a second Device — see coppers/greetz.hpp for why
    // that matters.
    const audio::Device& device() const { return _device; }
    std::size_t size() const { return _names.size(); }

private:
    // Tries `_index`, then each following track, until one loads or all have
    // been tried. Returns false when none will.
    bool load_from(std::size_t start, int direction);

    audio::Device _device;
    std::unique_ptr<audio::Music> _music;
    std::vector<std::string> _names;
    std::size_t _index;
    std::string _current;
};

} // namespace coppers
