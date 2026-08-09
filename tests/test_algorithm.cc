// util::algorithm and the gfx::renderer rectangle predicates.
//
// These were extracted from shapes the tree had written inline more than once —
// three copies of find-by-name, two of rect-inside-bounds, a dozen of
// `w <= 0 || h <= 0`. The point of testing them here rather than only through
// their callers is that they are now shared: a wrong edge case in index_of is
// three bugs, not one.
//
// The edges are where these earn their keep, so that is what this covers:
// empty containers, the first and last element, a value exactly on a boundary,
// and a zero-extent rectangle.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <gfx/renderer/types.hpp>
#include <util/algorithm.hpp>

#include <string>
#include <vector>

//============================================================================
// index_of
//============================================================================

TEST_CASE("index_of finds the first match, or npos")
{
    const std::vector<std::string> names{"idle", "run", "jump", "run"};

    CHECK(util::index_of(
              names, [](const std::string& s) { return s == "idle"; }) == 0u);
    CHECK(util::index_of(
              names, [](const std::string& s) { return s == "jump"; }) == 2u);

    // First of a duplicate, which is what the three find() callers document.
    CHECK(util::index_of(
              names, [](const std::string& s) { return s == "run"; }) == 1u);

    CHECK(util::index_of(names, [](const std::string& s) {
              return s == "moonwalk";
          }) == util::npos);
}

TEST_CASE("index_of on an empty container is npos, not zero")
{
    const std::vector<int> empty;
    CHECK(util::index_of(empty, [](int) { return true; }) == util::npos);
}

TEST_CASE("index_of matching the last element is not confused with npos")
{
    // The failure mode of a hand-rolled version: comparing against end() after
    // already having taken a distance, or returning size() as "not found".
    const std::vector<int> values{1, 2, 3};
    CHECK(util::index_of(values, [](int v) { return v == 3; }) == 2u);
    CHECK(util::index_of(values, [](int v) { return v == 4; }) == util::npos);
    CHECK(util::npos != 3u);
}

TEST_CASE("index_of_value is the same thing for plain equality")
{
    const std::vector<int> values{10, 20, 30};
    CHECK(util::index_of_value(values, 20) == 1u);
    CHECK(util::index_of_value(values, 99) == util::npos);
}

TEST_CASE("index_of works on a raw array, since it takes a range")
{
    const int values[] = {5, 6, 7};
    CHECK(util::index_of_value(values, 7) == 2u);
}

//============================================================================
// all_positive
//============================================================================

TEST_CASE("all_positive rejects zero and negatives, at any arity")
{
    CHECK(util::all_positive(1));
    CHECK(util::all_positive(1, 2));
    CHECK(util::all_positive(1, 2, 3, 4));

    // Zero is not a positive dimension: a 0x16 surface has no pixels, and that
    // is the case the callers are guarding against.
    CHECK_FALSE(util::all_positive(0));
    CHECK_FALSE(util::all_positive(16, 0));
    CHECK_FALSE(util::all_positive(0, 16));
    CHECK_FALSE(util::all_positive(-1, 16));
    CHECK_FALSE(util::all_positive(16, 16, 16, -1));
}

TEST_CASE("all_positive works on the types dimensions actually have")
{
    CHECK(util::all_positive(1.5));
    CHECK_FALSE(util::all_positive(0.0));
    CHECK(util::all_positive(std::size_t{1}));
    CHECK_FALSE(util::all_positive(std::size_t{0}));

    static_assert(util::all_positive(16, 16), "");
    static_assert(!util::all_positive(16, 0), "");
}

//============================================================================
// in_range / in_grid
//============================================================================

TEST_CASE("in_range is half-open")
{
    CHECK(util::in_range(0, 0, 3));
    CHECK(util::in_range(2, 0, 3));

    // The end is excluded, which is what an index-against-size test wants.
    CHECK_FALSE(util::in_range(3, 0, 3));
    CHECK_FALSE(util::in_range(-1, 0, 3));

    // An empty range contains nothing, including its own bound.
    CHECK_FALSE(util::in_range(0, 0, 0));
}

TEST_CASE("in_grid excludes every edge that is outside")
{
    CHECK(util::in_grid(0, 0, 4, 3));
    CHECK(util::in_grid(3, 2, 4, 3));

    CHECK_FALSE(util::in_grid(4, 0, 4, 3)); // one past in x
    CHECK_FALSE(util::in_grid(0, 3, 4, 3)); // one past in y
    CHECK_FALSE(util::in_grid(-1, 0, 4, 3));
    CHECK_FALSE(util::in_grid(0, -1, 4, 3));

    // A zero-sized grid holds nothing.
    CHECK_FALSE(util::in_grid(0, 0, 0, 0));
}

//============================================================================
// Rectangle predicates
//============================================================================

TEST_CASE("contains accepts a rect flush with the bounds and rejects one past")
{
    const gfx::renderer::Rect sheet = gfx::renderer::bounds_of(64, 32);

    CHECK(gfx::renderer::contains(sheet, {0, 0, 64, 32})); // exactly the sheet
    CHECK(gfx::renderer::contains(sheet, {48, 16, 16, 16})); // flush corner
    CHECK(gfx::renderer::contains(sheet, {0, 0, 1, 1}));

    // One pixel over each edge in turn — the failure SDL does not report.
    CHECK_FALSE(gfx::renderer::contains(sheet, {49, 16, 16, 16}));
    CHECK_FALSE(gfx::renderer::contains(sheet, {48, 17, 16, 16}));
    CHECK_FALSE(gfx::renderer::contains(sheet, {-1, 0, 16, 16}));
    CHECK_FALSE(gfx::renderer::contains(sheet, {0, -1, 16, 16}));
    CHECK_FALSE(gfx::renderer::contains(sheet, {0, 0, 65, 32}));
}

TEST_CASE("contains works against bounds that are not at the origin")
{
    const gfx::renderer::Rect window{10, 10, 20, 20};
    CHECK(gfx::renderer::contains(window, {10, 10, 20, 20}));
    CHECK(gfx::renderer::contains(window, {15, 15, 5, 5}));
    CHECK_FALSE(gfx::renderer::contains(window, {5, 15, 5, 5}));
    CHECK_FALSE(gfx::renderer::contains(window, {25, 25, 10, 10}));
}

TEST_CASE("intersects is exclusive at the touching edge")
{
    const gfx::renderer::Rect a{0, 0, 10, 10};

    CHECK(gfx::renderer::intersects(a, {5, 5, 10, 10}));
    CHECK(gfx::renderer::intersects(a, {-5, -5, 10, 10}));
    CHECK(gfx::renderer::intersects(a, {0, 0, 10, 10}));

    // Sharing an edge is not overlapping: there is no pixel in both.
    CHECK_FALSE(gfx::renderer::intersects(a, {10, 0, 10, 10}));
    CHECK_FALSE(gfx::renderer::intersects(a, {0, 10, 10, 10}));
    CHECK_FALSE(gfx::renderer::intersects(a, {20, 20, 5, 5}));

    // A zero-area rectangle overlaps nothing, including itself.
    CHECK_FALSE(gfx::renderer::intersects(a, {5, 5, 0, 0}));
}

TEST_CASE("is_empty catches the rectangles that draw nothing")
{
    CHECK_FALSE(gfx::renderer::is_empty({0, 0, 1, 1}));
    CHECK(gfx::renderer::is_empty({0, 0, 0, 16}));
    CHECK(gfx::renderer::is_empty({0, 0, 16, 0}));
    CHECK(gfx::renderer::is_empty({0, 0, -4, 16}));
}
