#pragma once

#include <game/collide.hpp>

//============================================================================
//
// Walking and jumping against the collision layer
//
//     orchard::Player player;
//     game::Aabb box = spawn_rect;
//     box = player.step(level.collision(), box, intent, motion, delta);
//
// NO RENDERER, NO PAD, NO LEVEL — a box, a velocity and a tile grid. The whole
// of it is arithmetic over supplied state, which is what lets the question this
// demo exists to answer ("can the level be crossed?") be run headlessly in
// tests/test_orchard.cc rather than watched.
//
// THE BOX IS THE CALLER'S, and that is the reason step() takes one and hands it
// back rather than keeping it. An owned rectangle would be a second copy of the
// extent whatever holds the player's art already carries, with nothing deriving
// one from the other — the state game::Entity::box was shaped to make
// unreachable. The demo's one copy lives on the entity: Entity::world_box()
// reads it in and Entity::place_box() writes the resolved origin back.
//
// The input arrives as an Intent rather than as a rig::Pad for the same reason:
// a test scripts the intent directly, and orchard/demo.cc is the only place
// that knows a button was involved.
//
//============================================================================
namespace orchard
{

// What the player is asking for this frame.
//
// `jump_held` and `jump_pressed` are both needed and are not redundant. You
// cannot know at the moment of a press that it was a tap, so the jump fires on
// the edge (`jump_pressed`) and its height is trimmed later by finding
// `jump_held` false during the ascent.
struct Intent {
    bool left = false;
    bool right = false;
    bool jump_pressed = false;
    bool jump_held = false;
};

// The tuning, in one struct so a test can drive the shipped numbers rather than
// a copy of them.
//
// THE TWO NUMBERS THE LEVEL IS AUTHORED AGAINST are `jump_speed` and `gravity`,
// which between them fix the peak rise at jump_speed^2 / (2 * gravity) — 66.6
// px, or 63.3 px as actually stepped at 60 Hz, since the peak falls between two
// frames. Either way it is over the 48 px between one branch platform and the
// next and over the 32 px steps in the ground profile, with enough margin that
// clearing either does not need a frame-perfect launch.
//
// `walk_speed` is the other half of a branch-to-branch hop, which is 32 px of
// gap as well as 48 px of rise. The player has to cross the gap while its feet
// are above the far platform, and at these numbers that window is about 0.35 s
// wide — 42 px of travel for a 32 px gap.
//
// `jump_cut_speed` sets the *shortest* jump, which is the one number here with
// a floor under it rather than a target: the tap has to clear a single 16 px
// cell, because the ground profile has 16 px steps in it and a minimum hop that
// cannot mount one reads as the button not having worked. At 220 px/s the tap
// rises about 26 px.
struct Motion {
    double walk_speed = 120.0;     // px/s
    double gravity = 1200.0;       // px/s^2
    double jump_speed = 400.0;     // px/s, upward, at the launch
    double jump_cut_speed = 220.0; // px/s, the ceiling once Jump is released
    double max_fall_speed = 600.0; // px/s

    // Ground credit that outlives the ground. A jump pressed within this long
    // of walking off an edge still fires: without it a player who presses Jump
    // one frame late at a platform edge gets no jump and cannot tell why.
    double coyote_time = 0.08; // s

    // A Jump pressed this long before landing fires on the landing frame, for
    // the same reason from the other side.
    double jump_buffer = 0.10; // s
};

// What walks, falls and jumps a box.
//
// The velocity, the credits and the flags are here because they are the
// player's own and nothing else holds them. The rectangle is not: a spawn
// supplies the first one and each step's answer supplies the next.
class Player
{
public:
    // One frame. Applies the intent, integrates, and resolves `box` against the
    // grid, answering where it ended up.
    //
    // `box` is in world pixels and stays `double` all the way through: it
    // integrates a velocity, and rounding it every frame makes slow movement
    // stutter. Rounding happens once, at draw. The extent is carried across
    // untouched — whatever rectangle is handed in is the one resolved, which is
    // how a spawn's own extent (the rectangle game::Level bounds-checked) stays
    // the collision box.
    game::Aabb step(const game::TileGrid& solid, const game::Aabb& box,
                    const Intent& intent, const Motion& motion, double delta);

    double velocity_x() const { return _velocity_x; }
    double velocity_y() const { return _velocity_y; }

    // Standing on something as of the last step().
    bool on_ground() const { return _on_ground; }

    // Blocked horizontally by the last step(). What a bump cue and an autopilot
    // both want; per-frame rather than sticky.
    bool bumped() const { return _bumped; }

    // -1 or 1. Holds its last value while standing still, so a sprite does not
    // snap back to facing right when the player stops.
    int facing() const { return _facing; }

private:
    double _velocity_x = 0.0;
    double _velocity_y = 0.0;

    // Seconds of each credit still live. Counted down rather than stamped
    // against a clock, because a screenshot run reads no clock at all.
    double _coyote = 0.0;
    double _buffered_jump = 0.0;

    bool _on_ground = false;
    bool _bumped = false;

    // Whether the current rise is a jump whose height may still be trimmed.
    // False while falling and false after a ceiling, so releasing Jump under a
    // branch does not apply the cut to a velocity that is already zero.
    bool _rising = false;

    int _facing = 1;
};

} // namespace orchard
