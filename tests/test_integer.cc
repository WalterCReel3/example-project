// util::floor_div, floor_mod, ceil_div.
//
// Small enough to look correct and wrong often enough to be worth pinning. The
// case that matters is negative operands: C division truncates toward zero, so
// -1 / 16 is 0 and every position in the cell left of the origin reports cell
// 0. A tilemap scrolled one pixel past its left edge drops a column that way,
// and it looks like a rendering bug rather than an arithmetic one.
//
// Checked against a reference — floor and ceil on a double, over the whole
// small-integer neighbourhood — rather than against a handful of literals,
// because the failures are at sign boundaries and an example-based test tends
// to pick the examples that work.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <util/integer.hpp>

#include <cmath>
#include <cstdint>

TEST_CASE("floor_div rounds toward negative infinity")
{
    CHECK(util::floor_div(33, 16) == 2);
    CHECK(util::floor_div(32, 16) == 2);
    CHECK(util::floor_div(0, 16) == 0);

    // The whole point: these are all -1 with floor, and 0 with `/`.
    CHECK(util::floor_div(-1, 16) == -1);
    CHECK(util::floor_div(-15, 16) == -1);
    CHECK(util::floor_div(-16, 16) == -1);
    CHECK(util::floor_div(-17, 16) == -2);

    CHECK(-1 / 16 == 0); // what it is being used instead of
}

TEST_CASE("floor_div agrees with std::floor over a neighbourhood of zero")
{
    for (int b : {1, 2, 3, 16, 17, -1, -2, -16}) {
        for (int a = -100; a <= 100; ++a) {
            const int expected = static_cast<int>(
                std::floor(static_cast<double>(a) / static_cast<double>(b)));
            CAPTURE(a);
            CAPTURE(b);
            REQUIRE(util::floor_div(a, b) == expected);
        }
    }
}

TEST_CASE("floor_mod is never negative for a positive divisor")
{
    CHECK(util::floor_mod(-1, 16) == 15);
    CHECK(util::floor_mod(0, 16) == 0);
    CHECK(util::floor_mod(17, 16) == 1);

    CHECK(-1 % 16 == -1); // what it is being used instead of

    // The identity that makes the pair useful.
    for (int b : {1, 3, 16, 17}) {
        for (int a = -100; a <= 100; ++a) {
            REQUIRE(util::floor_div(a, b) * b + util::floor_mod(a, b) == a);
            REQUIRE(util::floor_mod(a, b) >= 0);
            REQUIRE(util::floor_mod(a, b) < b);
        }
    }
}

TEST_CASE("ceil_div counts a partial cell as one")
{
    CHECK(util::ceil_div(32, 16) == 2);
    CHECK(util::ceil_div(33, 16) == 3);
    CHECK(util::ceil_div(1, 16) == 1);
    CHECK(util::ceil_div(0, 16) == 0);

    for (int b : {1, 2, 3, 16, 17, -1, -3, -16}) {
        for (int a = -100; a <= 100; ++a) {
            const int expected = static_cast<int>(
                std::ceil(static_cast<double>(a) / static_cast<double>(b)));
            CAPTURE(a);
            CAPTURE(b);
            REQUIRE(util::ceil_div(a, b) == expected);
        }
    }
}

TEST_CASE("they are constexpr, so a cell index can be a compile-time constant")
{
    static_assert(util::floor_div(-1, 16) == -1, "");
    static_assert(util::floor_mod(-1, 16) == 15, "");
    static_assert(util::ceil_div(33, 16) == 3, "");
    CHECK(true);
}

TEST_CASE("they work on the widths a coordinate might actually be")
{
    CHECK(util::floor_div<long>(-1, 16) == -1);
    CHECK(util::floor_div<std::int64_t>(-1, 16) == -1);
    CHECK(util::floor_div<short>(-1, 16) == -1);

    // Unsigned has no negative case, but must still divide.
    CHECK(util::floor_div<unsigned>(33u, 16u) == 2u);
    CHECK(util::ceil_div<unsigned>(33u, 16u) == 3u);
}
