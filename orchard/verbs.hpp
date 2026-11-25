#pragma once

#include <rig/input.hpp>

//============================================================================
//
// orchard's verbs, and the one place they become buttons
//
//     orchard::Controls controls(pad);
//     if (controls.pressed(orchard::Verb::Jump)) { ... }
//
// A verb is what this game's player can ask for. It lives in this executable
// because it is this game's vocabulary: `rig` carries the transport and the
// per-device button alias, and what an input *means* is the game's to say.
//
// `button_for` is the only place in orchard that names a button, so a rebind is
// one edit. Note what that is and is not for: a handheld reporting the wrong
// button is rig's mapping to fix, because `Button::A` must mean the same thing
// for the next program. This table is for "Jump should be B in this game".
//
// ONE BUTTON PER VERB. A verb bound to two buttons has to say what `pressed()`
// means when the second goes down while the first is already held, and the
// obvious `a || b` fires twice; nothing here needs that yet, so a verb is one
// button.
//
//============================================================================
namespace orchard
{

// A request, not an act: `Jump` is something the ground may refuse, and
// orchard::Player is where that is decided.
enum class Verb {
    Left,
    Right,
    Jump,
    Quit,
    Count // not a verb; the size of the table
};

// For a log line. Never parsed.
const char* verb_name(Verb verb);

// The button a verb is bound to. Exposed so the bindings can be logged at
// startup: on a handheld whose pad enumeration is a guess, "Jump is A" in the
// log is the difference between a five-minute diagnosis and a device round
// trip.
rig::Button button_for(Verb verb);

// A rig::Pad read in orchard's vocabulary. Holds a reference — it is a view
// over a pad somebody else owns, not a second input layer, and does not outlive
// one.
class Controls
{
public:
    explicit Controls(const rig::Pad& pad)
        : _pad(pad)
    {
    }

    bool down(Verb verb) const { return _pad.down(button_for(verb)); }
    bool pressed(Verb verb) const { return _pad.pressed(button_for(verb)); }

private:
    const rig::Pad& _pad;
};

} // namespace orchard
