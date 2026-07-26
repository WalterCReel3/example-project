#pragma once

#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <string>
#include <system_error>
#include <type_traits>

#include <util/ascii.hpp>

//============================================================================
//
// Text to number
//
// Strict, whole-string conversion for the numbers that arrive in asset and
// config files: XML attributes, TMX CSV layer data, JSON scalars, OBJ vertex
// components, MIDI mapping values.
//
//     int width = 0;
//     if (!util::from_string(text, width)) {
//         throw ParseError(...);   // the caller decides what failure means
//     }
//
// "Strict" means the entire input must be the number and nothing else. All of
// these fail rather than returning a partial result:
//
//     ""      "abc"     "12px"    "1.5" -> int     " 12"    "12 "
//     "+12"   "0x1f"    "1e400"   "2147483648" -> int
//
// A leading '+' and leading whitespace are rejected for every type,
// deliberately. std::from_chars rejects both; strtod accepts both. Normalizing
// here means the contract does not change depending on which type a caller
// instantiates, which is the kind of divergence that makes a parser behave
// differently for x than for y.
//
// Why not the obvious alternatives:
//
//   - `std::stoi` / `std::stod` throw for control flow, ignore trailing garbage
//     ("12px" is 12), and are locale-dependent.
//   - `atoi` reports failure as 0, which is also a valid value.
//   - `std::from_chars` alone would be enough for integers, but the
//     floating-point overload needs GCC 11 and the compiler floor here is
//     GCC 8.3. See docs/TARGETS.md § 1.
//
// One live constraint: the floating-point path uses the strtod family, which
// reads the decimal separator from LC_NUMERIC. Nothing in this tree calls
// setlocale, so that is '.' today, which is what every format above writes.
// Enabling LC_NUMERIC later would change what these functions accept — the same
// latent locale coupling that made the <ctype.h> predicates a defect (D10).
//
//============================================================================
namespace util
{

namespace detail
{

// Which conversion family a type uses. Integers go through std::from_chars,
// which covers every width; floating-point types go through the strtod family,
// which is spelled once per type.
struct integer_conversion {
};
struct floating_conversion {
};

// A documented concept requirement rather than an enforced one — there is no
// `concept` keyword at C++17. Specialize this to teach from_string a new
// numeric family; nothing else has to change. See docs/TARGETS.md § 1 on why
// the codebase expresses compile-time interfaces this way.
template<typename T>
struct number_traits {
    typedef
        typename std::conditional<std::is_integral<T>::value,
                                  integer_conversion, floating_conversion>::type
            conversion_type;
};

// The strtod family, selected by overload rather than by a chain of tests. The
// unused value parameter is the tag: it carries only the type.
inline float call_strtox(const char* text, char** end, float)
{
    return std::strtof(text, end);
}
inline double call_strtox(const char* text, char** end, double)
{
    return std::strtod(text, end);
}
inline long double call_strtox(const char* text, char** end, long double)
{
    return std::strtold(text, end);
}

template<typename T>
bool convert(const char* text, T& out, integer_conversion)
{
    const char* const last = text + std::strlen(text);
    T value{};
    const std::from_chars_result result = std::from_chars(text, last, value);
    // ptr != last catches trailing garbage; ec catches malformed input and
    // values too large for T.
    if (result.ec != std::errc() || result.ptr != last) {
        return false;
    }
    out = value;
    return true;
}

template<typename T>
bool convert(const char* text, T& out, floating_conversion)
{
    char* end = nullptr;

    // The strtod family reports overflow through errno and does not clear it
    // first, so it has to be cleared here — and restored, because errno is
    // global state a caller may be mid-way through inspecting.
    const int saved = errno;
    errno = 0;

    const T value = call_strtox(text, &end, T());

    const bool out_of_range = (errno == ERANGE);
    errno = saved;

    if (end == text || *end != '\0' || out_of_range) {
        return false;
    }
    out = value;
    return true;
}

} // namespace detail

// True on success, in which case `out` holds the value. False otherwise, and
// `out` is left untouched — so a caller that has already defaulted it does not
// need to restore it.
template<typename T>
bool from_string(const char* text, T& out)
{
    static_assert(
        !std::is_same<T, bool>::value,
        "from_string does not parse bools: the accepted spellings are "
        "a property of the format, not of the number, so spell them at "
        "the format boundary");
    static_assert(std::is_integral<T>::value ||
                      std::is_floating_point<T>::value,
                  "from_string converts integers and floating-point types");

    if (!text || !*text) {
        return false;
    }
    // Rejected here, before dispatch, so that every family agrees.
    if (*text == '+' || util::ascii_is_whitespace(*text)) {
        return false;
    }

    typedef typename detail::number_traits<T>::conversion_type conversion;
    return detail::convert(text, out, conversion());
}

// std::string overload. Uses c_str(), so a string with an embedded NUL is
// rejected rather than silently truncated at it.
template<typename T>
bool from_string(const std::string& text, T& out)
{
    if (text.size() != std::strlen(text.c_str())) {
        return false;
    }
    return from_string(text.c_str(), out);
}

} // namespace util
