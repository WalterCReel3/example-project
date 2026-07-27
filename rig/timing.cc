#include <rig/timing.hpp>

#include <SDL.h>

#include <chrono>

namespace rig
{

FrameTiming::FrameTiming(double max_delta, double smoothing)
    : max_delta_(max_delta)
    , smoothing_(smoothing)
    , last_(0.0)
    , delta_(0.0)
    , elapsed_(0.0)
    , average_delta_(0.0)
    , frames_(0)
    , started_(false)
    , clamped_(false)
{
}

void FrameTiming::advance(double now)
{
    clamped_ = false;

    // The first advance() establishes the origin. There is no previous frame to
    // measure against, so it contributes a zero delta rather than the interval
    // between construction and the first frame — which on a real loop includes
    // asset loading, and would be the largest delta of the run.
    if (!started_) {
        started_ = true;
        last_ = now;
        delta_ = 0.0;
        ++frames_;
        return;
    }

    double delta = now - last_;
    last_ = now;

    if (delta < 0.0) {
        delta = 0.0;
    }
    if (max_delta_ > 0.0 && delta > max_delta_) {
        delta = max_delta_;
        clamped_ = true;
    }

    delta_ = delta;
    elapsed_ += delta;
    ++frames_;

    // Seeded from the first real delta instead of easing up from zero, which
    // would report an implausibly high frame rate for the first second — which
    // is exactly when someone is watching the number.
    if (average_delta_ > 0.0) {
        average_delta_ += smoothing_ * (delta - average_delta_);
    } else {
        average_delta_ = delta;
    }
}

void FrameTiming::reset(double now)
{
    last_ = now;
    delta_ = 0.0;
    clamped_ = false;
    started_ = true;
}

double FrameTiming::fps() const
{
    if (average_delta_ <= 0.0) {
        return 0.0;
    }
    return 1.0 / average_delta_;
}

double FrameClock::now()
{
    // steady_clock rather than SDL_GetTicks(): SDL_GetTicks is milliseconds in
    // a Uint32, so it has no sub-millisecond resolution to time a plot or a
    // blit with, and it wraps after 49 days. SDL_GetPerformanceCounter would
    // also serve, but this needs no SDL initialisation, which lets the timing
    // tests run without a video or audio subsystem.
    using clock = std::chrono::steady_clock;
    const std::chrono::duration<double> since_epoch =
        clock::now().time_since_epoch();
    return since_epoch.count();
}

FrameClock::FrameClock(int target_fps, double max_delta)
    : timing_(max_delta)
    , target_fps_(0)
    , frame_target_(0.0)
    , next_deadline_(0.0)
{
    set_target_fps(target_fps);
}

void FrameClock::set_target_fps(int fps)
{
    target_fps_ = fps > 0 ? fps : 0;
    frame_target_ =
        target_fps_ > 0 ? 1.0 / static_cast<double>(target_fps_) : 0.0;
    next_deadline_ = frame_target_ > 0.0 ? now() + frame_target_ : 0.0;
}

double FrameClock::tick()
{
    double t = now();

    if (frame_target_ > 0.0) {
        const double remaining = next_deadline_ - t;
        if (remaining > 0.0) {
            // Truncated to whole milliseconds, so this returns at or just after
            // the deadline rather than before it. The residual is absorbed by
            // the deadline accumulating below, so it does not become drift.
            const double ms = remaining * 1000.0;
            if (ms >= 1.0) {
                SDL_Delay(static_cast<Uint32>(ms));
            }
            t = now();
        }

        // Accumulated rather than recomputed from t, so a frame finishing early
        // or late does not shift every deadline after it.
        next_deadline_ += frame_target_;

        // Unless we are already past it, which means the frame overran its
        // budget. Catching up would spend the next several frames sleeping for
        // nothing, trying to win back time that is gone; resynchronise instead
        // and accept the dropped frames.
        if (next_deadline_ < t) {
            next_deadline_ = t + frame_target_;
        }
    }

    timing_.advance(t);
    return timing_.delta();
}

void FrameClock::reset()
{
    const double t = now();
    timing_.reset(t);
    if (frame_target_ > 0.0) {
        next_deadline_ = t + frame_target_;
    }
}

} // namespace rig
