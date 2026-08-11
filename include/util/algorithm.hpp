#pragma once

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <type_traits>

//============================================================================
//
// Small algorithms over ranges and values
//
//     const std::size_t i = util::index_of(_frames, by_id);   // or npos
//     if (!util::all_positive(width, height)) { ... }
//     if (util::in_range(x, 0, width)) { ... }
//
// Named shapes that were being rewritten inline. Each one here appeared at
// least twice in the tree with the same structure and a different spelling,
// which is the bar for extracting it — a helper with one caller is indirection,
// not an abstraction.
//
// Nothing here knows about geometry. `contains(outer, inner)` for rectangles
// lives in gfx/renderer/types.hpp with the Rect it needs, because a container
// algorithm has no business depending on a rendering type and util has no
// business owning one.
//
// These take ranges rather than iterator pairs. That is against the standard
// library's own convention and deliberate: every call site in this tree passes
// a whole container, and `begin(c), end(c)` at each of them is the noise the
// extraction is meant to remove. Where a sub-range is genuinely wanted, the
// std:: algorithm underneath is still right there.
//
//============================================================================
namespace util
{

// What index_of returns for "not found". A free constant rather than a member,
// so a caller comparing against it does not have to name the container's type;
// std::string::npos is the model, including the lowercase spelling.
const std::size_t npos = static_cast<std::size_t>(-1);

// Position of the first element satisfying `predicate`, or npos.
//
// The shape behind gfx::Atlas::find, gfx::AnimationSet::find and
// gfx::TileMap::find, which were three copies of std::find_if plus a distance
// plus an end test, differing only in the member being compared.
template<typename Container, typename Predicate>
std::size_t index_of(const Container& container, Predicate predicate)
{
    const auto first = std::begin(container);
    const auto last = std::end(container);
    const auto found = std::find_if(first, last, predicate);

    if (found == last) {
        return npos;
    }
    return static_cast<std::size_t>(std::distance(first, found));
}

// Position of the first element equal to `value`, or npos.
template<typename Container, typename T>
std::size_t index_of_value(const Container& container, const T& value)
{
    return index_of(container,
                    [&value](const auto& element) { return element == value; });
}

// Whether every argument is greater than zero.
//
// The most repeated conditional in the tree: a dozen sites spelled
// `width <= 0 || height <= 0`, and one spelled it over four values. Naming it
// says what is being asked — a dimension has to be positive to mean anything —
// where the inline form reads as an arbitrary bounds check.
//
// Variadic because the arity varies: two for a surface, four for a map's tile
// geometry.
template<typename... Ts>
constexpr bool all_positive(Ts... values)
{
    static_assert(sizeof...(Ts) > 0, "all_positive() needs something to test");
    static_assert((std::is_arithmetic<Ts>::value && ...),
                  "all_positive() is for numbers");

    return ((values > Ts{}) && ...);
}

// Half-open containment: begin <= value < end, which is what an index test
// against a size wants. Closed intervals are the other convention and the
// reason to be explicit about which this is.
template<typename T>
constexpr bool in_range(T value, T begin, T end)
{
    return value >= begin && value < end;
}

// Whether (x, y) addresses a cell of a width x height grid. Two in_range calls,
// named because the pair is the thing a caller means and getting one of the
// four comparisons wrong is invisible until something reads out of bounds.
template<typename T>
constexpr bool in_grid(T x, T y, T width, T height)
{
    return in_range(x, T{}, width) && in_range(y, T{}, height);
}

} // namespace util
