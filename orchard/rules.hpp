#pragma once

#include <cstddef>
#include <vector>

#include "actors.hpp"

//============================================================================
//
// orchard's rules: what a pickup is worth, what contact costs, when a run ends
//
//     orchard::Rules rules;
//     rules.advance(delta);
//     if (rules.collect(kind))                      { ...remove the entity... }
//     if (rules.touch_animal(slot, overlapping))    { ...burst, sit down... }
//     if (rules.reach_goal(overlapping))            { ...the run is over... }
//
// NO RENDERER, NO ENTITY STORE AND NO GEOMETRY, for the reason player.hpp and
// actors.hpp give. The caller owns the cast and tests the boxes —
// game::overlaps is one call and belongs beside the boxes it reads — and hands
// this the answer. What is left is the part a screenshot cannot check: how a
// score moves, whether a bump fires once or sixty times, and which frame the
// run ends on.
//
// THE DESIGN IS RECORDED, NOT INVENTED HERE. "The game" in
// planning/2026-08-10-game-layer-and-demo/ reads: *Foxy forages in an orchard.
// Three animals wander it. Touching one costs you your last pickup and sits you
// down dizzy for a moment.* Avoidance, not combat — nothing kills, there is no
// health and there is no fail state. The goal is the one thing this file adds
// to that paragraph, because the map gained a `goal` spawn after it was
// written.
//
// NOTHING HERE ENUMERATES THE PICKUP TYPES. A score is per Behaviour and so is
// the points table, so the map and `Behaviour` between them are the only places
// that say which pickups exist. A `cherries` field beside a `gems` field would
// make a third, and adding a pear would be an edit to all three.
//
//============================================================================
namespace orchard
{

// Behaviour used as an array subscript. The idiom is already in the tree —
// tests/test_orchard.cc counts the shipped spawn table this way — and this is
// the spelling of it, so the cast is written once.
constexpr std::size_t behaviour_slots =
    static_cast<std::size_t>(Behaviour::Count);
std::size_t slot_of(Behaviour behaviour);

// Where a run is. Three states and no more: there is nothing to pause, nothing
// to die from, and no menu.
enum class Phase {
    Playing,  // under the player's control
    Dizzy,    // sat down after a bump; the intent is ignored until it passes
    Finished, // the goal was reached and the run is over
};

// For logs and a test's INFO. Never null.
const char* phase_name(Phase phase);

// The rules' tuning, in one struct so a test drives the shipped numbers rather
// than a copy of them — the same reason Motion and Prowl are each one.
struct Stakes {
    // Seconds sat down after a bump.
    //
    // THE STATE SETS THIS DURATION AND THE ART DOES NOT, which is why `dizzy`
    // was chosen over `hurt` at ingest: 6 frames on a repeat loop holds for as
    // long as it is asked to, so this is tunable without re-timing the sidecar.
    double dizzy_time = 1.25;

    // Seconds the victory pose holds before a run stops. The same reasoning,
    // more sharply: `victory` is ONE frame on a repeat loop (decision 7's
    // note), so the art cannot end anything at all. The clock comes from here
    // and the flourish comes from the HUD and the cue.
    double victory_time = 2.5;

    // What each Behaviour is worth to collect, indexed by slot_of(). Zero for
    // everything that is not a pickup, which is what makes collect() a lookup
    // rather than a switch over the two types that happen to score today.
    //
    // Filled in the constructor rather than here: only two entries are non-zero
    // and spelling out seven initialisers would bury them.
    int points[behaviour_slots];

    Stakes();
};

// The run's state, and the whole of what contact means.
//
// The three questions are separate calls rather than one step() over a list,
// because each has a different trigger discipline and folding them together
// would hide that: a pickup fires once because the caller removes it, an animal
// fires on the EDGE of an overlap, and the goal fires once and then latches.
class Rules
{
public:
    Rules() = default;
    explicit Rules(const Stakes& stakes)
        : _stakes(stakes)
    {
    }

    const Stakes& stakes() const { return _stakes; }
    Phase phase() const { return _phase; }

    // How many of one kind have been collected and kept. Goes down when a bump
    // costs one, so this is what is held rather than what was ever touched.
    int held(Behaviour behaviour) const;

    int points() const { return _points; }

    // Bumps taken. Not a life counter — nothing runs out — but a run's record
    // of how well it was routed, and what a HUD shows instead of a health bar.
    int bumps() const { return _bumps; }

    // Seconds of sitting down still to go. Zero outside Phase::Dizzy.
    double dizzy_left() const { return _dizzy; }

    // Seconds since the goal was reached. Zero until it is.
    double finished_for() const { return _finished_for; }

    // Whether the victory pose has held for as long as Stakes asks. What a main
    // loop stops on; a screenshot run ignores it and keeps drawing.
    bool run_over() const;

    // Whether the player drives this frame. False while dizzy and false once
    // the goal is reached — the one place the rules reach back into motion, and
    // they do it by withholding the intent rather than by touching Player.
    bool accepts_intent() const { return _phase == Phase::Playing; }

    // Counts the timers down. Once per frame, before the three questions below:
    // a dizzy spell expiring on this frame should let this frame's contact land
    // rather than swallowing it.
    void advance(double delta);

    // A pickup the player overlaps. Adds its points and answers true, or
    // answers false and changes nothing if the run is over or the kind scores
    // nothing.
    //
    // No edge detection, because there is nothing to detect: the caller removes
    // the entity on a true answer, so the overlap cannot survive the frame.
    // Being dizzy does not stop it — a player sitting on a cherry has still got
    // the cherry, and the alternative is fruit that can be stood on and not
    // taken.
    bool collect(Behaviour behaviour);

    // Whether animal `slot` overlaps the player this frame, answering true on
    // the frame a bump LANDS.
    //
    // EDGE-TRIGGERED, PER ANIMAL, AND THAT IS THE POINT OF THE CALL. A bump
    // lands on the frame an overlap begins and on no other, so sixty frames
    // inside an opossum is one bump — the same press-versus-held distinction
    // Intent makes for the Jump button. Per animal, so the eagle's overlap
    // cannot clear the latch the frog set.
    //
    // It also settles what happens when a dizzy spell ends inside an animal:
    // the latch is kept whatever the phase, so an overlap that began while the
    // player was sitting down has no edge left to give when it stands up and
    // has to separate first. Without that the player is bumped again on the
    // recovery frame, loses the next pickup, and the run stalls in a loop it
    // cannot walk out of. A grace timer would do the same job less exactly —
    // long enough for the worst case is too long for every other one.
    //
    // `slot` is an index the caller assigns and keeps; the store grows to fit.
    bool touch_animal(std::size_t slot, bool overlapping);

    // The goal. True on the one frame the run ends, false every frame after,
    // including every frame the player keeps standing in it.
    bool reach_goal(bool overlapping);

private:
    // What a bump costs: the most recent pickup, and nothing if there is none.
    // Per "The game", whose only fail state is losing progress you can re-earn.
    void drop_last_pickup();

    Stakes _stakes;
    Phase _phase = Phase::Playing;

    int _held[behaviour_slots] = {0};
    int _points = 0;
    int _bumps = 0;

    double _dizzy = 0.0;
    double _finished_for = 0.0;

    // Every pickup still held, newest last. A stack rather than a single "last
    // kind", because losing one has to expose the one before it: two cherries
    // and a gem, bumped twice, costs the gem and then a cherry. Bounded by the
    // map's eighteen pickups.
    std::vector<Behaviour> _taken;

    // One overlap latch per animal, indexed by the caller's slot.
    std::vector<char> _touching;
};

} // namespace orchard
