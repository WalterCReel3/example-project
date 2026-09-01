#pragma once

#include <rig/input.hpp>

//============================================================================
//
// What the player is asking for, rather than which button is held
//
//     game::ActionMap actions;
//     actions.bind(game::Action::Jump, rig::Button::B);   // this pad is odd
//
//     if (actions.down(pad, game::Action::Left))   { walk(-1); }
//     if (actions.pressed(pad, game::Action::Jump)) { jump(); }
//
// One indirection over rig::Button, and it has to earn itself, so here is the
// exact thing it buys: NO CALL SITE NAMES A BUTTON. A demo written against
// rig::Pad directly spells `Button::A` at every place jumping is decided —
// the movement code, the animation switch, the sound cue, the HUD prompt — and
// changing which button jumps means finding all of them and being sure you
// found all of them. Written against this, jumping is `Action::Jump`
// everywhere and the button appears exactly once, in a table.
//
// TWO DIFFERENT THINGS GO WRONG, and only one of them is fixed here. Worth
// separating, because the initiative doc's phrasing — "when a handheld reports
// the wrong button, the fix should be one table" — reads as though this table
// is always the one:
//
//   - THE DEVICE IS MIS-ENUMERATED. rig::Pad's raw-joystick path takes buttons
//     by index and says in its own header that the mapping is a guess, because
//     there is no way to know what button 3 is on an unrecognised device. When
//     that guess is wrong, `Button::A` is not the A button, and the right fix
//     is rig::Pad's table — fixing it there fixes every demo at once and keeps
//     the vocabulary honest. Fixing it here would leave `Button::A` meaning the
//     wrong thing for the next program.
//   - THE GAME WANTS A DIFFERENT BUTTON. The pad is enumerated correctly and
//     jumping should be on B rather than A. That is this table, and a call to
//     bind() is the whole change.
//
// Either way the demo's call sites do not move, and that is the property this
// layer actually provides. It is a smaller claim than "one table fixes a bad
// pad" and it is the one that survives reading rig/input.cc.
//
// DOWN AND PRESSED BOTH SURVIVE THIS LAYER, deliberately. A wrapper that
// offered only one would be smaller and would quietly break a commitment
// already made: the Risks section of planning/2026-08-10-game-layer-and-demo/
// says one-shot sound cues fire on edges — "pressed(), not down()" — because a
// cue retriggered every frame while a button is held is what starves an
// 8-voice mixer on the handheld. Collapsing the distinction here would make
// that impossible to honour without going around this class.
//
//============================================================================
//
// THE SOURCE IS A TEMPLATE PARAMETER, NOT A rig::Pad&, and the reason is that
// the alternative cannot be tested rather than that it is elegant.
//
// A Source must provide, and this is a documented requirement rather than an
// enforced one — there is no `concept` keyword at C++17 with GCC 8.3, so a type
// that fails this produces a template error and not a constraint diagnostic:
//
//     bool down(rig::Button) const;
//     bool pressed(rig::Button) const;
//     ... both answering false for rig::Button::Count, as rig::Pad does.
//
// rig::Pad satisfies it and is what a demo passes.
//
// Why not take a rig::Pad& directly: a Pad reads its state from SDL events, and
// a Pad constructed in a test has no device attached and no events to feed it,
// so down() and pressed() answer false for every button. A test written against
// one could assert nothing about whether an action reaches the right button —
// it would pass just as happily against a table that routed everything to the
// same place. Synthesising SDL_KEYDOWN events instead would work, at the price
// of pinning this test to rig::Pad's private keyboard scancode table, so a
// change in rig would break a test of game. The template lets tests/
// test_action.cc drive the routing directly, which is the only thing this class
// does.
//
// The house pattern, applied where the thing being selected is genuinely one
// job with two providers — which is the distinction gfx got wrong and recorded
// in CLAUDE.md.
//
// THIS IS WHY wreel::game STILL DOES NOT ASK ITS ENVIRONMENT ANYTHING. This
// header names rig::Button, a vocabulary of named buttons that is a plain enum
// in a header. It never names rig::Pad, never opens a device, never touches
// SDL. game/CMakeLists.txt's charter excludes rig because rig's JOB — finding
// out what is attached and what the user is pressing — is not this module's;
// that exclusion is untouched by consuming its enum, and the charter comment
// says so.
//
//============================================================================
namespace game
{

// What the game asks for. Four, matching the "Proposed shape" section of
// planning/2026-08-10-game-layer-and-demo/: this is the whole input surface of
// a game about walking, jumping and picking things up.
//
// Deliberately not Up/Down: nothing in the design climbs or crouches under
// player control, and an action nothing reads is a binding somebody has to keep
// working for no caller.
enum class Action {
    Left,
    Right,
    Jump,
    Pickup,
    Count // not an action; the size of the table
};

// "Jump", or "?" for a value outside the enum. For a log line saying what is
// bound to what, which is the diagnostic this layer exists to make possible.
// Spelled like rig::button_name and answering out of range the same way.
const char* action_name(Action action);

class ActionMap
{
public:
    // The default table: Left and Right on the dpad, Jump on A, Pickup on B.
    //
    // These are what a correctly-enumerated pad should give, so a device that
    // needs bind() called on it is a device whose enumeration is worth logging
    // — rig::Pad::mapped() is false on exactly those.
    ActionMap();

    // Rule of zero: a table of enums copies and moves correctly on its own, so
    // a demo wanting a second scheme keeps a second ActionMap rather than
    // mutating a shared one. Two of them cannot be compared; nothing needs to.

    // Point an action at a different button. Action::Count is ignored rather
    // than written past the end of the table.
    void bind(Action action, rig::Button button);

    // The button an action is bound to, or rig::Button::Count for a value
    // outside the enum — which every conforming Source answers false to, so
    // the query functions below need no separate range check.
    rig::Button button(Action action) const;

    // ONE BUTTON PER ACTION. Binding Jump to both A and Up is a real want and
    // is not here: nothing in the design needs it, and adding it later changes
    // this table's element type and bind()'s signature without touching a
    // single call site — which is the whole point of the class. Two actions
    // MAY share a button, and both then fire; that is legal and untested for
    // usefulness rather than forbidden.

    // Held right now.
    template<typename Source>
    bool down(const Source& source, Action action) const
    {
        return source.down(button(action));
    }

    // Went down this frame. What a jump, a pickup and a one-shot cue want; see
    // the note on edges above.
    template<typename Source>
    bool pressed(const Source& source, Action action) const
    {
        return source.pressed(button(action));
    }

private:
    rig::Button _buttons[static_cast<int>(Action::Count)];
};

} // namespace game
