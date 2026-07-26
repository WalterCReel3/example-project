// util::from_string — strict text to number conversion.
//
// The point of this header is that everything which is not exactly a number
// fails, rather than producing a partial result that looks like data. Most of
// these cases are things std::stoi, atoi or pugixml's as_int() accept.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <util/number.hpp>

#include <climits>
#include <string>

TEST_CASE("integers parse, including the signed edges")
{
    int value = 0;

    REQUIRE(util::from_string("0", value));
    CHECK(value == 0);

    REQUIRE(util::from_string("42", value));
    CHECK(value == 42);

    REQUIRE(util::from_string("-42", value));
    CHECK(value == -42);

    REQUIRE(util::from_string("2147483647", value));
    CHECK(value == INT_MAX);

    REQUIRE(util::from_string("-2147483648", value));
    CHECK(value == INT_MIN);

    // Leading zeros are digits, not an octal prefix.
    REQUIRE(util::from_string("007", value));
    CHECK(value == 7);
}

TEST_CASE("out-of-range integers fail rather than wrapping")
{
    int value = -1;

    CHECK_FALSE(util::from_string("2147483648", value));
    CHECK_FALSE(util::from_string("-2147483649", value));
    CHECK_FALSE(util::from_string("99999999999999999999", value));

    // The out-parameter is untouched on failure, so a caller that pre-set it to
    // a default does not have to restore it.
    CHECK(value == -1);
}

// Boundaries are derived from <climits> rather than written out, because they
// differ across this project's own targets: `long` is 64-bit on the x86-64 host
// and 32-bit on armv7, so a hardcoded 2147483648 makes this pass on the dev box
// and fail on the Miyoo Mini. The property under test is "the type's own width
// is respected", not any particular number.
TEST_CASE("width is respected per type")
{
    const auto one_past = [](long long limit) {
        return std::to_string(limit) + "0"; // ten times the maximum
    };

    short small = 0;
    REQUIRE(util::from_string(std::to_string(SHRT_MAX), small));
    CHECK(small == SHRT_MAX);
    CHECK_FALSE(util::from_string(one_past(SHRT_MAX), small));

    long as_long = 0;
    REQUIRE(util::from_string(std::to_string(LONG_MAX), as_long));
    CHECK(as_long == LONG_MAX);
    CHECK_FALSE(util::from_string(one_past(LONG_MAX), as_long));

    // long long is at least 64 bits everywhere, so this value fits on every
    // target -- unlike the same string parsed into a long above.
    long long wide = 0;
    REQUIRE(util::from_string("2147483648", wide));
    CHECK(wide == 2147483648LL);

    unsigned int unsigned_value = 0;
    REQUIRE(util::from_string(std::to_string(UINT_MAX), unsigned_value));
    CHECK(unsigned_value == UINT_MAX);
    // A negative is not an unsigned number, rather than wrapping to a huge one.
    CHECK_FALSE(util::from_string("-1", unsigned_value));
}

TEST_CASE("trailing and leading garbage fails")
{
    int value = -1;

    // atoi returns 12 for this, which is how a malformed asset dimension
    // becomes a plausible-looking sprite.
    CHECK_FALSE(util::from_string("12px", value));
    CHECK_FALSE(util::from_string("12 ", value));
    CHECK_FALSE(util::from_string("12.0", value));
    CHECK_FALSE(util::from_string("12,13", value));
    CHECK_FALSE(util::from_string(" 12", value));
    CHECK_FALSE(util::from_string("\t12", value));
    CHECK_FALSE(util::from_string("abc", value));
    CHECK_FALSE(util::from_string("", value));
    CHECK_FALSE(util::from_string("-", value));
    CHECK_FALSE(util::from_string("0x1f", value));

    CHECK(value == -1);
}

// std::from_chars rejects a leading '+' and strtod accepts it. Normalized to
// rejection so the contract does not depend on which type is instantiated.
TEST_CASE("a leading plus is rejected for both integers and floats")
{
    int i = -1;
    double d = -1.0;

    CHECK_FALSE(util::from_string("+12", i));
    CHECK_FALSE(util::from_string("+1.5", d));

    CHECK(i == -1);
    CHECK(d == doctest::Approx(-1.0));
}

// Same normalization in the other direction: strtod skips leading whitespace,
// from_chars does not.
TEST_CASE("leading whitespace is rejected for both integers and floats")
{
    int i = -1;
    double d = -1.0;

    CHECK_FALSE(util::from_string(" 12", i));
    CHECK_FALSE(util::from_string(" 1.5", d));
    CHECK_FALSE(util::from_string("\n1.5", d));

    CHECK(i == -1);
    CHECK(d == doctest::Approx(-1.0));
}

TEST_CASE("floating point parses what asset files contain")
{
    double value = 0.0;

    REQUIRE(util::from_string("0", value));
    CHECK(value == doctest::Approx(0.0));

    REQUIRE(util::from_string("32.5", value));
    CHECK(value == doctest::Approx(32.5));

    REQUIRE(util::from_string("-0.25", value));
    CHECK(value == doctest::Approx(-0.25));

    REQUIRE(util::from_string("1e3", value));
    CHECK(value == doctest::Approx(1000.0));

    REQUIRE(util::from_string(".5", value));
    CHECK(value == doctest::Approx(0.5));

    float single = 0.0f;
    REQUIRE(util::from_string("1.5", single));
    CHECK(single == doctest::Approx(1.5f));
}

TEST_CASE("malformed and out-of-range floating point fails")
{
    double value = -1.0;

    CHECK_FALSE(util::from_string("1.5x", value));
    CHECK_FALSE(util::from_string("1.2.3", value));
    CHECK_FALSE(util::from_string("", value));
    CHECK_FALSE(util::from_string(".", value));
    CHECK_FALSE(util::from_string("abc", value));

    // strtod reports overflow through errno, which is checked rather than
    // ignored -- otherwise this would silently be HUGE_VAL.
    CHECK_FALSE(util::from_string("1e400", value));
    CHECK_FALSE(util::from_string("-1e400", value));

    CHECK(value == doctest::Approx(-1.0));
}

// errno is a global. Clobbering it would make an unrelated later check see a
// failure that already happened.
TEST_CASE("a failed parse leaves errno as it found it")
{
    errno = EACCES;

    double value = 0.0;
    CHECK_FALSE(util::from_string("1e400", value));
    CHECK(errno == EACCES);

    CHECK(util::from_string("1.5", value));
    CHECK(errno == EACCES);

    errno = 0;
}

TEST_CASE("the std::string overload agrees with the const char* one")
{
    int value = 0;

    REQUIRE(util::from_string(std::string("42"), value));
    CHECK(value == 42);

    CHECK_FALSE(util::from_string(std::string("42px"), value));
    CHECK_FALSE(util::from_string(std::string(""), value));

    // An embedded NUL is trailing garbage, not a terminator, so this fails
    // rather than silently reading only the part before it.
    const std::string embedded("12\0"
                               "34",
                               5);
    REQUIRE(embedded.size() == 5);
    CHECK_FALSE(util::from_string(embedded, value));
    CHECK(value == 42);
}

TEST_CASE("null input fails rather than dereferencing")
{
    int value = -1;
    const char* text = nullptr;

    CHECK_FALSE(util::from_string(text, value));
    CHECK(value == -1);
}
