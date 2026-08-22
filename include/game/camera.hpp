#pragma once

//============================================================================
//
// A following camera
//
//     game::Camera camera(320, 240);
//     camera.set_deadzone(64.0, 48.0);
//     camera.set_bounds(level.pixel_width(), level.pixel_height());
//     camera.snap_to(player.x, player.y);
//
//     camera.follow(player.x, player.y);
//     map.draw(context, camera.x(), camera.y());
//
// Arithmetic over supplied positions and nothing else: no renderer, no window,
// no entity store. That is what makes the whole of it testable headlessly, and
// it is why the camera is told where the target is rather than being handed
// something to read it from.
//
// THE TWO AXES ARE TREATED IDENTICALLY, which is worth stating because a
// following camera is usually met first in a side-scroller and the asymmetries
// that genre wants are easy to assume are here. They are not. The deadzone is a
// rectangle with independent width and height but no bias in either direction,
// the clamp applies to all four edges by the same rule, and follow() runs the
// same computation on x and y. A top-down tile explorer uses this unchanged —
// with a small square deadzone, or a zero-sized one for a locked centre.
//
// What a genre-specific camera adds on top, and what is deliberately NOT here:
// look-ahead that leads the target in its direction of travel, a downward bias
// while falling, and room-at-a-time transitions instead of a continuous follow.
// None of those is missing by oversight; each is a policy that belongs to a
// game, and each would be built by driving the target position or the bounds
// this camera is given rather than by adding a mode to it.
//
// WHY THE OUTPUT IS INTEGER, since it is the constraint that shapes the
// interface and is invisible from the code. A fractional camera offset puts
// every tile blit on SDL's stretch path, measured at several times the cost of
// a 1:1 blit — on a device that is software-rasterising every pixel on two
// Cortex-A7 cores, that is the difference between hitting the frame budget and
// missing it. See decision 4 of planning/2026-08-10-game-layer-and-demo/.
//
// So the rounding happens HERE and exactly once. Entity positions stay double
// on purpose — they integrate a velocity every frame, and rounding them each
// time makes slow movement stutter — and this is the single place a double
// becomes the integer a blit is given. Do not "clean up" x() and y() into
// doubles: the fractional position is available as exact_x() and exact_y() for
// anything that genuinely needs it, and every drawing caller wants the integer.
//
//============================================================================
namespace game
{

class Camera
{
public:
    // The viewport is the render target's size in pixels. Bounds default to
    // exactly the viewport, which pins the camera at the origin until
    // set_bounds() says the level is bigger.
    Camera(int view_width, int view_height);

    // A rectangle centred in the viewport that the target may move inside
    // without the camera following. Sizes in pixels, not insets.
    //
    // Clamped to the viewport: a deadzone larger than the view would let the
    // target leave the screen entirely, which is never what a caller means. A
    // zero-sized deadzone is legal and gives a locked camera that keeps the
    // target exactly centred — which is the usual setting for a top-down
    // explorer, and the reason zero is supported rather than rejected.
    void set_deadzone(double width, double height);

    // The level's full extent in pixels. The camera will not show anything
    // outside it, with one exception that is not avoidable.
    //
    // THE SMALL-LEVEL RULE: on an axis where the level is smaller than the
    // viewport, "show nothing outside the level" cannot be satisfied at all —
    // clamping to the near edge and clamping to the far edge ask for different
    // answers and both are right. The camera centres the level in the viewport
    // instead. The rule is applied per axis, so a level narrower than the view
    // but taller than it is centred on x and clamped normally on y.
    //
    // A NEGATIVE ORIGIN IS THE CORRECT RESULT THERE, and is stated because it
    // reads as a clamp that failed to anyone meeting it for the first time.
    // Centring a 120-wide level in a 320-wide viewport puts the viewport's
    // top-left at x = -100; "non-negative" and "centred" are mutually
    // exclusive, and pinning to 0 was rejected because it piles all the empty
    // space on one side and reads as a broken level rather than a small one.
    // Both that value and the odd-slack case are pinned in
    // tests/test_camera.cc.
    void set_bounds(int width, int height);

    // Centre on the target immediately, ignoring the deadzone, then clamp.
    // For a level start or a teleport, where converging from wherever the
    // camera happened to be is wrong.
    void snap_to(double target_x, double target_y);

    // Move the camera the minimum distance that brings the target back inside
    // the deadzone, then clamp to bounds. A target already inside the deadzone
    // moves the camera not at all — not by a rounding-error amount, not at all
    // — which is the property that keeps a standing player from shimmering the
    // whole background by a pixel.
    //
    // There is no smoothing. The camera is where it should be at the end of
    // every call, so a fixed-delta screenshot run reproduces exactly.
    void follow(double target_x, double target_y);

    // The camera origin — the world position of the viewport's top-left corner
    // — in whole pixels, which is what a draw call wants.
    //
    // std::floor, not a round-to-nearest: it is monotonic, so a camera easing
    // through a coordinate never goes backwards a pixel, and it has no
    // halfway case to break a tie on. A target sitting exactly on a .5
    // boundary therefore has one answer and cannot oscillate between two.
    int x() const;
    int y() const;

    // The unrounded position. For tests, and for anything that needs to know
    // the camera is between pixels — parallax, say, which computes its own
    // offset from this and rounds that once itself.
    double exact_x() const { return _x; }
    double exact_y() const { return _y; }

    int view_width() const { return _view_width; }
    int view_height() const { return _view_height; }

private:
    // Clamps _x and _y to the bounds, applying the small-level rule.
    void clamp();

    int _view_width;
    int _view_height;

    int _bounds_width;
    int _bounds_height;

    double _deadzone_width = 0.0;
    double _deadzone_height = 0.0;

    // Doubles, and clamped in doubles before being floored on the way out. The
    // camera keeps sub-pixel position for the same reason an entity does: a
    // target moving slower than one pixel per frame would otherwise never move
    // the camera at all, and then jump.
    double _x = 0.0;
    double _y = 0.0;
};

} // namespace game
