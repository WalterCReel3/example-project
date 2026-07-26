#ifndef WREEL_AUDIO_MUSIC_HPP
#define WREEL_AUDIO_MUSIC_HPP

// Streamed background music. One track plays at a time, which is SDL2_mixer's
// model rather than a limitation we chose.
//
// Prefer tracker formats (.mod .xm .it .s3m) on handheld targets. A song is
// tens of kilobytes instead of megabytes, and playback is sample mixing rather
// than transform decoding — which matters when 128 MB is the whole budget and
// two Cortex-A7 cores are also rasterising.
//
// Availability depends on the build's codec tier; ask audio::compiled_codecs()
// rather than assuming.

#include <string>

#include <util/nocopy.hpp>

// Must match SDL_mixer.h exactly: it declares `typedef struct Mix_Music
// Mix_Music;` — the struct tag is Mix_Music. Getting this wrong is a hard error
// in any TU that also includes SDL_mixer.h, which is the same trap
// include/gfx/renderer/context.hpp documents for TTF_Font.
//
// (Mix_Chunk needs no such care: SDL_mixer.h defines it as a complete struct,
// so audio/sound.hpp can simply forward-declare `struct Mix_Chunk;`.)
typedef struct Mix_Music Mix_Music;

namespace audio
{

class Music
{
public:
    // Throws std::runtime_error if the file is missing, or if its format was
    // not compiled into this build — the message names the codec tier so the
    // failure is actionable rather than a bare "unsupported".
    explicit Music(const std::string& path);
    ~Music();

private:
    DISALLOW_COPY_AND_ASSIGN(Music);

public:
    // loops = -1 repeats forever, which is the usual case for game music.
    void play(int loops = -1);
    void stop();
    void pause();
    void resume();

    bool playing() const;
    bool paused() const;

    // Playback position in seconds, or -1.0 if the format cannot report it.
    //
    // NOTE for visual sync: seconds is all SDL2_mixer exposes. libxmp can
    // report tracker row/pattern/tick, which would give sample-accurate beat
    // sync, but the mixer does not surface it. Row-level sync needs libxmp
    // driven directly. See planning/2026-07-25-midi-live-visuals/.
    double position() const;

    // Total duration in seconds, or -1.0 if unknown.
    double duration() const;

    // 0.0 .. 1.0, clamped. Music volume is global in SDL2_mixer, so this and
    // Device::set_music_volume() are the same underlying control.
    static void set_volume(float volume);

private:
    Mix_Music* _music;
    std::string _path;
};

} // namespace audio

#endif
