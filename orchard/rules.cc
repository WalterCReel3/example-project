#include "rules.hpp"

#include <algorithm>

namespace orchard
{

namespace
{

// Counts a timer down without letting it go negative, so a long frame does not
// leave a debt the next spell has to pay off. The same job player.cc's expire()
// does for the jump credits, and kept local for the same reason it is: two
// callers in two files with no shared header between them is not a library.
double expire(double remaining, double delta)
{
    return std::max(0.0, remaining - delta);
}

} // namespace

std::size_t slot_of(Behaviour behaviour)
{
    return static_cast<std::size_t>(behaviour);
}

const char* phase_name(Phase phase)
{
    switch (phase) {
    case Phase::Playing:
        return "playing";
    case Phase::Dizzy:
        return "dizzy";
    case Phase::Finished:
        return "finished";
    }
    return "unknown";
}

Stakes::Stakes()
    : points()
{
    // The gem is the rare one — three in the map against fifteen cherries — and
    // is priced so that finding all three is worth about as much as clearing
    // the cherries. That is what makes the detour to the top of a climb a
    // choice rather than a completionist tax.
    points[slot_of(Behaviour::Cherry)] = 1;
    points[slot_of(Behaviour::Gem)] = 5;
}

int Rules::held(Behaviour behaviour) const
{
    return _held[slot_of(behaviour)];
}

bool Rules::run_over() const
{
    return _phase == Phase::Finished && _finished_for >= _stakes.victory_time;
}

void Rules::advance(double delta)
{
    if (_phase == Phase::Finished) {
        _finished_for += delta;
        return;
    }

    if (_phase == Phase::Dizzy) {
        _dizzy = expire(_dizzy, delta);
        if (_dizzy <= 0.0) {
            _phase = Phase::Playing;
        }
    }
}

bool Rules::collect(Behaviour behaviour)
{
    if (_phase == Phase::Finished) {
        return false;
    }

    const int worth = _stakes.points[slot_of(behaviour)];
    if (worth == 0) {
        // Not a pickup. Answering false rather than scoring zero keeps the
        // caller's "then remove it" honest: an animal is not consumed by being
        // handed to the wrong call.
        return false;
    }

    ++_held[slot_of(behaviour)];
    _points += worth;
    _taken.push_back(behaviour);
    return true;
}

void Rules::drop_last_pickup()
{
    if (_taken.empty()) {
        return;
    }

    const Behaviour lost = _taken.back();
    _taken.pop_back();

    --_held[slot_of(lost)];
    _points -= _stakes.points[slot_of(lost)];
}

bool Rules::touch_animal(std::size_t slot, bool overlapping)
{
    if (slot >= _touching.size()) {
        _touching.resize(slot + 1, 0);
    }

    // Read and written whatever the phase. A latch that only tracked overlaps
    // the player was able to react to would rearm itself the moment a dizzy
    // spell ended inside an animal, which is the loop the header describes.
    const bool was_touching = _touching[slot] != 0;
    _touching[slot] = overlapping ? 1 : 0;

    if (!overlapping || was_touching || _phase != Phase::Playing) {
        return false;
    }

    ++_bumps;
    drop_last_pickup();

    _phase = Phase::Dizzy;
    _dizzy = _stakes.dizzy_time;
    return true;
}

bool Rules::reach_goal(bool overlapping)
{
    if (!overlapping || _phase == Phase::Finished) {
        return false;
    }

    // Reached while sitting down still counts. A player knocked into the goal
    // has arrived, and the alternative is an end-of-level marker that silently
    // does nothing for a second and a quarter.
    _phase = Phase::Finished;
    _finished_for = 0.0;
    _dizzy = 0.0;
    return true;
}

} // namespace orchard
