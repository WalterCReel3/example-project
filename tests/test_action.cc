// game::ActionMap — which button an action reads, and the edge/level split.
//
// No SDL, no Context, no pad. include/game/action.hpp is a template over a
// documented Source concept for exactly this reason: a rig::Pad built in a test
// has no device and no events, so every button answers false and a test written
// against one would pass just as happily against a table that routed every
// action to the same place. The stub below answers what the test tells it to,
// which is the only way the routing is observable at all.
//
// So the assertions here are about routing and about frame semantics. They are
// written to fail against a plausible wrong implementation rather than to
// restate the right one: a table read with the wrong index, a rebind that
// reaches down() but not pressed(), a pressed() forwarded to Source::down(),
// and an out-of-range action read past the end of the four-element table.
//
// THE MATCHING WRITE IS NOT PINNED, despite the case under "Out of range" that
// looks like it. bind()'s range guard is out of an assertion's reach: dropping
// it writes _buttons[4], which disturbs no entry any assertion reads, and
// button() range-checks its own read independently — so every case in this file
// still passes. That write is observable to a sanitizer, but only when the flag
// reaches game/action.cc as well as this file. _buttons is ActionMap's only
// member, so _buttons[4] is past the end of the whole 16-byte object rather
// than inside it, and AddressSanitizer reports it on the stack and on the heap
// alike. No preset in this tree turns that on.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <game/action.hpp>
#include <rig/input.hpp>

#include <string>

namespace
{

const int button_count = static_cast<int>(rig::Button::Count);

// A Source, per the requirement in include/game/action.hpp: down() and
// pressed() over a rig::Button, both answering false for rig::Button::Count as
// rig::Pad does. Conforming there is what gives the unbound-action cases below
// their meaning — an ActionMap that routed Action::Count to a real button would
// otherwise be indistinguishable from one that routed it nowhere.
//
// The level and the edge are stored separately rather than derived from one
// another. rig::Pad cannot produce an edge without a level, but this can, and
// that is deliberate: it is what catches a wrapper whose pressed() forwards to
// Source::down().
//
// begin_frame() follows rig::Pad's contract — it clears the edges and leaves
// the held buttons alone.
class Buttons
{
public:
    Buttons()
        : _down{}
        , _edge{}
    {
    }

    void begin_frame()
    {
        for (int index = 0; index < button_count; ++index) {
            _edge[index] = false;
        }
    }

    // Down, and down for the first time this frame.
    void press(rig::Button button) { set(button, true, true); }

    // Down, but held from an earlier frame.
    void hold(rig::Button button) { set(button, true, false); }

    void release(rig::Button button) { set(button, false, false); }

    void set(rig::Button button, bool is_down, bool is_edge)
    {
        const int index = static_cast<int>(button);
        if (index < 0 || index >= button_count) {
            return;
        }
        _down[index] = is_down;
        _edge[index] = is_edge;
    }

    bool down(rig::Button button) const
    {
        const int index = static_cast<int>(button);
        return index >= 0 && index < button_count && _down[index];
    }

    bool pressed(rig::Button button) const
    {
        const int index = static_cast<int>(button);
        return index >= 0 && index < button_count && _edge[index];
    }

private:
    bool _down[button_count];
    bool _edge[button_count];
};

// Every real button down and newly down; rig::Button::Count still false. An
// action that reads any button at all is down against this, so it is what turns
// "not bound" into an observable claim rather than a coincidence.
Buttons all_held()
{
    Buttons buttons;
    for (int index = 0; index < button_count; ++index) {
        buttons.press(static_cast<rig::Button>(index));
    }
    return buttons;
}

// Only this button, nothing else.
Buttons only(rig::Button button)
{
    Buttons buttons;
    buttons.press(button);
    return buttons;
}

} // namespace

//============================================================================
// The default table
//
// Asserted through the query functions rather than through button(), because
// the property that matters is that an action reaches one button and not the
// others. A table read with a constant index, or read off by one, passes a
// direct comparison against button() far less readily than it passes here.
//============================================================================

TEST_CASE("each default action reads its own button and no other")
{
    const game::ActionMap actions;

    SUBCASE("Left is on the dpad left")
    {
        const Buttons source = only(rig::Button::Left);
        CHECK(actions.down(source, game::Action::Left));
        CHECK_FALSE(actions.down(source, game::Action::Right));
        CHECK_FALSE(actions.down(source, game::Action::Jump));
        CHECK_FALSE(actions.down(source, game::Action::Pickup));
    }

    SUBCASE("Right is on the dpad right")
    {
        const Buttons source = only(rig::Button::Right);
        CHECK_FALSE(actions.down(source, game::Action::Left));
        CHECK(actions.down(source, game::Action::Right));
        CHECK_FALSE(actions.down(source, game::Action::Jump));
        CHECK_FALSE(actions.down(source, game::Action::Pickup));
    }

    SUBCASE("Jump is on A")
    {
        const Buttons source = only(rig::Button::A);
        CHECK_FALSE(actions.down(source, game::Action::Left));
        CHECK_FALSE(actions.down(source, game::Action::Right));
        CHECK(actions.down(source, game::Action::Jump));
        CHECK_FALSE(actions.down(source, game::Action::Pickup));
    }

    SUBCASE("Pickup is on B")
    {
        const Buttons source = only(rig::Button::B);
        CHECK_FALSE(actions.down(source, game::Action::Left));
        CHECK_FALSE(actions.down(source, game::Action::Right));
        CHECK_FALSE(actions.down(source, game::Action::Jump));
        CHECK(actions.down(source, game::Action::Pickup));
    }
}

TEST_CASE("a button nothing is bound to drives no action")
{
    const game::ActionMap actions;
    const Buttons source = only(rig::Button::Start);

    CHECK_FALSE(actions.down(source, game::Action::Left));
    CHECK_FALSE(actions.down(source, game::Action::Right));
    CHECK_FALSE(actions.down(source, game::Action::Jump));
    CHECK_FALSE(actions.down(source, game::Action::Pickup));
    CHECK_FALSE(actions.pressed(source, game::Action::Jump));
}

//============================================================================
// Rebinding
//
// The one thing this class exists to buy. "The button appears exactly once, in
// a table" is only true if changing the table is sufficient — so the old button
// going quiet is as much of the claim as the new one answering.
//============================================================================

TEST_CASE("rebinding moves an action off the old button and onto the new one")
{
    game::ActionMap actions;
    actions.bind(game::Action::Jump, rig::Button::X);

    const Buttons was_jump = only(rig::Button::A);
    CHECK_FALSE(actions.down(was_jump, game::Action::Jump));

    const Buttons is_jump = only(rig::Button::X);
    CHECK(actions.down(is_jump, game::Action::Jump));
    CHECK(actions.button(game::Action::Jump) == rig::Button::X);
}

TEST_CASE("rebinding reaches pressed() as well as down()")
{
    game::ActionMap actions;
    actions.bind(game::Action::Jump, rig::Button::X);

    const Buttons source = only(rig::Button::X);
    CHECK(actions.down(source, game::Action::Jump));
    CHECK(actions.pressed(source, game::Action::Jump));

    const Buttons stale = only(rig::Button::A);
    CHECK_FALSE(actions.pressed(stale, game::Action::Jump));
}

TEST_CASE("rebinding one action leaves the others where they were")
{
    game::ActionMap actions;
    actions.bind(game::Action::Jump, rig::Button::Y);

    CHECK(actions.button(game::Action::Left) == rig::Button::Left);
    CHECK(actions.button(game::Action::Right) == rig::Button::Right);
    CHECK(actions.button(game::Action::Pickup) == rig::Button::B);

    const Buttons source = only(rig::Button::B);
    CHECK(actions.down(source, game::Action::Pickup));
}

TEST_CASE("a copy is a second scheme, not a shared one")
{
    const game::ActionMap defaults;

    game::ActionMap alternate = defaults;
    alternate.bind(game::Action::Jump, rig::Button::B);

    const Buttons source = only(rig::Button::A);
    CHECK(defaults.down(source, game::Action::Jump));
    CHECK_FALSE(alternate.down(source, game::Action::Jump));
    CHECK(defaults.button(game::Action::Jump) == rig::Button::A);
}

TEST_CASE("two actions may share a button, and both fire")
{
    game::ActionMap actions;
    actions.bind(game::Action::Pickup, rig::Button::A);

    const Buttons source = only(rig::Button::A);
    CHECK(actions.down(source, game::Action::Jump));
    CHECK(actions.down(source, game::Action::Pickup));
    CHECK(actions.pressed(source, game::Action::Jump));
    CHECK(actions.pressed(source, game::Action::Pickup));
}

//============================================================================
// Out of range
//
// Action::Count is the table's size and not an action. The header's claim is
// that it resolves to rig::Button::Count, which every conforming Source answers
// false to — so both halves are checked, because only the pair rules out a
// read past the end of a four-element table landing on something valid.
//============================================================================

TEST_CASE("Action::Count is bound to no button and drives nothing")
{
    const game::ActionMap actions;
    const Buttons source = all_held();

    CHECK(actions.button(game::Action::Count) == rig::Button::Count);
    CHECK_FALSE(actions.down(source, game::Action::Count));
    CHECK_FALSE(actions.pressed(source, game::Action::Count));
}

TEST_CASE("a value outside the enum is bound to no button either")
{
    const game::ActionMap actions;
    const Buttons source = all_held();

    const game::Action below = static_cast<game::Action>(-1);
    CHECK(actions.button(below) == rig::Button::Count);
    CHECK_FALSE(actions.down(source, below));
    CHECK_FALSE(actions.pressed(source, below));

    const game::Action above = static_cast<game::Action>(99);
    CHECK(actions.button(above) == rig::Button::Count);
    CHECK_FALSE(actions.down(source, above));
    CHECK_FALSE(actions.pressed(source, above));
}

TEST_CASE("binding an out-of-range action is ignored, not written past the end")
{
    game::ActionMap actions;
    actions.bind(game::Action::Count, rig::Button::Start);
    actions.bind(static_cast<game::Action>(-1), rig::Button::Select);

    CHECK(actions.button(game::Action::Left) == rig::Button::Left);
    CHECK(actions.button(game::Action::Right) == rig::Button::Right);
    CHECK(actions.button(game::Action::Jump) == rig::Button::A);
    CHECK(actions.button(game::Action::Pickup) == rig::Button::B);
    CHECK(actions.button(game::Action::Count) == rig::Button::Count);

    const Buttons source = all_held();
    CHECK_FALSE(actions.down(source, game::Action::Count));
}

//============================================================================
// Edges and levels
//
// The commitment in the Risks section of
// planning/2026-08-10-game-layer-and-demo/: one-shot cues fire on `pressed(),
// not down()`, because a cue retriggered every frame while a button is held
// starves an 8-voice mixer. That is only honourable if the distinction survives
// this wrapper, so it is pinned here rather than left to rig::Pad's own tests.
//============================================================================

TEST_CASE("pressed() is an edge and down() is a level, across frames")
{
    const game::ActionMap actions;
    Buttons source;

    // Frame 1: the button goes down.
    source.press(rig::Button::A);
    CHECK(actions.down(source, game::Action::Jump));
    CHECK(actions.pressed(source, game::Action::Jump));

    // Frame 2: still held. This is the assertion the mixer depends on.
    source.begin_frame();
    CHECK(actions.down(source, game::Action::Jump));
    CHECK_FALSE(actions.pressed(source, game::Action::Jump));

    // Frame 3: held for a third frame. An edge does not come back on its own.
    source.begin_frame();
    CHECK(actions.down(source, game::Action::Jump));
    CHECK_FALSE(actions.pressed(source, game::Action::Jump));

    // Frame 4: released.
    source.begin_frame();
    source.release(rig::Button::A);
    CHECK_FALSE(actions.down(source, game::Action::Jump));
    CHECK_FALSE(actions.pressed(source, game::Action::Jump));

    // Frame 5: down again, so the edge is back.
    source.begin_frame();
    source.press(rig::Button::A);
    CHECK(actions.down(source, game::Action::Jump));
    CHECK(actions.pressed(source, game::Action::Jump));
}

TEST_CASE("down() and pressed() reach different Source functions")
{
    const game::ActionMap actions;
    Buttons source;

    // A level with no edge — a held button, as far as rig::Pad is concerned.
    source.hold(rig::Button::A);
    CHECK(actions.down(source, game::Action::Jump));
    CHECK_FALSE(actions.pressed(source, game::Action::Jump));

    // An edge with no level. rig::Pad does not produce this; the stub does,
    // because it is what distinguishes forwarding pressed() to
    // Source::pressed() from forwarding it to Source::down().
    source.set(rig::Button::A, false, true);
    CHECK_FALSE(actions.down(source, game::Action::Jump));
    CHECK(actions.pressed(source, game::Action::Jump));
}

TEST_CASE("an action held on one button does not raise an edge on another")
{
    const game::ActionMap actions;
    Buttons source;

    source.hold(rig::Button::A);
    source.press(rig::Button::B);

    CHECK(actions.down(source, game::Action::Jump));
    CHECK_FALSE(actions.pressed(source, game::Action::Jump));
    CHECK(actions.down(source, game::Action::Pickup));
    CHECK(actions.pressed(source, game::Action::Pickup));
}

//============================================================================
// action_name
//
// Spelled like rig::button_name and answering out of range the same way. These
// cases pin the names themselves; the enum and the name table drifting apart is
// caught by the static_assert in game/action.cc instead, and cannot be reached
// from here — a missing entry is a build failure, not a failing assertion.
//============================================================================

TEST_CASE("action_name names each action")
{
    CHECK(std::string(game::action_name(game::Action::Left)) == "Left");
    CHECK(std::string(game::action_name(game::Action::Right)) == "Right");
    CHECK(std::string(game::action_name(game::Action::Jump)) == "Jump");
    CHECK(std::string(game::action_name(game::Action::Pickup)) == "Pickup");
}

TEST_CASE("action_name answers \"?\" outside the enum")
{
    CHECK(std::string(game::action_name(game::Action::Count)) == "?");
    CHECK(std::string(game::action_name(static_cast<game::Action>(-1))) == "?");
    CHECK(std::string(game::action_name(static_cast<game::Action>(99))) == "?");
}
