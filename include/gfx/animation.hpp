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

    // The LOGICAL (untrimmed) size of the frame currently showing, which is
    // what anything positioning this sprite wants: two frames of one animation
    // that trim differently have the same logical size and different source
    // sizes, and laying out against the source size makes the sprite jitter.
    //
    // Zero when there is nothing to show. Unlike frame(), these need the atlas
    // — a size is not a property of the animation — so a sprite holding a
    // sequence but no atlas answers 0 rather than guessing.
    //
    // Here because the atlas and the current index are both private: without
    // them a caller that wants to place this sprite has to be handed the atlas
    // separately and index it by hand, which is the arithmetic this replaces.
    int frame_width() const;
    int frame_height() const;

    // Where the ART sits inside that logical frame: the trimmed region's size
    // at the trim offset, in frame-local pixels with (0, 0) at the frame's
    // top-left. {0, 0, frame_width(), frame_height()} for an untrimmed frame,
    // and a zero rect when there is nothing to show.
    //
    // The logical size above is what a LAYOUT wants; this is what an ANCHOR
    // wants, and they are not the same question. A frame packed with empty rows
    // below its art has a logical bottom edge that is not the bottom of
    // anything drawn, so a caller sitting a sprite on a floor against
    // frame_height() plants the padding rather than the feet — and because the
    // padding differs between an animation's poses, the sprite then steps
    // vertically when the pose changes. `critters` has both halves of that:
    // frog-idle carries five empty rows under the frog and frog-jump carries
    // none.
    //
    // Frame-local and not world: the caller composes this with wherever it is
    // drawing, which is the same contract frame_width() has.
    renderer::Rect frame_art() const;

    const Animation* animation() const { return _animation; }

    // Draws the current frame with its untrimmed top-left at (x, y), scaled by
    // an integer factor. A no-op for a sprite with nothing to show, rather than
    // a crash — a missing pose should leave a hole, not take the demo down.
    //
    // `mirror` is handed to Atlas::draw, which reverses the frame inside its
    // logical box, so (x, y) and frame_width() mean the same either way — and
    // which throws std::logic_error if the atlas is not mirrorable(). That one
    // does take the demo down: it is a mistake in the program, not a pose
    // missing from an asset.
    //
    // It is an argument and not playback state, so frame_art() always
    // describes the frame as packed; mirrored, the art's left edge is at
    // frame_width() - frame_art().x - frame_art().w.
    void draw(renderer::Context& context, int x, int y, int scale = 1,
              Mirror mirror = Mirror::None) const;

private:
    // The frame currently showing, or null if there is nothing to show. The
    // one place the atlas is indexed, so the bounds test cannot be written out
    // correctly in one of the three callers and wrongly in another.
    const AtlasFrame* current_frame() const;

    const Atlas* _atlas = nullptr;
    const Animation* _animation = nullptr;
    double _elapsed = 0.0;  // within the current frame
    std::size_t _frame = 0; // position in the sequence
    bool _forward = true;   // PingPong direction
    bool _finished = false;
};

} // namespace gfx
