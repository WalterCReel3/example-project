// coppers::Playlist — track stepping over audio::Music.
//
// Runs against the real .mod files with SDL's dummy audio driver, which
// cmake/Testing.cmake already sets for every test. That is deliberate: the
// interesting behaviour is what happens when a track will not load, and a
// synthetic fixture cannot exercise libxmp's actual accept/reject.
//
// The contract being pinned is that nothing here is fatal. A missing file, an
// unsupported codec, or no audio device at all must leave the demo running in
// silence rather than throwing — the same call audio::Device makes, for the
// same reason: a firmware with no working audio should still show the bars.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <SDL.h>

#include <string>
#include <vector>

#include "../coppers/playlist.hpp"

namespace
{

// SDL_Init is needed for the audio subsystem; the dummy driver comes from the
// test environment.
struct SdlAudio {
    SdlAudio() { SDL_Init(SDL_INIT_AUDIO); }
    ~SdlAudio() { SDL_Quit(); }
};

std::vector<std::string> real_tracks()
{
    std::vector<std::string> tracks;
    tracks.push_back("complications.mod");
    tracks.push_back("complications ii.mod");
    tracks.push_back("her bloody weekend.mod");
    return tracks;
}

} // namespace

TEST_CASE("the three tracks load and step in order, wrapping at the end")
{
    const SdlAudio sdl;
    coppers::Playlist playlist(real_tracks());

    REQUIRE(playlist.size() == 3);

    // Two of the three filenames contain spaces. If path handling ever grows a
    // shell or a naive tokenizer, this is where it breaks.
    const std::string first = playlist.current();
    CHECK(first == "complications.mod");
    CHECK(playlist.playing());

    // playing() is asserted at every step, not just current(). A regression in
    // audio::Music changed the track name correctly and left the mixer silent,
    // and the first version of this test — which checked only the name — passed
    // straight through it. Checking the bookkeeping is not checking the
    // behaviour.
    playlist.next();
    CHECK(playlist.current() == "complications ii.mod");
    CHECK(playlist.playing());

    playlist.next();
    CHECK(playlist.current() == "her bloody weekend.mod");
    CHECK(playlist.playing());

    playlist.next();
    CHECK(playlist.current() == first);
    CHECK(playlist.playing());
}

TEST_CASE("previous steps backwards and wraps at the start")
{
    const SdlAudio sdl;
    coppers::Playlist playlist(real_tracks());

    REQUIRE(playlist.current() == "complications.mod");

    playlist.previous();
    CHECK(playlist.current() == "her bloody weekend.mod");
    CHECK(playlist.playing());

    playlist.previous();
    CHECK(playlist.current() == "complications ii.mod");
    CHECK(playlist.playing());
}

TEST_CASE("a missing track is skipped rather than stopping the music")
{
    const SdlAudio sdl;

    std::vector<std::string> tracks;
    tracks.push_back("complications.mod");
    tracks.push_back("definitely-not-here.mod");
    tracks.push_back("her bloody weekend.mod");

    coppers::Playlist playlist(tracks);
    REQUIRE(playlist.current() == "complications.mod");

    // Stepping onto the missing one must land on the following track, not stop.
    playlist.next();
    CHECK(playlist.current() == "her bloody weekend.mod");
    CHECK(playlist.playing());
}

TEST_CASE("a track that is not a module is skipped too")
{
    const SdlAudio sdl;

    // A real file that libxmp will refuse. Distinct from the missing-file case:
    // one fails in the filesystem and the other inside the decoder, and only
    // the second proves the codec's rejection is handled.
    std::vector<std::string> tracks;
    tracks.push_back("complications.mod");
    tracks.push_back("test.json");

    coppers::Playlist playlist(tracks);
    REQUIRE(playlist.current() == "complications.mod");

    playlist.next();
    CHECK(playlist.current() == "complications.mod");
    CHECK(playlist.playing());
}

TEST_CASE("no playable track at all leaves the demo silent, not broken")
{
    const SdlAudio sdl;

    std::vector<std::string> tracks;
    tracks.push_back("nope.mod");
    tracks.push_back("also-nope.mod");

    // Constructing must not throw, and current() reports nothing playing.
    coppers::Playlist playlist(tracks);
    CHECK(playlist.current().empty());
    CHECK_FALSE(playlist.playing());

    // And stepping a dead playlist is still safe.
    playlist.next();
    playlist.previous();
    CHECK(playlist.current().empty());
}

TEST_CASE("an empty playlist is not a special case for the caller")
{
    const SdlAudio sdl;

    // Braces, not parentheses: `Playlist playlist(std::vector<std::string>())`
    // declares a function returning a Playlist. -Wvexing-parse catches it.
    const std::vector<std::string> nothing;
    coppers::Playlist playlist(nothing);
    CHECK(playlist.size() == 0);
    CHECK(playlist.current().empty());

    playlist.next();
    playlist.previous();
    CHECK(playlist.current().empty());
}
