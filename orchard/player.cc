#include "player.hpp"

#include <algorithm>

namespace orchard
{

namespace
{

// Counts a credit down without letting it go negative, so a long fall does not
// accumulate a debt that has to be paid off before the next one counts.
double expire(double remaining, double delta)
{
    return std::max(0.0, remaining - delta);
}

} // namespace

game::Aabb Player::step(const game::TileGrid& solid, const game::Aabb& box,
                        const Intent& intent, const Motion& motion,
                        double delta)
{
    // Velocity, not acceleration, on the ground axis: a platformer that
    // accelerates horizontally has to say what happens to that momentum in the
    // air, and nothing here wants an answer to that yet.
    const double direction =
        (intent.right ? 1.0 : 0.0) - (intent.left ? 1.0 : 0.0);
    _velocity_x = direction * motion.walk_speed;
    if (direction != 0.0) {
        _facing = direction > 0.0 ? 1 : -1;
    }

    _coyote = _on_ground ? motion.coyote_time : expire(_coyote, delta);
    _buffered_jump = intent.jump_pressed ? motion.jump_buffer
                                         : expire(_buffered_jump, delta);

    // Each credit widens a window that the present frame already opens on its
    // own: standing on ground is ground, and a press this frame is a press.
    // Testing the live condition beside its credit is what makes `0.0` mean "no
    // forgiveness" rather than "never". At any tuning with both credits
    // positive the disjunctions are redundant — the assignments above have
    // already set the credit on exactly the frame its live term is true — so
    // this is the same jump the shipped numbers always produced.
    const bool from_ground = _on_ground || _coyote > 0.0;
    const bool asking = intent.jump_pressed || _buffered_jump > 0.0;

    if (asking && from_ground) {
        _velocity_y = -motion.jump_speed;
        _rising = true;
        _on_ground = false;

        // Both credits are spent by the jump they paid for. Leaving either
        // alive would let one press fire twice — once at the edge and again on
        // the next frame, which still has ground credit.
        _buffered_jump = 0.0;
        _coyote = 0.0;
    }

    if (_rising) {
        if (_velocity_y >= 0.0) {
            _rising = false;
        } else if (!intent.jump_held && _velocity_y < -motion.jump_cut_speed) {
            _velocity_y = -motion.jump_cut_speed;
        }
    }

    _velocity_y =
        std::min(_velocity_y + motion.gravity * delta, motion.max_fall_speed);

    const game::Resolution moved =
        game::slide(solid, box, _velocity_x * delta, _velocity_y * delta);

    _bumped = moved.hit_x;
    if (moved.hit_x) {
        _velocity_x = 0.0;
    }

    // Recomputed every frame rather than latched: gravity is applied above, so
    // a player resting on a floor is blocked downward on every single frame and
    // this reads true continuously. Walking off an edge simply stops setting
    // it.
    _on_ground = moved.hit_y && _velocity_y > 0.0;

    if (moved.hit_y) {
        if (_velocity_y < 0.0) {
            // A ceiling. The rise is over whatever the button is doing.
            _rising = false;
        }
        _velocity_y = 0.0;
    }

    return game::Aabb{moved.x, moved.y, box.width, box.height};
}

} // namespace orchard
