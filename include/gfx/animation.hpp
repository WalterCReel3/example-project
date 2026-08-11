#pragma once

#include <string>
#include <vector>

#include <gfx/atlas.hpp>

//============================================================================
//
// Animation: a frame sequence over an Atlas, and something playing it
//
//     const gfx::Animation run =
//         gfx::animation_from_prefix(atlas, "run", 12.0, gfx::Loop::Repeat);
//
//     gfx::AnimatedSprite foxy(atlas, run);
//     foxy.advance(clock.delta());
//     foxy.draw(context, x, y);
//
// Two types, split by what is shared. An Animation is a *definition* — which
// frames, how fast, whether it loops — and ten entities running the same one
// share a single copy. An AnimatedSprite is *playback state*, one per entity:
// where it is in the sequence and how long the current frame has been up.
//
// Frames are an explicit list of indexes rather than a first-and-count range,
// which costs a few bytes and buys two things. Reuse: `idle` can be
// 0,1,2,1 without the atlas holding a duplicate. And a hold: repeating an index
// keeps that frame up for two ticks, which is how variable timing is expressed
// without a duration per frame.
//
// Indexes, not names, because resolution happens once. A string compare per
// sprite per frame is affordable for a dozen sprites on a dev box and not for a
// screen of them on two Cortex-A7 cores.
//
// See planning/2026-07-25-software-2d-sprites-tiling/.
//
//============================================================================
namespace gfx
{

enum class Loop {
    Repeat,  // 0,1,2,0,1,2,...
    Once,    // 0,1,2,2,2,... and finished() goes true
    PingPong // 0,1,2,1,0,1,2,... — the ends are not repeated
};

// Parses "repeat", "once" or "pingpong", case-sensitively. Returns `fallback`
// for anything else, since an unknown mode in an asset should not stop a demo
// starting; the loader is what reports it.
Loop loop_from_string(const std::string& text, Loop fallback = Loop::Repeat);
const char* to_string(Loop loop);

// A definition. An aggregate: there is no invariant a constructor could hold
// that the atlas it refers to could not invalidate anyway.
struct Animation {
    std::string name;

    // Indexes into the Atlas this was built against. Playing an Animation
    // against a different atlas is undefined in the ordinary way — the indexes
    // mean nothing there.
    std::vector<Atlas::Index> frames;

    // Seconds one frame is held. Zero or negative means "do not advance", which
    // is what a single-frame pose wants and what stops a division by zero
    // reaching anything downstream.
    double seconds_per_frame = 1.0 / 12.0;

    Loop loop = Loop::Repeat;

    bool empty() const { return frames.empty(); }
    std::size_t size() const { return frames.size(); }

    // Seconds for one pass. Zero for an empty or held animation.
    double duration() const;
};

// Builds an Animation from every frame in `atlas` named "<prefix>.NNN", in
// index order. tools/pack_atlas.py guarantees those are contiguous and in
// playback order, which is the convention data/jetpackdude.xml already used.
//
// Returns an Animation with no frames if the prefix matches nothing; the caller
// decides whether that is an error, because a demo missing one pose should
// still start.
Animation animation_from_prefix(const Atlas& atlas, const std::string& prefix,
                                double fps, Loop loop);

// A named set of definitions. Mirrors Atlas deliberately: resolve a name to an
// Index once, then use the Index.
class AnimationSet
{
public:
    typedef std::vector<Animation> Animations;
    typedef Animations::size_type Index;

    static constexpr Index npos = static_cast<Index>(-1);

    void add(Animation animation);

    Index size() const { return _animations.size(); }
    bool empty() const { return _animations.empty(); }

    const Animation& operator[](Index index) const
    {
        return _animations[index];
    }
    const Animations& animations() const { return _animations; }

    // Linear, like Atlas::find and for the same reason: this is a setup call.
    Index find(const std::string& name) const;

private:
    Animations _animations;
};

// Playback state. Copyable and assignable — it holds observers, not owners — so
// it can live in the entity store's vector.
//
// The Atlas and the Animation must outlive it. That is the usual arrangement
// for this kind of thing: the atlas is loaded once at startup and the
// definitions with it, while sprites come and go.
class AnimatedSprite
{
public:
    // A default-constructed sprite has nothing to draw and ignores advance().
    AnimatedSprite() = default;

    AnimatedSprite(const Atlas& atlas, const Animation& animation);

    // Switches animation and restarts from frame 0. Restarts even if it is the
    // same animation, which is what a caller asking for it usually means; test
    // for it first if not.
    void play(const Animation& animation);

    // Restarts the current animation without changing it.
    void restart();

    // Advances by `seconds`. Handles a delta spanning several frames, because
    // rig::FrameTiming clamps at 100 ms and a 60 fps animation crosses six
    // frames in that.
    void advance(double seconds);

    // Whether this can *draw*. A sprite with an animation but no atlas is
    // still meaningful — it advances and reports its frame — so the weaker
    // question is answered by animation() rather than by this.
    bool valid() const { return _atlas != nullptr && _animation != nullptr; }

    // True once a Loop::Once animation has reached its last frame. Always false
    // for Repeat and PingPong, which have no end.
    bool finished() const { return _finished; }

    // Position within the animation, not into the atlas.
    std::size_t frame_number() const { return _frame; }

    // The atlas index of the frame currently showing, or Atlas::npos if there
    // is nothing to show.
    Atlas::Index frame() const;

    const Animation* animation() const { return _animation; }

    // Draws the current frame with its untrimmed top-left at (x, y), scaled by
    // an integer factor. A no-op for a sprite with nothing to show, rather than
    // a crash — a missing pose should leave a hole, not take the demo down.
    void draw(renderer::Context& context, int x, int y, int scale = 1) const;

private:
    const Atlas* _atlas = nullptr;
    const Animation* _animation = nullptr;
    double _elapsed = 0.0;  // within the current frame
    std::size_t _frame = 0; // position in the sequence
    bool _forward = true;   // PingPong direction
    bool _finished = false;
};

} // namespace gfx
