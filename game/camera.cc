#include <game/camera.hpp>

#include <cmath>

namespace game
{

namespace
{

double clamp_to(double value, double low, double high)
{
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

// One axis of the deadzone follow. `origin` is the camera's current position on
// the axis, `view` the viewport extent, `dead` the deadzone extent.
double follow_axis(double origin, double target, double view, double dead)
{
    // The deadzone's near and far edges, in view space.
    const double near_edge = (view - dead) / 2.0;
    const double far_edge = near_edge + dead;

    const double in_view = target - origin;

    if (in_view < near_edge) {
        return target - near_edge;
    }
    if (in_view > far_edge) {
        return target - far_edge;
    }

    // Inside the deadzone: unchanged, and unchanged exactly. Recomputing a
    // "no-op" from the target would introduce a rounding difference that shows
    // up as the whole background shifting a pixel under a standing player.
    return origin;
}

// One axis of the bounds clamp.
double clamp_axis(double origin, double view, double bounds)
{
    const double slack = bounds - view;

    if (slack >= 0.0) {
        return clamp_to(origin, 0.0, slack);
    }

    // The level is smaller than the viewport on this axis, so "show nothing
    // outside the level" is not satisfiable — clamping to the left edge and to
    // the right edge ask for different answers and both are right.
    //
    // Centre it. The alternative, pinning to the origin, puts all the empty
    // space on one side and reads as a bug in the level rather than as a small
    // level. Centring is symmetric, is what the same rule would do on either
    // axis, and needs no special case in a caller.
    return slack / 2.0;
}

} // namespace

Camera::Camera(int view_width, int view_height)
    : _view_width(view_width)
    , _view_height(view_height)
    , _bounds_width(view_width)
    , _bounds_height(view_height)
{
}

void Camera::set_deadzone(double width, double height)
{
    _deadzone_width = clamp_to(width, 0.0, static_cast<double>(_view_width));
    _deadzone_height = clamp_to(height, 0.0, static_cast<double>(_view_height));
}

void Camera::set_bounds(int width, int height)
{
    _bounds_width = width;
    _bounds_height = height;

    // The camera may now be showing something the new bounds exclude.
    clamp();
}

void Camera::snap_to(double target_x, double target_y)
{
    _x = target_x - _view_width / 2.0;
    _y = target_y - _view_height / 2.0;
    clamp();
}

void Camera::follow(double target_x, double target_y)
{
    _x = follow_axis(_x, target_x, static_cast<double>(_view_width),
                     _deadzone_width);
    _y = follow_axis(_y, target_y, static_cast<double>(_view_height),
                     _deadzone_height);
    clamp();
}

void Camera::clamp()
{
    _x = clamp_axis(_x, static_cast<double>(_view_width),
                    static_cast<double>(_bounds_width));
    _y = clamp_axis(_y, static_cast<double>(_view_height),
                    static_cast<double>(_bounds_height));
}

int Camera::x() const
{
    return static_cast<int>(std::floor(_x));
}

int Camera::y() const
{
    return static_cast<int>(std::floor(_y));
}

} // namespace game
