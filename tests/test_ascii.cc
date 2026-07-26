// Tests for include/util/ascii.hpp.
//
// Two things are being pinned here, and the second is the reason this file
// exists at all:
//
//   1. Each predicate's boundaries. Off-by-one at a range edge is the classic
//      failure mode for hand-written classification, and it would show up in
//      the OBJ loader as malformed geometry rather than as a compile error.
//
//   2. That a byte above 0x7F is *defined* and classifies false, on a
//      signed-char and an unsigned-char target alike. char is signed on the
//      x86-64 dev box and unsigned on both ARM targets, so this is the property
//      that has to hold identically on all of them.
//
// Much of this is checked twice, at compile time via static_assert and at run
// time via CHECK. The static_asserts are the real assertion — a constexpr
// predicate that is wrong at compile time cannot be right at run time — but the
// CHECKs make failures legible in the test output instead of stopping the
// build.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <util/ascii.hpp>

#include <string>
#include <algorithm>
#include <functional>

using util::ascii_is_alphabetic;
using util::ascii_is_alphanumeric;
using util::ascii_is_any;
using util::ascii_is_bit;
using util::ascii_is_control;
using util::ascii_is_digit;
using util::ascii_is_graphic;
using util::ascii_is_hex_digit;
using util::ascii_is_horizontal_whitespace;
using util::ascii_is_lower;
using util::ascii_is_octal_digit;
using util::ascii_is_printing;
using util::ascii_is_punctuation;
using util::ascii_is_upper;
using util::ascii_is_whitespace;
using util::ascii_to_lower;
using util::ascii_to_upper;

//============================================================================
// The high-byte guarantee — the reason this header replaced <cctype>
//============================================================================

TEST_CASE("bytes above 0x7F classify false rather than invoking UB")
{
    // Spelled as an unsigned byte and cast down, which is how such a byte
    // actually arrives: from a file. On this host char is signed, so 0xA0
    // becomes negative — the exact value ::isspace() may not receive.
    const char nbsp = static_cast<char>(0xA0); // ISO-8859-1 NBSP
    const char high = static_cast<char>(0xFF);
    const char top = static_cast<char>(0x80);

    // The NBSP case is the one that matters: glibc's ISO-8859-1 tables classify
    // 0xA0 as space, so an OBJ file carrying one would tokenize differently
    // depending on the environment's locale. Here it never does.
    CHECK_FALSE(ascii_is_whitespace(nbsp));
    CHECK_FALSE(ascii_is_horizontal_whitespace(nbsp));
    CHECK_FALSE(ascii_is_any(nbsp));

    for (const char c : {nbsp, high, top}) {
        CHECK_FALSE(ascii_is_any(c));
        CHECK_FALSE(ascii_is_whitespace(c));
        CHECK_FALSE(ascii_is_digit(c));
        CHECK_FALSE(ascii_is_alphabetic(c));
        CHECK_FALSE(ascii_is_alphanumeric(c));
        CHECK_FALSE(ascii_is_control(c));
        CHECK_FALSE(ascii_is_printing(c));
        CHECK_FALSE(ascii_is_graphic(c));
        CHECK_FALSE(ascii_is_punctuation(c));
        CHECK_FALSE(ascii_is_lower(c));
        CHECK_FALSE(ascii_is_upper(c));
        CHECK_FALSE(ascii_is_hex_digit(c));
        // Unchanged, not case-folded through some locale's table.
        CHECK(ascii_to_lower(c) == c);
        CHECK(ascii_to_upper(c) == c);
    }
}

TEST_CASE("the guarantee holds for unsigned char, as it will on ARM")
{
    // char is unsigned on aarch64 and armhf, so there the same byte arrives as
    // 128-255 rather than negative. Both paths must agree, which is the whole
    // point of widening through std::make_unsigned.
    const unsigned char nbsp = 0xA0;
    const unsigned char high = 0xFF;

    CHECK_FALSE(ascii_is_whitespace(nbsp));
    CHECK_FALSE(ascii_is_any(nbsp));
    CHECK_FALSE(ascii_is_any(high));
    CHECK_FALSE(ascii_is_printing(high));

    // Signed and unsigned spellings of the same byte agree.
    CHECK(ascii_is_whitespace(static_cast<char>(0xA0)) ==
          ascii_is_whitespace(static_cast<unsigned char>(0xA0)));
    CHECK(ascii_is_any(static_cast<char>(0xFF)) ==
          ascii_is_any(static_cast<unsigned char>(0xFF)));

    // And every byte in 0x80-0xFF is outside ASCII, both ways round.
    for (int i = 0x80; i <= 0xFF; ++i) {
        CHECK_FALSE(ascii_is_any(static_cast<char>(i)));
        CHECK_FALSE(ascii_is_any(static_cast<unsigned char>(i)));
    }
}

//============================================================================
// Whitespace — the only predicate the tokenizers actually reach
//============================================================================

TEST_CASE("ascii_is_whitespace matches the C locale's six characters")
{
    static_assert(ascii_is_whitespace(' '), "");
    static_assert(ascii_is_whitespace('\t'), "");
    static_assert(ascii_is_whitespace('\n'), "");
    static_assert(ascii_is_whitespace('\v'), "");
    static_assert(ascii_is_whitespace('\f'), "");
    static_assert(ascii_is_whitespace('\r'), "");

    CHECK(ascii_is_whitespace(' '));
    CHECK(ascii_is_whitespace('\t'));
    CHECK(ascii_is_whitespace('\n'));
    CHECK(ascii_is_whitespace('\v'));
    CHECK(ascii_is_whitespace('\f'));
    CHECK(ascii_is_whitespace('\r'));

    // Boundaries: 0x08 backspace is below '\t', 0x0E is above '\r'.
    static_assert(!ascii_is_whitespace('\b'), "");
    static_assert(!ascii_is_whitespace(static_cast<char>(0x0E)), "");
    CHECK_FALSE(ascii_is_whitespace('\b'));
    CHECK_FALSE(ascii_is_whitespace(static_cast<char>(0x0E)));

    CHECK_FALSE(ascii_is_whitespace('\0'));
    CHECK_FALSE(ascii_is_whitespace('a'));
    CHECK_FALSE(ascii_is_whitespace('0'));

    // Exactly six of the 128 ASCII code points, no more.
    int count = 0;
    for (int i = 0; i < 0x80; ++i) {
        if (ascii_is_whitespace(static_cast<char>(i))) {
            ++count;
        }
    }
    CHECK(count == 6);
}

TEST_CASE("ascii_is_horizontal_whitespace is space and tab only")
{
    static_assert(ascii_is_horizontal_whitespace(' '), "");
    static_assert(ascii_is_horizontal_whitespace('\t'), "");
    static_assert(!ascii_is_horizontal_whitespace('\n'), "");

    CHECK(ascii_is_horizontal_whitespace(' '));
    CHECK(ascii_is_horizontal_whitespace('\t'));
    CHECK_FALSE(ascii_is_horizontal_whitespace('\n'));
    CHECK_FALSE(ascii_is_horizontal_whitespace('\r'));
    CHECK_FALSE(ascii_is_horizontal_whitespace('\v'));
    CHECK_FALSE(ascii_is_horizontal_whitespace('\f'));
}

//============================================================================
// Digits
//============================================================================

TEST_CASE("digit predicates hold at their range edges")
{
    static_assert(ascii_is_digit('0'), "");
    static_assert(ascii_is_digit('9'), "");
    static_assert(!ascii_is_digit('/'), ""); // 0x2F, one below '0'
    static_assert(!ascii_is_digit(':'), ""); // 0x3A, one above '9'

    CHECK(ascii_is_digit('0'));
    CHECK(ascii_is_digit('5'));
    CHECK(ascii_is_digit('9'));
    CHECK_FALSE(ascii_is_digit('/'));
    CHECK_FALSE(ascii_is_digit(':'));
    CHECK_FALSE(ascii_is_digit('a'));

    CHECK(ascii_is_bit('0'));
    CHECK(ascii_is_bit('1'));
    CHECK_FALSE(ascii_is_bit('2'));

    CHECK(ascii_is_octal_digit('0'));
    CHECK(ascii_is_octal_digit('7'));
    CHECK_FALSE(ascii_is_octal_digit('8'));
    CHECK_FALSE(ascii_is_octal_digit('9'));

    CHECK(ascii_is_hex_digit('0'));
    CHECK(ascii_is_hex_digit('9'));
    CHECK(ascii_is_hex_digit('a'));
    CHECK(ascii_is_hex_digit('f'));
    CHECK(ascii_is_hex_digit('A'));
    CHECK(ascii_is_hex_digit('F'));
    CHECK_FALSE(ascii_is_hex_digit('g'));
    CHECK_FALSE(ascii_is_hex_digit('G'));
    CHECK_FALSE(ascii_is_hex_digit('`')); // 0x60, one below 'a'
    CHECK_FALSE(ascii_is_hex_digit('@')); // 0x40, one below 'A'
}

//============================================================================
// Letters and case
//============================================================================

TEST_CASE("letter predicates hold at their range edges")
{
    static_assert(ascii_is_lower('a'), "");
    static_assert(ascii_is_lower('z'), "");
    static_assert(!ascii_is_lower('`'), ""); // 0x60
    static_assert(!ascii_is_lower('{'), ""); // 0x7B
    static_assert(ascii_is_upper('A'), "");
    static_assert(ascii_is_upper('Z'), "");
    static_assert(!ascii_is_upper('@'), ""); // 0x40
    static_assert(!ascii_is_upper('['), ""); // 0x5B

    CHECK(ascii_is_lower('a'));
    CHECK(ascii_is_lower('z'));
    CHECK_FALSE(ascii_is_lower('`'));
    CHECK_FALSE(ascii_is_lower('{'));
    CHECK_FALSE(ascii_is_lower('A'));

    CHECK(ascii_is_upper('A'));
    CHECK(ascii_is_upper('Z'));
    CHECK_FALSE(ascii_is_upper('@'));
    CHECK_FALSE(ascii_is_upper('['));
    CHECK_FALSE(ascii_is_upper('a'));

    CHECK(ascii_is_alphabetic('a'));
    CHECK(ascii_is_alphabetic('Z'));
    CHECK_FALSE(ascii_is_alphabetic('0'));
    CHECK_FALSE(ascii_is_alphabetic('_'));

    CHECK(ascii_is_alphanumeric('a'));
    CHECK(ascii_is_alphanumeric('Z'));
    CHECK(ascii_is_alphanumeric('0'));
    CHECK_FALSE(ascii_is_alphanumeric('_'));
    CHECK_FALSE(ascii_is_alphanumeric(' '));

    // 26 + 26 + 10.
    int letters = 0;
    int alnum = 0;
    for (int i = 0; i < 0x80; ++i) {
        const char c = static_cast<char>(i);
        if (ascii_is_alphabetic(c)) {
            ++letters;
        }
        if (ascii_is_alphanumeric(c)) {
            ++alnum;
        }
    }
    CHECK(letters == 52);
    CHECK(alnum == 62);
}

TEST_CASE("case conversion touches only letters and round-trips")
{
    static_assert(ascii_to_lower('A') == 'a', "");
    static_assert(ascii_to_upper('a') == 'A', "");
    static_assert(ascii_to_lower('a') == 'a', "");
    static_assert(ascii_to_upper('A') == 'A', "");

    CHECK(ascii_to_lower('A') == 'a');
    CHECK(ascii_to_lower('Z') == 'z');
    CHECK(ascii_to_upper('a') == 'A');
    CHECK(ascii_to_upper('z') == 'Z');

    // Non-letters pass through untouched. '@' and '[' bracket 'A'-'Z'; '`' and
    // '{' bracket 'a'-'z'. A range error would corrupt one of these.
    for (const char c : {'0', '9', ' ', '_', '@', '[', '`', '{', '\n', '\0'}) {
        CHECK(ascii_to_lower(c) == c);
        CHECK(ascii_to_upper(c) == c);
    }

    for (int i = 0; i < 0x80; ++i) {
        const char c = static_cast<char>(i);
        if (ascii_is_lower(c)) {
            CHECK(ascii_to_lower(ascii_to_upper(c)) == c);
        } else if (ascii_is_upper(c)) {
            CHECK(ascii_to_upper(ascii_to_lower(c)) == c);
        } else {
            CHECK(ascii_to_lower(c) == c);
            CHECK(ascii_to_upper(c) == c);
        }
    }
}

//============================================================================
// The printable/control partition
//============================================================================

TEST_CASE("control, printing and graphic partition the ASCII range")
{
    static_assert(ascii_is_control('\0'), "");
    static_assert(ascii_is_control(static_cast<char>(0x1F)), "");
    static_assert(ascii_is_control(static_cast<char>(0x7F)), ""); // DEL
    static_assert(!ascii_is_control(' '), "");

    CHECK(ascii_is_control('\0'));
    CHECK(ascii_is_control('\n'));
    CHECK(ascii_is_control(static_cast<char>(0x7F)));
    CHECK_FALSE(ascii_is_control(' '));
    CHECK_FALSE(ascii_is_control('~'));

    // Space prints but is not graphic; DEL is neither.
    CHECK(ascii_is_printing(' '));
    CHECK_FALSE(ascii_is_graphic(' '));
    CHECK(ascii_is_printing('~')); // 0x7E, the top of both
    CHECK(ascii_is_graphic('~'));
    CHECK_FALSE(ascii_is_printing(static_cast<char>(0x7F)));
    CHECK_FALSE(ascii_is_graphic(static_cast<char>(0x7F)));

    // Every ASCII code point is exactly one of control or printing, and
    // graphic is printing minus the space.
    for (int i = 0; i < 0x80; ++i) {
        const char c = static_cast<char>(i);
        CHECK(ascii_is_control(c) != ascii_is_printing(c));
        CHECK(ascii_is_graphic(c) == (ascii_is_printing(c) && c != ' '));
        // Punctuation is exactly the graphic non-alphanumerics.
        CHECK(ascii_is_punctuation(c) ==
              (ascii_is_graphic(c) && !ascii_is_alphanumeric(c)));
    }

    CHECK(ascii_is_punctuation('_'));
    CHECK(ascii_is_punctuation('/'));
    CHECK_FALSE(ascii_is_punctuation('a'));
    CHECK_FALSE(ascii_is_punctuation(' '));
}

TEST_CASE("ascii_is_any is exactly the low 128")
{
    int count = 0;
    for (int i = 0; i <= 0xFF; ++i) {
        if (ascii_is_any(static_cast<unsigned char>(i))) {
            ++count;
        }
    }
    CHECK(count == 128);
}

//============================================================================
// The properties the callable-object shape exists to provide
//============================================================================

TEST_CASE("predicates pass to standard algorithms directly")
{
    // This is why these are constexpr objects rather than P3688's free function
    // templates: a function template cannot be passed here, and util/string.hpp
    // does exactly this in both tokenizers.
    const std::string input = "   \t leading";

    const std::string::const_iterator first = std::find_if(
        input.begin(), input.end(), std::not_fn(ascii_is_whitespace));
    REQUIRE(first != input.end());
    CHECK(*first == 'l');

    const std::string::const_iterator space =
        std::find_if(input.begin(), input.end(), ascii_is_whitespace);
    CHECK(space == input.begin());

    CHECK(std::count_if(input.begin(), input.end(), ascii_is_whitespace) == 5);
}

TEST_CASE("predicates work across character types")
{
    // ASCII code points classify the same whatever type carries them.
    CHECK(ascii_is_whitespace(L' '));
    CHECK(ascii_is_whitespace(u' '));
    CHECK(ascii_is_whitespace(U' '));
    CHECK(ascii_is_digit(L'7'));
    CHECK(ascii_is_any(L' '));

    // Above ASCII they are false, including the code points a locale would
    // have opinions about. U+00A0 no-break space and U+2028 line separator are
    // both whitespace to Unicode; neither is ASCII. Written as escapes rather
    // than literal characters so the intent survives a copy-paste.
    CHECK_FALSE(ascii_is_any(L'\u00A0'));
    CHECK_FALSE(ascii_is_whitespace(L'\u00A0'));
    CHECK_FALSE(ascii_is_whitespace(u'\u2028'));
    CHECK_FALSE(ascii_is_any(U'\U0001F600'));
    CHECK_FALSE(ascii_is_whitespace(U'\U0001F600'));

    CHECK(ascii_to_upper(L'a') == L'A');
    CHECK(ascii_to_lower(U'A') == U'a');
    // Outside ASCII, returned unchanged rather than case-mapped. U+00E9 is
    // e-acute, whose uppercase U+00C9 a locale-aware toupper might produce.
    CHECK(ascii_to_upper(L'\u00E9') == L'\u00E9');
    CHECK(ascii_to_lower(L'\u00C9') == L'\u00C9');
}

// Built at compile time, which is the point: <cctype> cannot do this at all.
// Written as a constexpr function rather than a constexpr lambda so the oldest
// compiler in the matrix has less to disagree with.
struct whitespace_table {
    bool bits[128];
};

constexpr whitespace_table make_whitespace_table()
{
    whitespace_table result = {};
    for (int i = 0; i < 128; ++i) {
        result.bits[i] = ascii_is_whitespace(static_cast<char>(i));
    }
    return result;
}

TEST_CASE("predicates are usable in constant expressions")
{
    constexpr bool space_is_space = ascii_is_whitespace(' ');
    static_assert(space_is_space, "");

    constexpr whitespace_table t = make_whitespace_table();

    static_assert(t.bits[static_cast<unsigned char>(' ')], "");
    static_assert(t.bits[static_cast<unsigned char>('\t')], "");
    static_assert(!t.bits[static_cast<unsigned char>('a')], "");
    CHECK(t.bits[static_cast<unsigned char>('\n')]);
    CHECK_FALSE(t.bits[static_cast<unsigned char>('x')]);
}
