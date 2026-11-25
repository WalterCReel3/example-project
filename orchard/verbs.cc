#include "verbs.hpp"

#include <cstddef>

namespace orchard
{

namespace
{

// Indexed by Verb, in the enum's order.
//
// Deduced length plus a static_assert rather than `[Verb::Count]`: a fixed
// length would accept a short list and value-initialise the tail, so a verb
// added without a binding would silently read as Button::Up — which is a live
// button, and the symptom is one verb quietly aliasing another.
const rig::Button bindings[] = {
    rig::Button::Left,  // Left
    rig::Button::Right, // Right
    rig::Button::A,     // Jump
    rig::Button::Start, // Quit
};

const char* const names[] = {
    "Left",
    "Right",
    "Jump",
    "Quit",
};

constexpr std::size_t verb_count = static_cast<std::size_t>(Verb::Count);

static_assert(sizeof(bindings) / sizeof(bindings[0]) == verb_count,
              "every Verb needs a button");
static_assert(sizeof(names) / sizeof(names[0]) == verb_count,
              "every Verb needs a name");

} // namespace

const char* verb_name(Verb verb)
{
    return names[static_cast<std::size_t>(verb)];
}

rig::Button button_for(Verb verb)
{
    return bindings[static_cast<std::size_t>(verb)];
}

} // namespace orchard
