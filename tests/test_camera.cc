// game::Camera — deadzone follow, clamped to level bounds, integer output.
//
// Arithmetic over supplied positions, so there is no Context, no window and no
// level here: the camera is told where the target is. That is what keeps this
// in the headless group, and it is also the reason the camera takes a position
// rather than reading one off an entity.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <game/camera.hpp>

namespace
{

// A viewport big enough that a 100x60 deadzone leaves room on every side.
const int view_w = 320;
const int view_h = 240;

game::Camera framed()
{
    game::Camera camera(view_w, view_h);
    camera.set_deadzone(100.0, 60.0);
    camera.set_bounds(2000, 1500);
    return camera;
}

} // namespace

//============================================================================
// The deadzone
//============================================================================

TEST_CASE("a target moving inside the deadzone does not move the camera")
{
    game::Camera camera = framed();
    camera.snap_to(1000.0, 700.0);

    const double origin_x = camera.exact_x();
    const double origin_y = camera.exact_y();
    const int pixel_x = camera.x();
    const int pixel_y = camera.y();

    // The deadzone is 100x60 centred, so the target may move +-50 and +-30
    // from the centre without the camera following.
    camera.follow(1000.0 + 49.0, 700.0 + 29.0);
    CHECK(camera.exact_x() == doctest::Approx(origin_x));
    CHECK(camera.exact_y() == doctest::Approx(origin_y));

    camera.follow(1000.0 - 49.0, 700.0 - 29.0);
    CHECK(camera.exact_x() == doctest::Approx(origin_x));
    CHECK(camera.exact_y() == doctest::Approx(origin_y));

    // Exactly on the edge is still inside: the camera has nothing to correct.
    camera.follow(1000.0 + 50.0, 700.0 + 30.0);
    CHECK(camera.exact_x() == doctest::Approx(origin_x));
    CHECK(camera.exact_y() == doctest::Approx(origin_y));

    // And not by a rounding-error amount either — the integer output is
    // bit-identical, which is what stops a standing player shimmering the
    // whole background.
    CHECK(camera.x() == pixel_x);
    CHECK(camera.y() == pixel_y);
}

TEST_CASE("crossing each deadzone edge moves the camera the minimum distance")
{
    game::Camera camera = framed();
    camera.snap_to(1000.0, 700.0);

    const double origin_x = camera.exact_x();
    const double origin_y = camera.exact_y();

    SUBCASE("right edge")
    {
        camera.follow(1000.0 + 55.0, 700.0);
        // 5 past the edge, so the camera moves exactly 5.
        CHECK(camera.exact_x() == doctest::Approx(origin_x + 5.0));
        CHECK(camera.exact_y() == doctest::Approx(origin_y));
    }

    SUBCASE("left edge")
    {
        camera.follow(1000.0 - 55.0, 700.0);
        CHECK(camera.exact_x() == doctest::Approx(origin_x - 5.0));
        CHECK(camera.exact_y() == doctest::Approx(origin_y));
    }

    SUBCASE("bottom edge")
    {
        camera.follow(1000.0, 700.0 + 37.0);
        CHECK(camera.exact_y() == doctest::Approx(origin_y + 7.0));
        CHECK(camera.exact_x() == doctest::Approx(origin_x));
    }

    SUBCASE("top edge")
    {
        camera.follow(1000.0, 700.0 - 37.0);
        CHECK(camera.exact_y() == doctest::Approx(origin_y - 7.0));
        CHECK(camera.exact_x() == doctest::Approx(origin_x));
    }
}

TEST_CASE("the target ends up on the deadzone edge it crossed, not centred")
{
    // The point of a deadzone: after following, the target sits where it
    // pushed to, so reversing direction costs the deadzone's width before the
    // camera moves again. A camera that re-centred would have no deadzone.
    game::Camera camera = framed();
    camera.snap_to(1000.0, 700.0);

    camera.follow(1200.0, 700.0);

    const double in_view = 1200.0 - camera.exact_x();
    CHECK(in_view == doctest::Approx((view_w + 100.0) / 2.0));

    // Reversing by less than the deadzone width does not move it back.
    const double held = camera.exact_x();
    camera.follow(1200.0 - 99.0, 700.0);
    CHECK(camera.exact_x() == doctest::Approx(held));

    // One more pixel does.
    camera.follow(1200.0 - 101.0, 700.0);
    CHECK(camera.exact_x() < held);
}

TEST_CASE("a zero deadzone locks the target to the centre")
{
    game::Camera camera(view_w, view_h);
    camera.set_deadzone(0.0, 0.0);
    camera.set_bounds(4000, 4000);

    camera.follow(1000.0, 900.0);
    CHECK(camera.exact_x() == doctest::Approx(1000.0 - view_w / 2.0));
    CHECK(camera.exact_y() == doctest::Approx(900.0 - view_h / 2.0));

    // Every step follows, however small.
    camera.follow(1000.5, 900.0);
    CHECK(camera.exact_x() == doctest::Approx(1000.5 - view_w / 2.0));
}

TEST_CASE("a deadzone larger than the viewport is clamped to it")
{
    game::Camera camera(view_w, view_h);
    camera.set_bounds(4000, 4000);
    camera.set_deadzone(9000.0, 9000.0);

    // Clamped to the view, so the deadzone edges are the screen edges: a
    // target at the very edge of the view is still inside.
    camera.snap_to(2000.0, 2000.0);
    const double held_x = camera.exact_x();

    camera.follow(held_x + view_w - 1.0, 2000.0);
    CHECK(camera.exact_x() == doctest::Approx(held_x));

    // Past the screen edge it does follow, rather than letting the target
    // disappear.
    camera.follow(held_x + view_w + 10.0, 2000.0);
    CHECK(camera.exact_x() == doctest::Approx(held_x + 10.0));
}

TEST_CASE("a negative deadzone is treated as zero rather than inverted")
{
    game::Camera camera(view_w, view_h);
    camera.set_bounds(4000, 4000);
    camera.set_deadzone(-50.0, -50.0);

    camera.follow(1000.0, 900.0);
    CHECK(camera.exact_x() == doctest::Approx(1000.0 - view_w / 2.0));
}

//============================================================================
// Clamping to the level
//============================================================================

TEST_CASE("the camera clamps at all four level bounds")
{
    game::Camera camera(view_w, view_h);
    camera.set_deadzone(100.0, 60.0);
    camera.set_bounds(1000, 800);

    SUBCASE("left")
    {
        camera.snap_to(0.0, 400.0);
        CHECK(camera.exact_x() == doctest::Approx(0.0));
        CHECK(camera.x() == 0);
    }

    SUBCASE("top")
    {
        camera.snap_to(500.0, 0.0);
        CHECK(camera.exact_y() == doctest::Approx(0.0));
        CHECK(camera.y() == 0);
    }

    SUBCASE("right")
    {
        camera.snap_to(1000.0, 400.0);
        CHECK(camera.exact_x() == doctest::Approx(1000.0 - view_w));
        CHECK(camera.x() == 1000 - view_w);
    }

    SUBCASE("bottom")
    {
        camera.snap_to(500.0, 800.0);
        CHECK(camera.exact_y() == doctest::Approx(800.0 - view_h));
        CHECK(camera.y() == 800 - view_h);
    }
}

TEST_CASE("following past a bound stops at it and stays there")
{
    game::Camera camera(view_w, view_h);
    camera.set_deadzone(100.0, 60.0);
    camera.set_bounds(1000, 800);
    camera.snap_to(500.0, 400.0);

    for (int step = 0; step < 50; ++step) {
        camera.follow(2000.0, 2000.0);
    }

    CHECK(camera.exact_x() == doctest::Approx(1000.0 - view_w));
    CHECK(camera.exact_y() == doctest::Approx(800.0 - view_h));

    for (int step = 0; step < 50; ++step) {
        camera.follow(-2000.0, -2000.0);
    }

    CHECK(camera.exact_x() == doctest::Approx(0.0));
    CHECK(camera.exact_y() == doctest::Approx(0.0));
}

TEST_CASE("bounds exactly equal to the viewport pin the camera at the origin")
{
    game::Camera camera(view_w, view_h);
    camera.set_deadzone(100.0, 60.0);
    camera.set_bounds(view_w, view_h);

    camera.follow(0.0, 0.0);
    CHECK(camera.x() == 0);

    camera.follow(10000.0, 10000.0);
    CHECK(camera.x() == 0);
    CHECK(camera.y() == 0);
}

TEST_CASE("the default bounds are the viewport, before set_bounds is called")
{
    game::Camera camera(view_w, view_h);

    camera.follow(5000.0, 5000.0);
    CHECK(camera.x() == 0);
    CHECK(camera.y() == 0);
}

TEST_CASE("set_bounds re-clamps a camera that is already out of range")
{
    game::Camera camera(view_w, view_h);
    camera.set_bounds(4000, 4000);
    camera.snap_to(3000.0, 3000.0);
    REQUIRE(camera.x() > 0);

    // Shrinking the level must not leave the camera looking outside it.
    camera.set_bounds(1000, 800);
    CHECK(camera.exact_x() == doctest::Approx(1000.0 - view_w));
    CHECK(camera.exact_y() == doctest::Approx(800.0 - view_h));
}

//============================================================================
// A level smaller than the viewport
//============================================================================

TEST_CASE("a level narrower than the view is centred, not pinned")
{
    // Clamping the left edge to 0 and the right edge to bounds - view is
    // contradictory when bounds < view. Centring is the resolution: the level
    // sits in the middle of the screen with equal margins.
    game::Camera camera(view_w, view_h);
    camera.set_deadzone(0.0, 0.0);
    camera.set_bounds(120, 1500); // narrow, but tall enough to scroll

    camera.follow(60.0, 700.0);

    // (120 - 320) / 2 == -100, so the level's left edge lands 100px in.
    CHECK(camera.exact_x() == doctest::Approx(-100.0));
    CHECK(camera.x() == -100);

    // Vertically it still follows, because that axis has room.
    CHECK(camera.exact_y() == doctest::Approx(700.0 - view_h / 2.0));
}

TEST_CASE("a level smaller in both axes is centred in both")
{
    game::Camera camera(view_w, view_h);
    camera.set_deadzone(0.0, 0.0);
    camera.set_bounds(100, 80);

    camera.follow(50.0, 40.0);
    CHECK(camera.exact_x() == doctest::Approx((100.0 - view_w) / 2.0));
    CHECK(camera.exact_y() == doctest::Approx((80.0 - view_h) / 2.0));

    // And the target moving cannot dislodge it — there is nowhere to go.
    camera.follow(0.0, 0.0);
    CHECK(camera.exact_x() == doctest::Approx((100.0 - view_w) / 2.0));
    camera.follow(100.0, 80.0);
    CHECK(camera.exact_x() == doctest::Approx((100.0 - view_w) / 2.0));
}

TEST_CASE("a small level centres to a stable integer, odd slack included")
{
    // (121 - 320) / 2 == -99.5, so the rounding rule has to answer, and answer
    // the same way every frame.
    game::Camera camera(view_w, view_h);
    camera.set_bounds(121, 240);

    camera.follow(60.0, 120.0);
    const int first = camera.x();
    CHECK(first == -100); // floor(-99.5)

    for (int step = 0; step < 20; ++step) {
        camera.follow(60.0 + step, 120.0);
        CHECK(camera.x() == first);
    }
}

//============================================================================
// Integer output
//============================================================================

TEST_CASE("a target on a .5 boundary gives one answer, not two")
{
    game::Camera camera(view_w, view_h);
    camera.set_deadzone(0.0, 0.0);
    camera.set_bounds(4000, 4000);

    // Locked centre with an odd view offset puts the camera on a .5 exactly.
    camera.follow(1000.5, 900.5);
    REQUIRE(camera.exact_x() == doctest::Approx(1000.5 - view_w / 2.0));

    const int settled_x = camera.x();
    const int settled_y = camera.y();

    // Re-following the identical target must not flip the pixel.
    for (int step = 0; step < 20; ++step) {
        camera.follow(1000.5, 900.5);
        CHECK(camera.x() == settled_x);
        CHECK(camera.y() == settled_y);
    }
}

TEST_CASE("the integer output floors, and does so through zero")
{
    game::Camera camera(view_w, view_h);
    camera.set_deadzone(0.0, 0.0);
    camera.set_bounds(4000, 4000);

    // Positive side.
    camera.snap_to(view_w / 2.0 + 10.7, view_h / 2.0);
    CHECK(camera.exact_x() == doctest::Approx(10.7));
    CHECK(camera.x() == 10);

    // Negative side, which is reachable on a level smaller than the view.
    game::Camera small(view_w, view_h);
    small.set_bounds(view_w - 5, view_h);
    CHECK(small.exact_x() == doctest::Approx(-2.5));
    CHECK(small.x() == -3); // floor, not truncation toward zero
}

TEST_CASE("the integer output never goes backwards as the camera advances")
{
    // Monotonicity is the reason for floor over round-to-nearest: a camera
    // easing forward must never report a pixel it has already passed.
    game::Camera camera(view_w, view_h);
    camera.set_deadzone(0.0, 0.0);
    camera.set_bounds(100000, 4000);

    int previous = camera.x();
    for (int step = 0; step < 200; ++step) {
        camera.follow(1000.0 + step * 0.37, 900.0);
        CHECK(camera.x() >= previous);
        previous = camera.x();
    }
}

TEST_CASE("sub-pixel target movement accumulates rather than being lost")
{
    // The reason the camera keeps a double internally. A target moving a third
    // of a pixel per frame must still move the camera a pixel every third
    // frame, not never and then all at once.
    game::Camera camera(view_w, view_h);
    camera.set_deadzone(0.0, 0.0);
    camera.set_bounds(100000, 4000);

    camera.snap_to(1000.0, 900.0);
    const double start = camera.exact_x();

    for (int step = 1; step <= 30; ++step) {
        camera.follow(1000.0 + step / 3.0, 900.0);
    }

    CHECK(camera.exact_x() == doctest::Approx(start + 10.0));
}

//============================================================================
// snap_to
//============================================================================

TEST_CASE("snap_to centres regardless of the deadzone")
{
    game::Camera camera = framed();

    camera.snap_to(1000.0, 700.0);
    CHECK(camera.exact_x() == doctest::Approx(1000.0 - view_w / 2.0));
    CHECK(camera.exact_y() == doctest::Approx(700.0 - view_h / 2.0));

    // Even from a position where follow() would have done nothing.
    camera.follow(1000.0 + 10.0, 700.0);
    camera.snap_to(1000.0 + 10.0, 700.0);
    CHECK(camera.exact_x() == doctest::Approx(1010.0 - view_w / 2.0));
}

TEST_CASE("snap_to still clamps to the level")
{
    game::Camera camera(view_w, view_h);
    camera.set_deadzone(100.0, 60.0);
    camera.set_bounds(1000, 800);

    camera.snap_to(-500.0, -500.0);
    CHECK(camera.x() == 0);
    CHECK(camera.y() == 0);

    camera.snap_to(5000.0, 5000.0);
    CHECK(camera.x() == 1000 - view_w);
    CHECK(camera.y() == 800 - view_h);
}

//============================================================================
// The two axes behave the same way
//============================================================================

TEST_CASE("x and y are computed by the same rule")
{
    // A following camera is usually met first in a side-scroller, and the
    // header claims a top-down explorer can use this unchanged. That claim is
    // worth a test: a square viewport with a square deadzone must respond
    // identically to the same displacement on either axis.
    game::Camera camera(200, 200);
    camera.set_deadzone(40.0, 40.0);
    camera.set_bounds(5000, 5000);
    camera.snap_to(1000.0, 1000.0);

    const double origin_x = camera.exact_x();
    const double origin_y = camera.exact_y();

    camera.follow(1000.0 + 33.0, 1000.0 + 33.0);
    CHECK(camera.exact_x() - origin_x == doctest::Approx(13.0));
    CHECK(camera.exact_y() - origin_y == doctest::Approx(13.0));

    camera.snap_to(1000.0, 1000.0);
    camera.follow(1000.0 - 33.0, 1000.0 - 33.0);
    CHECK(camera.exact_x() - origin_x == doctest::Approx(-13.0));
    CHECK(camera.exact_y() - origin_y == doctest::Approx(-13.0));
}
