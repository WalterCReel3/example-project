// gfx::Animation, gfx::AnimatedSprite, and the animation sidecar loader.
//
// Playback is arithmetic over supplied deltas, exactly as rig::FrameTiming is,
// so nearly all of this runs with no video subsystem and no atlas: an Animation
// holds plain indexes and an AnimatedSprite that never draws needs no texture.
// Only the cases that resolve names against a real atlas take a Context.
//
// The awkward parts, and why each has cases here:
//
//   - A delta spanning several frames. rig::FrameTiming clamps a stall to
//     100 ms, which is more than one frame of anything, so "advance by exactly
//     one frame" is the case that never happens on a device.
//   - Loop::Once finishing. A pose that never reports finished() leaves a state
//     machine stuck, and one that reports it a frame early cuts the animation.
//   - Loop::PingPong. The ends are shown once, not twice, and the arithmetic
//     that avoids stepping frame by frame is where that is easy to get wrong.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <gfx/animation.hpp>
#include <gfx/system.hpp>
#include <loaders/animations.hpp>
#include <loaders/image.hpp>
#include <loaders/sparrow.hpp>
#include <posix/errors.hpp>

#include <SDL.h>

#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{

const double fps = 10.0;
const double frame_time = 1.0 / fps;

// An animation over indexes 0..count-1, with no atlas behind it. Playback never
// dereferences the atlas, so this is enough for every timing case.
gfx::Animation make(std::size_t count, gfx::Loop loop)
{
    gfx::Animation animation;
    animation.name = "test";
    animation.seconds_per_frame = frame_time;
    animation.loop = loop;
    for (std::size_t i = 0; i < count; ++i) {
        animation.frames.push_back(i);
    }
    return animation;
}

// The frame numbers a sprite visits over `ticks` advances of one frame time
// each, starting with the frame it shows before any advance.
std::vector<std::size_t> walk(gfx::AnimatedSprite& sprite, int ticks)
{
    std::vector<std::size_t> visited;
    visited.push_back(sprite.frame_number());
    for (int i = 0; i < ticks; ++i) {
        sprite.advance(frame_time);
        visited.push_back(sprite.frame_number());
    }
    return visited;
}

struct VideoFixture {
    VideoFixture()
    {
        SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
        system.reset(new gfx::System());
    }

    std::unique_ptr<gfx::System> system;
};

class ScratchFile
{
public:
    explicit ScratchFile(const std::string& text)
        : path_("test_animation.scratch.xml")
    {
        std::FILE* out = std::fopen(path_.c_str(), "w");
        REQUIRE(out != nullptr);
        std::fwrite(text.data(), 1, text.size(), out);
        std::fclose(out);
    }

    ~ScratchFile() { std::remove(path_.c_str()); }

    ScratchFile(const ScratchFile&) = delete;
    ScratchFile& operator=(const ScratchFile&) = delete;

    const std::string& path() const { return path_; }

private:
    std::string path_;
};

} // namespace

//============================================================================
// Loop modes
//============================================================================

TEST_CASE("Repeat cycles and never finishes")
{
    const gfx::Animation animation = make(3, gfx::Loop::Repeat);
    gfx::AnimatedSprite sprite;
    sprite.play(animation);

    CHECK(walk(sprite, 7) == std::vector<std::size_t>{0, 1, 2, 0, 1, 2, 0, 1});
    CHECK_FALSE(sprite.finished());
}

TEST_CASE("Once holds the last frame and reports finished")
{
    const gfx::Animation animation = make(3, gfx::Loop::Once);
    gfx::AnimatedSprite sprite;
    sprite.play(animation);

    CHECK(walk(sprite, 5) == std::vector<std::size_t>{0, 1, 2, 2, 2, 2});
    CHECK(sprite.finished());
}

TEST_CASE("Once finishes only after the last frame has had its own time")
{
    const gfx::Animation animation = make(3, gfx::Loop::Once);
    gfx::AnimatedSprite sprite;
    sprite.play(animation);

    sprite.advance(frame_time);
    sprite.advance(frame_time);
    CHECK(sprite.frame_number() == 2);

    // On the last frame, but it has only just arrived: an animation cut here
    // would never show its final pose for as long as the others.
    CHECK_FALSE(sprite.finished());

    sprite.advance(frame_time);
    CHECK(sprite.finished());
    CHECK(sprite.frame_number() == 2);
}

TEST_CASE("a one-frame Once animation still completes")
{
    // A single-frame pose. The obvious implementation — step, then test whether
    // there is a next frame — never finishes this, and a caller waiting on it
    // to return to idle waits forever.
    const gfx::Animation animation = make(1, gfx::Loop::Once);
    gfx::AnimatedSprite sprite;
    sprite.play(animation);

    CHECK_FALSE(sprite.finished());
    sprite.advance(frame_time);
    CHECK(sprite.finished());
    CHECK(sprite.frame_number() == 0);
}

TEST_CASE("PingPong shows each end once rather than twice")
{
    const gfx::Animation animation = make(3, gfx::Loop::PingPong);
    gfx::AnimatedSprite sprite;
    sprite.play(animation);

    // 0,1,2,1 and round again — not 0,1,2,2,1,0,0.
    CHECK(walk(sprite, 8) ==
          std::vector<std::size_t>{0, 1, 2, 1, 0, 1, 2, 1, 0});
    CHECK_FALSE(sprite.finished());
}

TEST_CASE("PingPong over two frames alternates")
{
    const gfx::Animation animation = make(2, gfx::Loop::PingPong);
    gfx::AnimatedSprite sprite;
    sprite.play(animation);

    CHECK(walk(sprite, 4) == std::vector<std::size_t>{0, 1, 0, 1, 0});
}

TEST_CASE("a single-frame animation stays put in every mode")
{
    for (gfx::Loop loop : {gfx::Loop::Repeat, gfx::Loop::PingPong}) {
        const gfx::Animation animation = make(1, loop);
        gfx::AnimatedSprite sprite;
        sprite.play(animation);

        CHECK(walk(sprite, 4) == std::vector<std::size_t>{0, 0, 0, 0, 0});
    }
}

//============================================================================
// Deltas that are not one frame
//============================================================================

TEST_CASE("a partial delta does not advance, and partials accumulate")
{
    const gfx::Animation animation = make(3, gfx::Loop::Repeat);
    gfx::AnimatedSprite sprite;
    sprite.play(animation);

    sprite.advance(frame_time * 0.4);
    CHECK(sprite.frame_number() == 0);
    sprite.advance(frame_time * 0.4);
    CHECK(sprite.frame_number() == 0);

    // 1.2 frame times in total.
    sprite.advance(frame_time * 0.4);
    CHECK(sprite.frame_number() == 1);
}

TEST_CASE("a delta spanning several frames lands where stepping would")
{
    // The case that actually happens: rig::FrameTiming clamps a stall to
    // 100 ms, which at 10 fps is a whole animation.
    const gfx::Animation animation = make(3, gfx::Loop::Repeat);

    gfx::AnimatedSprite jumped;
    jumped.play(animation);
    jumped.advance(frame_time * 5.0);

    gfx::AnimatedSprite stepped;
    stepped.play(animation);
    for (int i = 0; i < 5; ++i) {
        stepped.advance(frame_time);
    }

    CHECK(jumped.frame_number() == stepped.frame_number());
    CHECK(jumped.frame_number() == 2);
}

TEST_CASE("a multi-frame delta does not overshoot a Once animation")
{
    const gfx::Animation animation = make(3, gfx::Loop::Once);
    gfx::AnimatedSprite sprite;
    sprite.play(animation);

    sprite.advance(frame_time * 100.0);

    CHECK(sprite.finished());
    CHECK(sprite.frame_number() == 2);
}

TEST_CASE("a multi-frame delta wraps PingPong the same way stepping does")
{
    const gfx::Animation animation = make(4, gfx::Loop::PingPong);

    for (int ticks = 0; ticks < 14; ++ticks) {
        gfx::AnimatedSprite jumped;
        jumped.play(animation);
        jumped.advance(frame_time * ticks);

        gfx::AnimatedSprite stepped;
        stepped.play(animation);
        for (int i = 0; i < ticks; ++i) {
            stepped.advance(frame_time);
        }

        CHECK(jumped.frame_number() == stepped.frame_number());
    }
}

TEST_CASE("an absurd delta terminates rather than stepping through it")
{
    const gfx::Animation animation = make(3, gfx::Loop::Repeat);
    gfx::AnimatedSprite sprite;
    sprite.play(animation);

    // A frame duration near zero against a whole second. Stepping would run
    // this a billion times.
    gfx::Animation fast = animation;
    fast.seconds_per_frame = 1e-9;
    sprite.play(fast);
    sprite.advance(1.0);

    CHECK(sprite.frame_number() < 3);
}

TEST_CASE("zero and negative deltas are ignored")
{
    const gfx::Animation animation = make(3, gfx::Loop::Repeat);
    gfx::AnimatedSprite sprite;
    sprite.play(animation);

    sprite.advance(0.0);
    sprite.advance(-frame_time * 10.0);
    CHECK(sprite.frame_number() == 0);
}

//============================================================================
// State
//============================================================================

TEST_CASE("a default sprite draws nothing and ignores advance")
{
    gfx::AnimatedSprite sprite;

    CHECK_FALSE(sprite.valid());
    CHECK(sprite.frame() == gfx::Atlas::npos);
    CHECK(sprite.animation() == nullptr);

    sprite.advance(1.0);
    CHECK(sprite.frame_number() == 0);
}

TEST_CASE("play restarts, including onto the same animation")
{
    const gfx::Animation animation = make(3, gfx::Loop::Repeat);
    gfx::AnimatedSprite sprite;
    sprite.play(animation);

    sprite.advance(frame_time * 2.0);
    CHECK(sprite.frame_number() == 2);

    sprite.play(animation);
    CHECK(sprite.frame_number() == 0);
}

TEST_CASE("restart clears finished")
{
    const gfx::Animation animation = make(2, gfx::Loop::Once);
    gfx::AnimatedSprite sprite;
    sprite.play(animation);

    sprite.advance(frame_time * 10.0);
    REQUIRE(sprite.finished());

    sprite.restart();
    CHECK_FALSE(sprite.finished());
    CHECK(sprite.frame_number() == 0);
}

TEST_CASE("a sprite is copyable, so it can live in a container")
{
    // The entity store will hold these by value.
    const gfx::Animation animation = make(3, gfx::Loop::Repeat);
    gfx::AnimatedSprite sprite;
    sprite.play(animation);
    sprite.advance(frame_time);

    std::vector<gfx::AnimatedSprite> sprites;
    sprites.push_back(sprite);
    sprites.push_back(gfx::AnimatedSprite());
    sprites[1] = sprite;

    CHECK(sprites[0].frame_number() == 1);
    CHECK(sprites[1].frame_number() == 1);

    // Copies carry their own playback state.
    sprites[0].advance(frame_time);
    CHECK(sprites[0].frame_number() == 2);
    CHECK(sprites[1].frame_number() == 1);
}

TEST_CASE("frame indexes address the atlas, not the sequence")
{
    // The distinction that matters when an animation does not start at atlas
    // index 0, which is every animation but the first.
    gfx::Animation animation;
    animation.seconds_per_frame = frame_time;
    animation.frames = {7, 4, 7};

    gfx::AnimatedSprite sprite;
    sprite.play(animation);

    CHECK(sprite.frame_number() == 0);
    CHECK(sprite.frame() == 7);

    sprite.advance(frame_time);
    CHECK(sprite.frame_number() == 1);
    CHECK(sprite.frame() == 4);

    // A repeated index is how a hold is expressed without per-frame durations.
    sprite.advance(frame_time);
    CHECK(sprite.frame() == 7);
}

//============================================================================
// Animation
//============================================================================

TEST_CASE("duration counts the whole cycle, and PingPong's is longer")
{
    CHECK(make(3, gfx::Loop::Repeat).duration() ==
          doctest::Approx(3 * frame_time));
    CHECK(make(3, gfx::Loop::Once).duration() ==
          doctest::Approx(3 * frame_time));

    // Out and back, ends shown once: four steps for three frames.
    CHECK(make(3, gfx::Loop::PingPong).duration() ==
          doctest::Approx(4 * frame_time));

    CHECK(make(0, gfx::Loop::Repeat).duration() == 0.0);
}

TEST_CASE("loop modes round-trip through their names")
{
    for (gfx::Loop loop :
         {gfx::Loop::Repeat, gfx::Loop::Once, gfx::Loop::PingPong}) {
        CHECK(gfx::loop_from_string(gfx::to_string(loop)) == loop);
    }

    // Unknown falls back rather than throwing; the loader is what reports it.
    CHECK(gfx::loop_from_string("nonsense") == gfx::Loop::Repeat);
    CHECK(gfx::loop_from_string("nonsense", gfx::Loop::Once) ==
          gfx::Loop::Once);

    // Case-sensitive, deliberately: the documents are generated.
    CHECK(gfx::loop_from_string("Repeat") == gfx::Loop::Repeat);
    CHECK(gfx::loop_from_string("PingPong", gfx::Loop::Once) ==
          gfx::Loop::Once);
}

//============================================================================
// The sidecar document
//============================================================================

TEST_CASE("data/foxy.anim.xml parses")
{
    const loaders::AnimationDefs defs =
        loaders::load_animations("data/foxy.anim.xml");

    CHECK(defs.atlas_path == "foxy.xml");
    CHECK(defs.animations.size() == 12);

    bool found_run = false;
    for (const loaders::AnimationDef& def : defs.animations) {
        CHECK(def.fps > 0.0);
        CHECK(def.expected_frames > 0);

        // Generated entries derive their frames from the name.
        CHECK(def.frame_ids.empty());

        if (def.name == "run") {
            found_run = true;
            CHECK(def.expected_frames == 6);
        }
    }
    CHECK(found_run);
}

TEST_CASE("explicit Frame children override derivation")
{
    const ScratchFile file(
        "<Animations atlas=\"foxy.xml\">\n"
        "  <Animation name=\"hop\" fps=\"8\" loop=\"once\">\n"
        "    <Frame id=\"jump.000\"/>\n"
        "    <Frame id=\"jump.001\"/>\n"
        "    <Frame id=\"jump.000\"/>\n"
        "  </Animation>\n"
        "</Animations>\n");

    const loaders::AnimationDefs defs = loaders::load_animations(file.path());
    REQUIRE(defs.animations.size() == 1);

    CHECK(defs.animations[0].name == "hop");
    CHECK(defs.animations[0].fps == doctest::Approx(8.0));
    CHECK(defs.animations[0].loop == gfx::Loop::Once);
    CHECK(defs.animations[0].frame_ids ==
          std::vector<std::string>{"jump.000", "jump.001", "jump.000"});
}

TEST_CASE("a malformed sidecar is rejected")
{
    const ScratchFile no_root("<Nope/>");
    CHECK_THROWS_AS(loaders::load_animations(no_root.path()),
                    loaders::AnimationFormatError);

    const ScratchFile empty("<Animations atlas=\"foxy.xml\"></Animations>");
    CHECK_THROWS_AS(loaders::load_animations(empty.path()),
                    loaders::AnimationFormatError);

    const ScratchFile no_name(
        "<Animations><Animation fps=\"12\"/></Animations>");
    CHECK_THROWS_AS(loaders::load_animations(no_name.path()),
                    loaders::AnimationFormatError);

    // An animation that cannot advance is a stuck sprite, not a slow one.
    const ScratchFile zero_fps(
        "<Animations><Animation name=\"x\" fps=\"0\"/></Animations>");
    CHECK_THROWS_AS(loaders::load_animations(zero_fps.path()),
                    loaders::AnimationFormatError);

    const ScratchFile malformed("<Animations>");
    CHECK_THROWS_AS(loaders::load_animations(malformed.path()),
                    loaders::AnimationFormatError);
}

TEST_CASE("a missing sidecar throws the posix error, not a format error")
{
    CHECK_THROWS_AS(loaders::load_animations("data/does-not-exist.anim.xml"),
                    posix::no_such_file);
}

//============================================================================
// Resolving against the real atlas
//============================================================================

TEST_CASE("data/foxy.anim.xml resolves against data/foxy.xml")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 64, 64, /*fullscreen=*/false);

    const loaders::SparrowAtlas parsed = loaders::load_sparrow("data/foxy.xml");
    SDL_Surface* image = loaders::load_image("data/" + parsed.image_path);
    REQUIRE(image != nullptr);
    gfx::renderer::Texture texture(context, image);
    SDL_FreeSurface(image);
    const gfx::Atlas atlas(std::move(texture), parsed.frames);

    const gfx::AnimationSet set =
        loaders::load_animation_set(atlas, "data/foxy.anim.xml");

    CHECK(set.size() == 12);

    const gfx::AnimationSet::Index run = set.find("run");
    REQUIRE(run != gfx::AnimationSet::npos);
    CHECK(set[run].frames.size() == 6);
    CHECK(set[run].name == "run");

    CHECK(set.find("moonwalk") == gfx::AnimationSet::npos);

    // Every animation resolved to frames that exist, and its indexes address
    // the atlas rather than the sequence.
    for (gfx::AnimationSet::Index i = 0; i < set.size(); ++i) {
        CHECK_FALSE(set[i].empty());
        for (gfx::Atlas::Index frame : set[i].frames) {
            REQUIRE(frame < atlas.size());
            // Derived frames are named after their animation.
            CHECK(atlas[frame].id.compare(0, set[i].name.size() + 1,
                                          set[i].name + ".") == 0);
        }
    }
}

TEST_CASE("animation_from_prefix matches on the whole prefix")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 64, 64, /*fullscreen=*/false);

    const loaders::SparrowAtlas parsed = loaders::load_sparrow("data/foxy.xml");
    SDL_Surface* image = loaders::load_image("data/" + parsed.image_path);
    REQUIRE(image != nullptr);
    gfx::renderer::Texture texture(context, image);
    SDL_FreeSurface(image);
    const gfx::Atlas atlas(std::move(texture), parsed.frames);

    const gfx::Animation run =
        gfx::animation_from_prefix(atlas, "run", 12.0, gfx::Loop::Repeat);
    CHECK(run.frames.size() == 6);
    CHECK(run.seconds_per_frame == doctest::Approx(1.0 / 12.0));

    // Frames arrive in atlas order, which pack_atlas.py guarantees is playback
    // order.
    for (std::size_t i = 0; i + 1 < run.frames.size(); ++i) {
        CHECK(run.frames[i] < run.frames[i + 1]);
    }

    // "hurt" must not swallow "hurt2": the separator is part of the match.
    const gfx::Animation hurt =
        gfx::animation_from_prefix(atlas, "hurt", 12.0, gfx::Loop::Repeat);
    CHECK(hurt.frames.size() == 2);

    const gfx::Animation missing =
        gfx::animation_from_prefix(atlas, "nope", 12.0, gfx::Loop::Repeat);
    CHECK(missing.empty());
}

TEST_CASE("a sidecar naming a frame the atlas lacks is rejected")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 64, 64, /*fullscreen=*/false);

    const loaders::SparrowAtlas parsed = loaders::load_sparrow("data/foxy.xml");
    SDL_Surface* image = loaders::load_image("data/" + parsed.image_path);
    REQUIRE(image != nullptr);
    gfx::renderer::Texture texture(context, image);
    SDL_FreeSurface(image);
    const gfx::Atlas atlas(std::move(texture), parsed.frames);

    const ScratchFile named(
        "<Animations atlas=\"foxy.xml\">\n"
        "  <Animation name=\"hop\"><Frame id=\"nope.000\"/></Animation>\n"
        "</Animations>\n");
    CHECK_THROWS_AS(loaders::load_animation_set(atlas, named.path()),
                    loaders::AnimationFormatError);

    // And one whose name derives to nothing, which is the same drift seen from
    // the other side.
    const ScratchFile derived("<Animations atlas=\"foxy.xml\">\n"
                              "  <Animation name=\"moonwalk\" fps=\"12\"/>\n"
                              "</Animations>\n");
    CHECK_THROWS_AS(loaders::load_animation_set(atlas, derived.path()),
                    loaders::AnimationFormatError);
}
