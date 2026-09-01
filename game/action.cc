#include <game/action.hpp>

#include <algorithm>
#include <iterator>

namespace game
{

namespace
{

const int action_count = static_cast<int>(Action::Count);

// Both tables are indexed by Action, so they are edited with the enum. Their
// bounds are deduced and then asserted rather than written out: an explicit
// [action_count] accepts a short initializer list with no diagnostic, and the
// missing entries become a null name out of action_name and a binding to
// rig::Button(0) — dpad up. Deducing turns that into a build failure here, in
// the file where the entry is missing.
//
// The assert checks the count, not the order. Nothing available at C++17 can
// check that entry i describes enumerator i.

const char* const names[] = {"Left", "Right", "Jump", "Pickup"};
static_assert(static_cast<int>(std::size(names)) == action_count,
              "names[] needs one entry per game::Action, in enum order");

const rig::Button default_buttons[] = {rig::Button::Left, rig::Button::Right,
                                       rig::Button::A, rig::Button::B};
static_assert(static_cast<int>(std::size(default_buttons)) == action_count,
              "default_buttons[] needs one entry per game::Action, in enum "
              "order");

bool in_table(int index)
{
    return index >= 0 && index < action_count;
}

} // namespace

const char* action_name(Action action)
{
    const int index = static_cast<int>(action);
    return in_table(index) ? names[index] : "?";
}

ActionMap::ActionMap()
    : _buttons{}
{
    std::copy(std::begin(default_buttons), std::end(default_buttons), _buttons);
}

void ActionMap::bind(Action action, rig::Button button)
{
    const int index = static_cast<int>(action);
    if (!in_table(index)) {
        return;
    }
    _buttons[index] = button;
}

rig::Button ActionMap::button(Action action) const
{
    const int index = static_cast<int>(action);
    return in_table(index) ? _buttons[index] : rig::Button::Count;
}

} // namespace game
