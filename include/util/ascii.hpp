#pragma once

#include <type_traits>

//============================================================================
//
// ASCII character classification
//
// Locale-independent, constexpr, and defined for every input. The names and
// semantics follow P3688 "ASCII character utilities", the C++26 paper that
// standardises an <ascii> header for exactly this job:
//
//     https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2026/p3688r6.html
//
// Do not reach for <cctype> in a parser. ::isspace and friends are
// locale-dependent, take int, and are undefined for a negative char — and char
// is signed on the x86-64 dev box but *unsigned* on both ARM targets, so the
// same asset byte takes a different path on the dev box than on any device.
// SDL_isspace is not an escape either: SDL forwards it to ::isspace whenever
// HAVE_CTYPE_H is set, which is every target here.
//
// Two deliberate deviations from P3688:
//
//   * These are constexpr callable objects rather than free function templates,
//     so they can be handed straight to <algorithm> the way the tokenizers in
//     util/string.hpp do — a function template cannot be passed as a predicate.
//     This is the shape std::ranges uses for its niebloids, for the same
//     reason. The cost is that adopting <ascii> later is a thin shim rather
//     than a using-declaration.
//   * signed char and unsigned char are accepted. P3688 excludes them as
//     integer types, but util::File hands out byte buffers and a parser
//     iterating one should not have to cast.
//
// char8_t is absent because it is C++20, and GCC 8.3 sets this project's floor.
// P3688's ascii_is_digit(c, base) overload is also absent: it carries a
// precondition on base, nothing here needs it yet, and every predicate below is
// total.
//
//============================================================================
namespace util
{
namespace detail
{

// Widened to one unsigned type so every predicate below compares against the
// same thing, and so a negative char cannot survive into a comparison. Compiles
// away; on ARM, where char is already unsigned, it is a no-op.
typedef unsigned long ascii_code;

// A documented concept requirement, not an enforced one: C++17 has no concept
// keyword, so this is the traits-and-static_assert form the rest of the tree
// uses (see util::tokenizer_traits). Failing it produces one readable message
// instead of a template error several frames deep.
template<typename T>
struct is_ascii_character : std::false_type {
};

template<>
struct is_ascii_character<char> : std::true_type {
};
template<>
struct is_ascii_character<signed char> : std::true_type {
};
template<>
struct is_ascii_character<unsigned char> : std::true_type {
};
template<>
struct is_ascii_character<wchar_t> : std::true_type {
};
template<>
struct is_ascii_character<char16_t> : std::true_type {
};
template<>
struct is_ascii_character<char32_t> : std::true_type {
};

template<typename CharT>
constexpr ascii_code to_ascii_code(CharT c) noexcept
{
    static_assert(is_ascii_character<CharT>::value,
                  "util::ascii_* takes a character type: char, signed char, "
                  "unsigned char, wchar_t, char16_t or char32_t. Integer types "
                  "are rejected deliberately — passing an int is usually a "
                  "sign that a char was promoted somewhere it should not have "
                  "been.");
    return static_cast<ascii_code>(
        static_cast<typename std::make_unsigned<CharT>::type>(c));
}

} // namespace detail

// Generates the pair a predicate needs: an empty callable type, and one
// constexpr object of it. Macro-generated for the same reason posix/errors.hpp
// generates its exception types — sixteen hand-written copies of the same four
// lines is sixteen chances to typo one. Every body sees the widened code as
// `u`.
#define WREEL_ASCII_PREDICATE(name, expr)                          \
    struct name##_fn {                                             \
        template<typename CharT>                                   \
        constexpr bool operator()(CharT c) const noexcept          \
        {                                                          \
            const detail::ascii_code u = detail::to_ascii_code(c); \
            return (expr);                                         \
        }                                                          \
    };                                                             \
    inline constexpr name##_fn name {}

// Every body is written against the widened `u` rather than calling the other
// predicates, which keeps each one self-contained and independent of
// declaration order. Composing them instead would mean feeding `u` — an
// unsigned long by then — back into to_ascii_code(), which rejects integer
// types on purpose.

#define WREEL_ASCII_DIGIT (u >= '0' && u <= '9')
#define WREEL_ASCII_LOWER (u >= 'a' && u <= 'z')
#define WREEL_ASCII_UPPER (u >= 'A' && u <= 'Z')
#define WREEL_ASCII_ALNUM \
    (WREEL_ASCII_DIGIT || WREEL_ASCII_LOWER || WREEL_ASCII_UPPER)

WREEL_ASCII_PREDICATE(ascii_is_any, u <= 0x7F);
WREEL_ASCII_PREDICATE(ascii_is_control, u <= 0x1F || u == 0x7F);

// ' ', '\t', '\n', '\v', '\f', '\r' — the six the C locale calls space, which
// is what an OBJ or JSON file means by whitespace regardless of environment.
WREEL_ASCII_PREDICATE(ascii_is_whitespace,
                      u == ' ' || (u >= '\t' && u <= '\r'));
WREEL_ASCII_PREDICATE(ascii_is_horizontal_whitespace, u == ' ' || u == '\t');

WREEL_ASCII_PREDICATE(ascii_is_digit, WREEL_ASCII_DIGIT);
WREEL_ASCII_PREDICATE(ascii_is_bit, u == '0' || u == '1');
WREEL_ASCII_PREDICATE(ascii_is_octal_digit, u >= '0' && u <= '7');
WREEL_ASCII_PREDICATE(ascii_is_hex_digit, WREEL_ASCII_DIGIT ||
                                              (u >= 'a' && u <= 'f') ||
                                              (u >= 'A' && u <= 'F'));

WREEL_ASCII_PREDICATE(ascii_is_lower, WREEL_ASCII_LOWER);
WREEL_ASCII_PREDICATE(ascii_is_upper, WREEL_ASCII_UPPER);
WREEL_ASCII_PREDICATE(ascii_is_alphabetic,
                      WREEL_ASCII_LOWER || WREEL_ASCII_UPPER);
WREEL_ASCII_PREDICATE(ascii_is_alphanumeric, WREEL_ASCII_ALNUM);

WREEL_ASCII_PREDICATE(ascii_is_printing, u >= ' ' && u <= 0x7E);
WREEL_ASCII_PREDICATE(ascii_is_graphic, u > ' ' && u <= 0x7E);
WREEL_ASCII_PREDICATE(ascii_is_punctuation,
                      (u > ' ' && u <= 0x7E) && !(WREEL_ASCII_ALNUM));

#undef WREEL_ASCII_ALNUM
#undef WREEL_ASCII_UPPER
#undef WREEL_ASCII_LOWER
#undef WREEL_ASCII_DIGIT
#undef WREEL_ASCII_PREDICATE

// The transformations return the character type they were given, so they
// compose with the predicates without a cast at the call site. Anything outside
// the ASCII letter ranges — including every byte above 0x7F — is returned
// unchanged rather than run through a locale's case table.
struct ascii_to_lower_fn {
    template<typename CharT>
    constexpr CharT operator()(CharT c) const noexcept
    {
        return ascii_is_upper(c) ? static_cast<CharT>(c + ('a' - 'A')) : c;
    }
};
inline constexpr ascii_to_lower_fn ascii_to_lower{};

struct ascii_to_upper_fn {
    template<typename CharT>
    constexpr CharT operator()(CharT c) const noexcept
    {
        return ascii_is_lower(c) ? static_cast<CharT>(c - ('a' - 'A')) : c;
    }
};
inline constexpr ascii_to_upper_fn ascii_to_upper{};

} // namespace util
