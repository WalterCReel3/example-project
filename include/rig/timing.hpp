#pragma once

#include <cstdint>

//============================================================================
//
// Frame timing
//
//     rig::FrameClock clock(60);           // or 0 for uncapped
//     while (running) {
//         const double dt = clock.tick();
//         update(dt);
//         draw();
//     }
//
// The demo loop this replaces called SDL_Delay(10) unconditionally, which is
// neither a frame cap nor vsync: it added 10 ms to however long the frame took,
// so the frame rate depended on the scene and animation advanced per *frame*
// rather than per unit of time. On two Cortex-A7 cores and on a dev box that is
// two different speeds for the same demo.
//
// Two types, split on testability. FrameTiming is the arithmetic and takes its
// timestamps as arguments, so its behaviour — including the awkward parts below
// — is covered by tests that neither sleep nor read a clock. FrameClock is the
// thin part that reads a real clock and sleeps, and has nothing in it worth
// testing.
//
// Two decisions worth knowing about:
//
// *Delta is clamped.* A stall — an SD card read, a firmware hiccup, a debugger
// breakpoint, a window drag — otherwise yields a delta of whole seconds, and
// anything integrating it jumps: a scroller leaves the screen, a sine phase
// skips, a physics step tunnels through a wall. Clamping trades one slow frame
// for continuity, which is the right trade for a demo and for most games.
// clamped() reports when it happened so a HUD can show it rather than hide it.
//
// *The cap sleeps rather than spins.* A busy-wait would hold a core at 100% to
// hit a frame target, which on a handheld is heat and battery for nothing. The
// cost is that a sleep's granularity is a millisecond at best, so a capped
// frame rate is approximate. Where vsync is available it is the better limiter
// and this should be left uncapped.
//
//============================================================================
namespace rig
{

// Frame-timing arithmetic over externally supplied timestamps. Monotonic
// seconds from any origin; only differences are used.
class FrameTiming
{
public:
    // max_delta of 0 disables clamping. smoothing is the weight given to the
    // newest frame in the fps average — smaller is steadier and slower to
    // react. 0.05 keeps a HUD readable rather than flickering.
    explicit FrameTiming(double max_delta = 0.100, double smoothing = 0.05);

    // Records a frame boundary at `now`. Non-monotonic or repeated input gives
    // a delta of zero rather than a negative one, because a negative delta run
    // through an integrator is worse than a stalled one.
    void advance(double now);

    // Restarts from `now` without counting a frame. For use after a pause,
    // which would otherwise present as one enormous delta.
    void reset(double now);

    // Seconds covered by the most recent frame, after clamping.
    double delta() const { return delta_; }

    // Seconds since construction or the last reset(), accumulated from the
    // *clamped* deltas rather than from raw timestamps. So it is the time the
    // demo believes has passed, which is what animation is a function of, and
    // it stays consistent with delta() across a stall.
    double elapsed() const { return elapsed_; }

    // Smoothed frames per second, or 0 before there is an interval to measure.
    double fps() const;

    // Frames counted since construction or the last reset().
    std::uint64_t frames() const { return frames_; }

    // Whether the most recent advance() hit max_delta.
    bool clamped() const { return clamped_; }

private:
    double max_delta_;
    double smoothing_;
    double last_;
    double delta_;
    double elapsed_;
    double average_delta_;
    std::uint64_t frames_;
    bool started_;
    bool clamped_;
};

// FrameTiming driven by a monotonic clock, with an optional frame cap.
class FrameClock
{
public:
    // target_fps of 0 means uncapped, which is what to use with vsync.
    explicit FrameClock(int target_fps = 0, double max_delta = 0.100);

    // Sleeps if a cap is set and the frame came in early, then records the
    // frame boundary. Returns delta().
    double tick();

    // Discards the time since the last tick(). Call after loading, or on
    // regaining focus.
    void reset();

    double delta() const { return timing_.delta(); }
    double elapsed() const { return timing_.elapsed(); }
    double fps() const { return timing_.fps(); }
    std::uint64_t frames() const { return timing_.frames(); }
    bool clamped() const { return timing_.clamped(); }

    int target_fps() const { return target_fps_; }
    void set_target_fps(int fps);

    const FrameTiming& timing() const { return timing_; }

    // Monotonic seconds from an unspecified origin. Public because a caller
    // timing a *part* of a frame — the plot, the blit, the present — needs the
    // same clock the frame boundaries come from.
    static double now();

private:
    FrameTiming timing_;
    int target_fps_;
    double frame_target_;  // seconds per frame, or 0 when uncapped
    double next_deadline_; // when the current frame is allowed to end
};

} // namespace rig
