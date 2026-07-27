// coppers::GlyphSheet — the bitmap font.
//
// Loads the real sheet from data/ rather than a synthetic fixture, because the
// things worth testing are properties of that file: that the grid divides, that
// the charset maps where it is claimed to, and that the mask is read the right
// way up. A fixture would test the arithmetic against itself.
//
// No video subsystem is needed. IMG_Load does not require one, which is what
// lets the whole glyph path be covered headlessly and under qemu.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <string>

#include <rig/assets.hpp>

#include "../coppers/glyphs.hpp"

namespace
{

// The geometry the demo passes, kept in one place so a change to the sheet
// fails here rather than in a screenshot.
coppers::GlyphSheet load()
{
    return coppers::GlyphSheet(rig::asset_path("glyphs-16x16.png"), 20, 3, 32);
}

} // namespace

TEST_CASE("the sheet loads with the geometry the demo assumes")
{
    const coppers::GlyphSheet sheet = load();

    CHECK(sheet.cell_width() == 16);
    CHECK(sheet.cell_height() == 16);
    CHECK(sheet.sheet_width() == 320);
    CHECK(sheet.sheet_height() == 48);

    // ASCII 32..91: space through '['. No lowercase, which is why
    // to_sheet_text() upper-cases rather than substituting.
    CHECK(sheet.first_char() == 32);
    CHECK(sheet.last_char() == 91);
}

TEST_CASE("a grid that does not divide the image is an error, not a guess")
{
    // Silently accepting this would scramble every glyph, which presents as
    // unreadable text rather than as a failure — and only once something is on
    // screen.
    CHECK_THROWS_AS(
        coppers::GlyphSheet(rig::asset_path("glyphs-16x16.png"), 7, 3, 32),
        std::runtime_error);
}

TEST_CASE("a missing sheet throws rather than yielding an empty font")
{
    CHECK_THROWS(
        coppers::GlyphSheet(rig::asset_path("no-such-sheet.png"), 20, 3, 32));
}

TEST_CASE("cell rectangles follow from the index by arithmetic")
{
    const coppers::GlyphSheet sheet = load();
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    // Space is index 0: the very first cell.
    sheet.cell_rect(' ', x, y, w, h);
    CHECK(x == 0);
    CHECK(y == 0);
    CHECK(w == 16);
    CHECK(h == 16);

    // '0' is ASCII 48, so index 16 — second row of a twenty-column grid would
    // be index 20, so this is still row 0, column 16.
    sheet.cell_rect('0', x, y, w, h);
    CHECK(x == 16 * 16);
    CHECK(y == 0);

    // 'A' is 65, index 33: row 1, column 13.
    sheet.cell_rect('A', x, y, w, h);
    CHECK(x == 13 * 16);
    CHECK(y == 1 * 16);

    // 'Z' is 90, index 58: row 2, column 18.
    sheet.cell_rect('Z', x, y, w, h);
    CHECK(x == 18 * 16);
    CHECK(y == 2 * 16);
}

TEST_CASE("characters outside the sheet resolve to the space cell")
{
    const coppers::GlyphSheet sheet = load();
    int x = -1;
    int y = -1;
    int w = 0;
    int h = 0;

    // Lowercase is past the end of the sheet. Reaching here at all is a bug in
    // the caller, but resolving to cell 0 rather than reading past the mask is
    // what stops it being a crash.
    sheet.cell_rect('a', x, y, w, h);
    CHECK(x == 0);
    CHECK(y == 0);

    // And a high-bit byte, which is what a stray UTF-8 continuation looks like.
    sheet.cell_rect(static_cast<char>(0xC3), x, y, w, h);
    CHECK(x == 0);
    CHECK(y == 0);
}

TEST_CASE("the mask has ink where a glyph is and none where a space is")
{
    const coppers::GlyphSheet sheet = load();

    int space_pixels = 0;
    int letter_pixels = 0;
    for (int y = 0; y < sheet.cell_height(); ++y) {
        for (int x = 0; x < sheet.cell_width(); ++x) {
            if (sheet.pixel(' ', x, y)) {
                ++space_pixels;
            }
            if (sheet.pixel('A', x, y)) {
                ++letter_pixels;
            }
        }
    }

    CHECK(space_pixels == 0);

    // A capital A in a 16x16 cell is somewhere in the low hundreds of pixels.
    // Bounded on both sides: zero would mean the threshold rejected everything,
    // and 256 would mean it accepted the background too — the two ways reading
    // a two-tone image as a mask goes wrong.
    CHECK(letter_pixels > 20);
    CHECK(letter_pixels < 200);
}

TEST_CASE("glyphs are not read upside down")
{
    const coppers::GlyphSheet sheet = load();

    // 'L' has ink along its bottom row and only its left edge at the top, so it
    // distinguishes a vertical flip — which a row-order mistake in the mask
    // would produce and which a pixel count would not catch.
    int top = 0;
    int bottom = 0;
    for (int x = 0; x < sheet.cell_width(); ++x) {
        for (int y = 0; y < 4; ++y) {
            if (sheet.pixel('L', x, y)) {
                ++top;
            }
        }
        for (int y = sheet.cell_height() - 4; y < sheet.cell_height(); ++y) {
            if (sheet.pixel('L', x, y)) {
                ++bottom;
            }
        }
    }

    CHECK(bottom > top);
}

TEST_CASE("to_sheet_text upper-cases and blanks what cannot be drawn")
{
    const coppers::GlyphSheet sheet = load();

    CHECK(coppers::to_sheet_text("wreel", sheet) == "WREEL");
    CHECK(coppers::to_sheet_text("Copper Bars 1", sheet) == "COPPER BARS 1");

    // Lowercase is handled by upper-casing; anything still outside the range
    // becomes a space rather than a wrong glyph.
    CHECK(coppers::to_sheet_text("a~z", sheet) == "A Z");
    CHECK(coppers::to_sheet_text("\xC3\xA9", sheet) == "  ");

    // Length is preserved, which the scroller relies on: it computes the
    // message width as character count times cell width.
    const std::string mixed = "Hello, World! 42";
    CHECK(coppers::to_sheet_text(mixed, sheet).size() == mixed.size());
}
