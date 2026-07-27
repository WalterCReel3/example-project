// rig::FrameTiming — the frame-timing arithmetic.
//
// Every case here supplies its own timestamps, so nothing sleeps and nothing
// reads a clock. That is the reason the arithmetic was split out of FrameClock:
// a test that has to sleep to observe a frame cap is slow, flaky under load,
// and unrunnable under qemu at a sensible speed.
//
// FrameClock itself is deliberately not covered. What it adds is a call to
// steady_clock and a call to SDL_Delay, and a test for those tests the platform
// rather than this code.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <rig/timing.hpp>

TEST_CASE("the first frame has no predecessor, so its delta is zero")
{
    rig::FrameTiming timing;

    // 1000.0 rather than 0.0: the origin is arbitrary, and a clock whose epoch
    // is far from zero is the normal case for steady_clock.
    timing.advance(1000.0);

    CHECK(timing.delta() == doctest::Approx(0.0));
    CHECK(timing.elapsed() == doctest::Approx(0.0));
    CHECK(timing.frames() == 1);
    CHECK_FALSE(timing.clamped());
}

TEST_CASE("delta is the interval between frames, and elapsed accumulates")
{
    rig::FrameTiming timing;

    timing.advance(100.0);
    timing.advance(100.016);
    CHECK(timing.delta() == doctest::Approx(0.016));
    CHECK(timing.elapsed() == doctest::Approx(0.016));

    timing.advance(100.032);
    CHECK(timing.delta() == doctest::Approx(0.016));
    CHECK(timing.elapsed() == doctest::Approx(0.032));

    CHECK(timing.frames() == 3);
}

TEST_CASE("a stall is clamped, and reported as clamped")
{
    rig::FrameTiming timing(0.100);

    timing.advance(0.0);
    timing.advance(0.016);
    REQUIRE_FALSE(timing.clamped());

    // Two and a half seconds — an SD card read, or a debugger breakpoint.
    timing.advance(2.5);
    CHECK(timing.clamped());
    CHECK(timing.delta() == doctest::Approx(0.100));

    // elapsed() follows the clamped delta rather than the wall clock. It is the
    // time the demo believes has passed, which is what animation integrates,
    // and the two must not disagree or a scroller's position stops matching its
    // own accumulated delta.
    CHECK(timing.elapsed() == doctest::Approx(0.116));

    // The flag is per-frame, not sticky.
    timing.advance(2.516);
    CHECK_FALSE(timing.clamped());
    CHECK(timing.delta() == doctest::Approx(0.016));
}

TEST_CASE("clamping can be disabled")
{
    rig::FrameTiming timing(0.0);

    timing.advance(0.0);
    timing.advance(5.0);

    CHECK_FALSE(timing.clamped());
    CHECK(timing.delta() == doctest::Approx(5.0));
    CHECK(timing.elapsed() == doctest::Approx(5.0));
}

TEST_CASE("a non-monotonic clock yields zero rather than a negative delta")
{
    rig::FrameTiming timing;

    timing.advance(10.0);
    timing.advance(9.0);

    // Negative time run through an integrator moves everything backwards, which
    // is worse than one stalled frame.
    CHECK(timing.delta() == doctest::Approx(0.0));
    CHECK(timing.elapsed() == doctest::Approx(0.0));

    // And the origin has still moved, so the next frame measures from 9.0.
    timing.advance(9.016);
    CHECK(timing.delta() == doctest::Approx(0.016));
}

TEST_CASE("repeated timestamps are a zero delta, not a division by zero")
{
    rig::FrameTiming timing;

    timing.advance(1.0);
    timing.advance(1.0);
    timing.advance(1.0);

    CHECK(timing.delta() == doctest::Approx(0.0));
    CHECK(timing.elapsed() == doctest::Approx(0.0));
    CHECK(timing.frames() == 3);

    // fps() must not be inf or NaN here. A HUD formatting %.1f of an infinity
    // is how this would otherwise be discovered.
    CHECK(timing.fps() == doctest::Approx(0.0));
}

TEST_CASE("fps is zero before there is an interval to measure")
{
    rig::FrameTiming timing;

    CHECK(timing.fps() == doctest::Approx(0.0));

    timing.advance(0.0);
    CHECK(timing.fps() == doctest::Approx(0.0));
}

TEST_CASE("fps is seeded from the first real delta, not eased up from zero")
{
    rig::FrameTiming timing;

    timing.advance(0.0);
    timing.advance(1.0 / 60.0);

    // Exactly 60, not some fraction of it working its way up. Seeding matters
    // because the alternative reports a wildly wrong figure for the first
    // second, which is exactly when someone is watching the number.
    CHECK(timing.fps() == doctest::Approx(60.0));
}

TEST_CASE("fps smooths toward a changed frame rate rather than jumping")
{
    rig::FrameTiming timing(0.100, 0.05);

    // t holds the timestamp of the last advance(), so that the interval below
    // is exactly one 30 Hz frame and not one 60 Hz frame plus one.
    double t = 0.0;
    timing.advance(t);
    for (int i = 0; i < 59; ++i) {
        t += 1.0 / 60.0;
        timing.advance(t);
    }
    REQUIRE(timing.fps() == doctest::Approx(60.0));

    // Halve the frame rate. One frame at 30 fps should barely move a smoothed
    // average, which is the entire point of smoothing it.
    t += 1.0 / 30.0;
    timing.advance(t);
    CHECK(timing.fps() > 55.0);

    // But it must converge, or the HUD would report a frame rate the demo has
    // not had for some time.
    for (int i = 0; i < 400; ++i) {
        t += 1.0 / 30.0;
        timing.advance(t);
    }
    CHECK(timing.fps() == doctest::Approx(30.0).epsilon(0.01));
}

TEST_CASE("reset re-establishes the origin without counting a frame")
{
    rig::FrameTiming timing;

    timing.advance(0.0);
    timing.advance(0.016);
    REQUIRE(timing.frames() == 2);
    REQUIRE(timing.elapsed() == doctest::Approx(0.016));

    // A pause: five seconds pass with no frames.
    timing.reset(5.0);
    CHECK(timing.frames() == 2);
    CHECK(timing.delta() == doctest::Approx(0.0));

    // The pause contributes nothing, so the next frame is a normal one rather
    // than a five-second clamp.
    timing.advance(5.016);
    CHECK(timing.delta() == doctest::Approx(0.016));
    CHECK_FALSE(timing.clamped());
    CHECK(timing.elapsed() == doctest::Approx(0.032));
}
