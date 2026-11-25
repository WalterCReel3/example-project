#pragma once

#include <memory>
#include <string>
#include <vector>

namespace audio
{
class Device;
class Music;
class Sound;
} // namespace audio

//============================================================================
//
// orchard's cue table — the five sounds this game has, and when they fire
//
//     orchard::Cues cues(options.mute);
//     cues.play(orchard::Cue::Pickup);
//
// THE TABLE IS ORCHARD'S AND NOT game::Sfx. Decision 17. `game/`'s charter puts
// anything that makes sound on its does-NOT list and puts the rules of any one
// game in that game's executable directory, and `jump`/`land`/`pickup`/`bump`/
// `level-complete` is one game's vocabulary. A `game::Sfx` would also have put
// `wreel::audio` on a dependency line that reads `game -> gfx -> util ->
// posix`.
//
// NO audio TYPE APPEARS IN THIS HEADER, only three forward declarations behind
// unique_ptr. That keeps SDL2_mixer's facade out of every translation unit that
// wants to know what a Cue is — including tests/test_orchard.cc, which uses
// CueEdges below and links no mixer. It is also what makes --mute honest: a
// muted run constructs no Device at all, so there is nothing to turn down.
//
//============================================================================
namespace orchard
{

// Named for the WAV rather than for the rule that fires it, which is a
// deliberate reversal of the three-cue seam this replaced (`Collect`, `Bump`,
// `Victory`).
//
// With five cues and five files, matching names make cue_name() the filename
// stem and the loader a loop, so there is exactly one place that says which
// sounds exist. Naming them for rules instead needs a second table mapping rule
// to file, and two tables that must agree is the shape that silently drifts.
// The cost is that renaming a WAV renames an enumerator — a compiler error,
// which is the cheap kind of wrong.
enum class Cue {
    Jump,
    Land,
    Pickup,
    Bump,
    LevelComplete,

    Count
};

// The stem of the cue's file under `data/sfx/`. Never null.
const char* cue_name(Cue cue);

// Turns the player's per-frame state into the two cues that have no rule behind
// them.
//
// WHY THESE TWO ARE DIFFERENT FROM THE OTHER THREE. `pickup`, `bump` and
// `level-complete` come off Rules, which already answers once per event by
// construction — a pickup is removed on the frame it is taken, a bump is
// latched per animal, the goal latches once. `jump` and `land` have no such
// event to read. Player reports `on_ground()` and `velocity_y()`, and both are
// *states*: a player standing still is on the ground on every one of the sixty
// frames a second it stands there. Firing `land` off that state plays the same
// chunk sixty times a second, which sounds fine on a dev box with 16 voices and
// starves a handheld that has 8 and a 2048-sample buffer. Turning the state
// into an edge is this class's whole job.
//
// A JUMP IS THE ONLY THING THAT MAKES THE VERTICAL VELOCITY TURN UPWARD, which
// is why that — and not "was on the ground and now is not" — is the test.
// Player::step() applies gravity unconditionally, so between launches the
// velocity only ever increases; a ceiling and a floor both set it to zero.
// Reading the ground transition instead would miss every coyote-time jump,
// where the press arrives after the ground has already gone.
//
// Inline, and holding nothing but two numbers, so a headless test can drive it
// beside the real Player without linking a mixer.
class CueEdges
{
public:
    struct Fired {
        bool jump = false;
        bool land = false;
    };

    // Once per frame, with the values Player reports after its step().
    Fired observe(bool on_ground, double velocity_y);

private:
    // Starts true so that a player placed standing on the ground has not landed
    // on it. A spawn resting on the floor reads `on_ground` on its very first
    // frame, and that is a placement rather than an arrival.
    bool _on_ground = true;
    double _velocity_y = 0.0;
};

inline CueEdges::Fired CueEdges::observe(bool on_ground, double velocity_y)
{
    Fired fired;
    fired.jump = _velocity_y >= 0.0 && velocity_y < 0.0;
    fired.land = !_on_ground && on_ground;

    _on_ground = on_ground;
    _velocity_y = velocity_y;
    return fired;
}

// The five chunks and the module, with the device they need.
//
// NOTHING HERE IS FATAL, the call coppers::Playlist makes for the same reason:
// a firmware with no working audio device, a missing WAV or a codec this build
// was not compiled with costs the sound and not the demo. A handheld that
// cannot open its mixer should still show the orchard.
class Cues
{
public:
    // `mute` constructs no Device, loads nothing and plays nothing.
    // include/audio/device.hpp is explicit that a program with no Device does
    // not initialise the audio subsystem at all, so this is silence by not
    // existing rather than by a volume of zero — which is what a timed run and
    // a screenshot run want, since neither is meant to be an audio test.
    explicit Cues(bool mute);

    // Out of line: the unique_ptr members hold incomplete types above.
    ~Cues();

    Cues(const Cues&) = delete;
    Cues& operator=(const Cues&) = delete;

    // One chunk, once. Silently does nothing when muted, when no device opened,
    // or when this cue's file would not load.
    void play(Cue which);

    bool muted() const { return _mute; }

    // Whether a device was opened and is usable.
    bool available() const;

    // Voices the mixer was opened with, or 0 when there is none. The handheld
    // gets 8 and a dev box 16, from WREEL_AUDIO_VOICES.
    int voices() const;

    // The most voices seen busy at once, as a LOWER BOUND rather than a
    // measurement. Sound::play() asks for the first free voice, so a return of
    // index n means voices 0..n-1 were busy and at least n+1 were in flight.
    // Derived from the facade's own return value, which is the only concurrency
    // number audio:: exposes; Mix_Playing() would be reaching past it.
    //
    // ONLY MEANINGFUL FROM A RUN THAT KEEPS REAL TIME. A --screenshot run steps
    // a fixed delta as fast as it can and the mixer drains at wall-clock rate,
    // so its cues pile up against each other and the watermark reads far higher
    // than the same play would reach at 60 fps. --seconds is the one to quote.
    int peak_voices() const { return _peak; }

    // Cues the mixer had no free voice for. The number the Risks section is
    // actually worried about: dropping an effect is correct behaviour, but a
    // run that drops any on a 16-voice desktop will drop a great many more on
    // an 8-voice handheld.
    int dropped() const { return _dropped; }

private:
    bool _mute;

    std::unique_ptr<audio::Device> _device;

    // Indexed by Cue. A slot is null when that file would not load, which is
    // why play() checks rather than assuming five.
    std::vector<std::unique_ptr<audio::Sound>> _sounds;

    std::unique_ptr<audio::Music> _music;

    int _peak = 0;
    int _dropped = 0;
};

} // namespace orchard
