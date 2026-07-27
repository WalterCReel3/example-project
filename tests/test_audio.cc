// audio::Device, Sound and Music.
//
// Runs headless: cmake/Testing.cmake sets SDL_AUDIODRIVER=dummy so these are
// deterministic in CI and under qemu rather than depending on whether the
// machine has a sound card.
//
// The WAV fixture is synthesised at runtime rather than committed, so the test
// is self-contained and still exercises real decoding.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <audio/device.hpp>
#include <audio/music.hpp>
#include <audio/sound.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace
{

void put_u32(std::vector<char>& out, std::uint32_t v)
{
    out.push_back(static_cast<char>(v & 0xff));
    out.push_back(static_cast<char>((v >> 8) & 0xff));
    out.push_back(static_cast<char>((v >> 16) & 0xff));
    out.push_back(static_cast<char>((v >> 24) & 0xff));
}

void put_u16(std::vector<char>& out, std::uint16_t v)
{
    out.push_back(static_cast<char>(v & 0xff));
    out.push_back(static_cast<char>((v >> 8) & 0xff));
}

// Minimal 16-bit mono PCM RIFF/WAVE file: a quiet sine, written to `path`.
// duration_ms is kept small — this is decode-path exercise, not audio quality.
std::string write_test_wav(const std::string& path, int duration_ms = 100,
                           int rate = 22050)
{
    const int frames = (rate * duration_ms) / 1000;
    const std::uint32_t data_bytes = static_cast<std::uint32_t>(frames * 2);

    std::vector<char> wav;
    const char* riff = "RIFF";
    wav.insert(wav.end(), riff, riff + 4);
    put_u32(wav, 36 + data_bytes); // total size - 8
    const char* wave_fmt = "WAVEfmt ";
    wav.insert(wav.end(), wave_fmt, wave_fmt + 8);
    put_u32(wav, 16);                                   // fmt chunk size
    put_u16(wav, 1);                                    // PCM
    put_u16(wav, 1);                                    // mono
    put_u32(wav, static_cast<std::uint32_t>(rate));     // sample rate
    put_u32(wav, static_cast<std::uint32_t>(rate * 2)); // byte rate
    put_u16(wav, 2);                                    // block align
    put_u16(wav, 16);                                   // bits per sample
    const char* data = "data";
    wav.insert(wav.end(), data, data + 4);
    put_u32(wav, data_bytes);

    for (int i = 0; i < frames; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(rate);
        const double s = std::sin(t * 440.0 * 2.0 * 3.14159265358979);
        put_u16(wav, static_cast<std::uint16_t>(
                         static_cast<std::int16_t>(s * 8000.0)));
    }

    std::ofstream out(path, std::ios::binary);
    out.write(wav.data(), static_cast<std::streamsize>(wav.size()));
    out.close();
    return path;
}

} // namespace

TEST_CASE("Spec picks up the build-configured mixer profile")
{
    const audio::Spec spec;

    // Exact values are per-target, so assert they are sane rather than equal to
    // one target's numbers.
    CHECK(spec.frequency >= 8000);
    CHECK(spec.frequency <= 48000);
    CHECK((spec.channels == 1 || spec.channels == 2));
    CHECK(spec.buffer >= 256);
    CHECK(spec.voices >= 1);
}

TEST_CASE("compiled_codecs reports at least WAV and tracker support")
{
    const std::vector<std::string> codecs = audio::compiled_codecs();

    REQUIRE(!codecs.empty());
    CHECK(std::find(codecs.begin(), codecs.end(), "wav") != codecs.end());
    CHECK(std::find(codecs.begin(), codecs.end(), "mod/xm/it/s3m") !=
          codecs.end());
}

TEST_CASE("Device opens against the dummy driver and reports what it got")
{
    audio::Device device;

    REQUIRE(device.available());
    CHECK(device.actual().frequency > 0);
    CHECK(device.actual().voices >= 1);
    CHECK(device.driver_name() == "dummy");
}

TEST_CASE("volume setters clamp without throwing")
{
    audio::Device device;

    CHECK_NOTHROW(device.set_sound_volume(-5.0f));
    CHECK_NOTHROW(device.set_sound_volume(0.5f));
    CHECK_NOTHROW(device.set_sound_volume(99.0f));
    CHECK_NOTHROW(device.set_music_volume(0.0f));
}

TEST_CASE("a WAV loads, reports a plausible duration, and plays")
{
    audio::Device device;
    REQUIRE(device.available());

    const std::string path = write_test_wav("test_tone.wav", 100);

    {
        audio::Sound sound(path);

        // 100 ms, allowing for resampling to the device rate.
        CHECK(sound.duration() > 0.05);
        CHECK(sound.duration() < 0.20);

        CHECK(sound.play() >= 0); // a voice was assigned
        CHECK_NOTHROW(sound.set_volume(0.25f));
    }

    std::remove(path.c_str());
}

TEST_CASE("loading a missing sound throws")
{
    audio::Device device;
    REQUIRE(device.available());

    CHECK_THROWS_AS(audio::Sound("data/definitely-not-here.wav"),
                    std::runtime_error);
}

TEST_CASE("loading missing music throws with a codec hint")
{
    audio::Device device;
    REQUIRE(device.available());

    bool threw = false;
    try {
        audio::Music music("data/definitely-not-here.ogg");
    } catch (const std::runtime_error& e) {
        threw = true;
        // The message should name what this build can decode, so an
        // unsupported-format failure is actionable rather than opaque.
        const std::string what = e.what();
        CHECK(what.find("WREEL_AUDIO_CODECS") != std::string::npos);
    }
    CHECK(threw);
}

TEST_CASE("releasing a replaced track does not stop the one that replaced it")
{
    // Regression, 2026-07-27. ~Music guarded its halt on Mix_PlayingMusic(),
    // which answers "is ANY music playing" — SDL_mixer has one music channel
    // and offers no way to ask which Mix_Music owns it. So destroying a track
    // halted whatever was current, which need not have been that track.
    //
    // Unnoticed until coppers::Playlist became the first consumer to hold two
    // Music objects at once. Changing track constructs the successor, starts
    // it, then releases the predecessor — whose destructor stopped the
    // successor a moment after it began. Symptom on screen: the song name
    // changed and the music went silent.
    //
    // Mix_FreeMusic already halts, but only when the music being freed is the
    // one playing, and under Mix_LockAudio(). The fix was to delete our halt
    // and let it do the targeted thing.
    audio::Device device;
    REQUIRE(device.available());

    std::unique_ptr<audio::Music> current(
        new audio::Music("data/complications.mod"));
    current->play();
    REQUIRE(current->playing());

    // Exactly the Playlist ordering: the successor is constructed and started
    // while the predecessor is still alive, so a failed load would leave the
    // current track playing rather than dropping into silence.
    std::unique_ptr<audio::Music> replacement(
        new audio::Music("data/complications ii.mod"));
    replacement->play();
    REQUIRE(replacement->playing());

    current.reset();

    // The assertion that used to fail. Releasing the superseded track must not
    // touch the mixer's current music.
    CHECK(replacement->playing());
}

TEST_CASE("Device is destroyed cleanly while a sound is still playing")
{
    const std::string path = write_test_wav("test_tone_loop.wav", 100);

    {
        audio::Device device;
        REQUIRE(device.available());

        audio::Sound sound(path);
        sound.play(-1); // loop forever; destructor must halt it
    }

    // Reopening afterwards proves the subsystem was released rather than
    // leaked.
    {
        audio::Device again;
        CHECK(again.available());
    }

    std::remove(path.c_str());
}
