// coppers::BarField — the copper bar arithmetic.
//
// Resolved as one colour per scanline from a time value, with no rendering call
// involved, which is what makes this testable at all. The same split as
// rig::FrameTiming: the arithmetic is where the bugs are, and a bug here shows
// up as a subtly wrong picture rather than as a failure.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cstdint>
#include <set>
#include <vector>

#include "../coppers/bars.hpp"

namespace
{

std::vector<std::uint32_t> snapshot(const coppers::BarField& field)
{
    return std::vector<std::uint32_t>(field.rows(),
                                      field.rows() + field.height());
}

std::size_t distinct(const std::vector<std::uint32_t>& rows)
{
    return std::set<std::uint32_t>(rows.begin(), rows.end()).size();
}

} // namespace

TEST_CASE("every scanline is written, and the field is not blank")
{
    coppers::BarField field(240);
    field.resolve(0.0);

    const std::vector<std::uint32_t> rows = snapshot(field);
    REQUIRE(rows.size() == 240);

    // Bars plus a background: if the whole field were one colour the effect is
    // not happening, which a screenshot would show but a smoke test would not.
    CHECK(distinct(rows) > 8);

    // Nothing may be left at zero. A fully transparent scanline would mean an
    // uninitialised row, and against an opaque layer it reads as black — easy
    // to mistake for a design choice.
    for (std::size_t i = 0; i < rows.size(); ++i) {
        CHECK((rows[i] >> 24) == 255);
    }
}

TEST_CASE("the field moves over time")
{
    coppers::BarField field(240);

    field.resolve(0.0);
    const std::vector<std::uint32_t> first = snapshot(field);

    field.resolve(0.25);
    const std::vector<std::uint32_t> later = snapshot(field);

    CHECK(first != later);
}

TEST_CASE("resolve is a pure function of time")
{
    coppers::BarField a(240);
    coppers::BarField b(240);

    a.resolve(1.5);
    b.resolve(1.5);
    CHECK(snapshot(a) == snapshot(b));

    // And re-resolving the same instant reproduces it, so no state accumulates
    // between frames. This is what lets --screenshot step a fixed 1/60 and get
    // a reproducible image.
    a.resolve(9.0);
    a.resolve(1.5);
    CHECK(snapshot(a) == snapshot(b));
}

TEST_CASE("bars stay inside the field at every phase of their travel")
{
    // Sampling a full sweep rather than one instant: the interesting failure is
    // a bar whose centre plus half-height runs off an edge, which only happens
    // at the extremes of the sine.
    coppers::BarField field(240);

    for (int step = 0; step < 400; ++step) {
        const double t = static_cast<double>(step) * 0.05;
        field.resolve(t);
        const std::vector<std::uint32_t> rows = snapshot(field);
        REQUIRE(rows.size() == 240);
        for (std::size_t i = 0; i < rows.size(); ++i) {
            REQUIRE((rows[i] >> 24) == 255);
        }
    }
}

TEST_CASE("the composition scales with height rather than reshaping")
{
    // A bar occupies the same fraction of the screen at any internal
    // resolution. Without the reference-height scaling, a 240-line layer and a
    // 560-line one would look like different demos, and the internal-resolution
    // toggle would stop being a like-for-like measurement.
    coppers::BarField small(240);
    coppers::BarField large(480);

    small.resolve(2.0);
    large.resolve(2.0);

    const std::vector<std::uint32_t> small_rows = snapshot(small);
    const std::vector<std::uint32_t> large_rows = snapshot(large);

    std::size_t small_bg = 0;
    std::size_t large_bg = 0;
    for (std::size_t i = 0; i < small_rows.size(); ++i) {
        if (small_rows[i] == small_rows[0] ||
            small_rows[i] == small_rows.back()) {
            ++small_bg;
        }
    }
    for (std::size_t i = 0; i < large_rows.size(); ++i) {
        if (large_rows[i] == large_rows[0] ||
            large_rows[i] == large_rows.back()) {
            ++large_bg;
        }
    }

    // Same proportion of covered scanlines, within a few percent for rounding.
    const double small_ratio =
        static_cast<double>(small_bg) / static_cast<double>(small_rows.size());
    const double large_ratio =
        static_cast<double>(large_bg) / static_cast<double>(large_rows.size());
    CHECK(small_ratio == doctest::Approx(large_ratio).epsilon(0.08));
}

TEST_CASE("palettes cycle and change the output")
{
    REQUIRE(coppers::palette_count() > 1);

    coppers::BarField field(240);
    field.resolve(0.0);
    const std::vector<std::uint32_t> first = snapshot(field);
    const std::string first_name = field.palette_name();

    field.next_palette();
    field.resolve(0.0);
    CHECK(snapshot(field) != first);
    CHECK(std::string(field.palette_name()) != first_name);

    // Cycling all the way round returns to the start, so the button cannot walk
    // off the end of the table.
    for (std::size_t i = 1; i < coppers::palette_count(); ++i) {
        field.next_palette();
    }
    field.resolve(0.0);
    CHECK(snapshot(field) == first);
    CHECK(std::string(field.palette_name()) == first_name);
}

TEST_CASE(
    "an out-of-range palette index wraps rather than reading past the table")
{
    coppers::BarField field(240, 999);
    field.resolve(0.0);

    coppers::BarField expected(240, 999 % coppers::palette_count());
    expected.resolve(0.0);

    CHECK(snapshot(field) == snapshot(expected));
}

TEST_CASE("a degenerate height does not divide by zero or write nothing")
{
    // Guarding the constructor rather than trusting callers: the layer height
    // comes from SDL's reported output size, and a headless or misconfigured
    // display has been known to report zero.
    coppers::BarField field(0);
    field.resolve(1.0);

    CHECK(field.height() >= 1);
}
