#include <gfx/animation.hpp>

#include <algorithm>
#include <cmath>

namespace gfx
{

namespace
{

// A PingPong pass visits every frame out and every interior frame back, so the
// ends are shown once rather than twice: 0,1,2,1 for three frames.
std::size_t pingpong_period(std::size_t count)
{
    return count > 1 ? (count - 1) * 2 : 1;
}

} // namespace

Loop loop_from_string(const std::string& text, Loop fallback)
{
    if (text == "repeat") {
        return Loop::Repeat;
    }
    if (text == "once") {
        return Loop::Once;
    }
    if (text == "pingpong") {
        return Loop::PingPong;
    }
    return fallback;
}

const char* to_string(Loop loop)
{
    switch (loop) {
    case Loop::Repeat:
        return "repeat";
    case Loop::Once:
        return "once";
    case Loop::PingPong:
        return "pingpong";
    }
    return "repeat";
}

double Animation::duration() const
{
    if (frames.empty() || seconds_per_frame <= 0.0) {
        return 0.0;
    }

    const std::size_t steps =
        loop == Loop::PingPong ? pingpong_period(frames.size()) : frames.size();
    return static_cast<double>(steps) * seconds_per_frame;
}

Animation animation_from_prefix(const Atlas& atlas, const std::string& prefix,
                                double fps, Loop loop)
{
    Animation animation;
    animation.name = prefix;
    animation.seconds_per_frame = fps > 0.0 ? 1.0 / fps : 0.0;
    animation.loop = loop;

    // "run" matches "run.000" but not "running.000", so the separator is part
    // of what is compared.
    const std::string wanted = prefix + ".";
    for (Atlas::Index i = 0; i < atlas.size(); ++i) {
        const std::string& id = atlas[i].id;
        if (id.size() > wanted.size() &&
            id.compare(0, wanted.size(), wanted) == 0) {
            animation.frames.push_back(i);
        }
    }

    return animation;
}

void AnimationSet::add(Animation animation)
{
    _animations.push_back(std::move(animation));
}

AnimationSet::Index AnimationSet::find(const std::string& name) const
{
    const Animations::const_iterator found = std::find_if(
        _animations.begin(), _animations.end(),
        [&name](const Animation& animation) { return animation.name == name; });

    if (found == _animations.end()) {
        return npos;
    }
    return static_cast<Index>(std::distance(_animations.begin(), found));
}

AnimatedSprite::AnimatedSprite(const Atlas& atlas, const Animation& animation)
    : _atlas(&atlas)
    , _animation(&animation)
{
}

void AnimatedSprite::play(const Animation& animation)
{
    _animation = &animation;
    restart();
}

void AnimatedSprite::restart()
{
    _elapsed = 0.0;
    _frame = 0;
    _forward = true;
    _finished = false;
}

void AnimatedSprite::advance(double seconds)
{
    if (_animation == nullptr || _animation->frames.empty() ||
        _animation->seconds_per_frame <= 0.0 || _finished || seconds <= 0.0) {
        return;
    }

    const double per_frame = _animation->seconds_per_frame;
    const std::size_t count = _animation->frames.size();

    _elapsed += seconds;
    if (_elapsed < per_frame) {
        return;
    }

    // Whole frames crossed, divided out rather than stepped one at a time.
    // rig::FrameTiming clamps a stall to 100 ms, which is several frames of any
    // animation and thousands of a fast one, and stepping would make the work
    // proportional to the stall.
    double crossed = std::floor(_elapsed / per_frame);

    // An absurd delta, or a frame duration near zero, must not turn the cast
    // below into undefined behaviour. Past this many frames every loop mode has
    // long since repeated, so the visible result is unchanged.
    const double crossed_max = 1000000.0;
    if (crossed > crossed_max) {
        crossed = crossed_max;
        _elapsed = 0.0;
    } else {
        _elapsed -= crossed * per_frame;
    }
    const std::size_t steps = static_cast<std::size_t>(crossed);

    switch (_animation->loop) {
    case Loop::Repeat:
        _frame = (_frame + steps) % count;
        break;

    case Loop::Once:
        // Finished once the whole animation has *played*, which includes the
        // last frame being held for its own duration — so a one-frame pose
        // completes after one frame time rather than never.
        if (steps >= count - _frame) {
            _frame = count - 1;
            _finished = true;
            _elapsed = 0.0;
        } else {
            _frame += steps;
        }
        break;

    case Loop::PingPong: {
        // Position on the out-and-back cycle, which shows each end once rather
        // than twice: 0,1,2,1 for three frames. Converting to a phase and back
        // keeps this arithmetic instead of a direction-flipping loop.
        const std::size_t period = pingpong_period(count);
        const std::size_t phase =
            ((_forward ? _frame : period - _frame) + steps) % period;

        _frame = phase < count ? phase : period - phase;
        _forward = phase + 1 < count;
        break;
    }
    }
}

Atlas::Index AnimatedSprite::frame() const
{
    // Deliberately not gated on valid(): which frame is showing is a property
    // of the animation alone, and a sprite that has a sequence but no atlas can
    // still answer it. Only draw() needs the atlas.
    if (_animation == nullptr || _animation->frames.empty()) {
        return Atlas::npos;
    }
    return _animation->frames[_frame];
}

void AnimatedSprite::draw(renderer::Context& context, int x, int y,
                          int scale) const
{
    if (_atlas == nullptr) {
        return;
    }

    const Atlas::Index index = frame();
    if (index == Atlas::npos || index >= _atlas->size()) {
        return;
    }
    _atlas->draw(context, index, x, y, scale);
}

} // namespace gfx
