#pragma once

#include <type_traits>

//============================================================================
//
// Integer arithmetic the standard library does not provide
//
//     const int first = util::floor_div(-origin, tile);   // -1/16 == -1
//     const int rows  = util::ceil_div(height, tile);     // partial row counts
//
// These exist because the obvious spellings are wrong in ways that only show
// up at the edges, and the usual workaround is worse than either.
//
// C and C++ integer division truncates *toward zero*, so -1 / 16 is 0, not -1.
// Anything mapping a coordinate to a cell index needs floor semantics: with
// truncation, every position in the first cell left of the origin reports cell
// 0, and a tilemap scrolled one pixel past its left edge drops its first
// column.
//
// The tempting fix is std::floor on a double. That is what this replaces, and
// it is the wrong instinct on this project's targets: it converts to floating
// point and back for arithmetic that is exact in integers, on a Cortex-A7 where
// that is not free, and it silently loses precision above 2^53. It also reads
// as though something subtle is happening when nothing is.
//
// Written as function templates constrained to integral types rather than as
// overloads per width, so a caller cannot accidentally instantiate them on a
// double and get the behaviour this exists to avoid — SFINAE'd because C++17
// has no `concept` keyword. See CLAUDE.md § "On concept interfaces".
//
//============================================================================
namespace util
{

// The `enable_if` this file's signatures share: T must be a built-in integer.
// bool is excluded because dividing by a bool is never intended.
template<typename T>
struct is_integer
    : std::integral_constant<bool, std::is_integral<T>::value &&
                                       !std::is_same<T, bool>::value> {
};

// Division rounding toward negative infinity, so the result of mapping a
// coordinate to a cell index is continuous across zero.
//
//     floor_div( 33, 16) ==  2      floor_div(-1, 16) == -1
//     floor_div( 32, 16) ==  2      floor_div(-16, 16) == -1
//     floor_div(  0, 16) ==  0      floor_div(-17, 16) == -2
//
// Undefined for b == 0, like the built-in operator, and for the one overflow
// case the built-in has (the most negative value divided by -1).
template<typename T>
constexpr typename std::enable_if<is_integer<T>::value, T>::type floor_div(T a,
                                                                           T b)
{
    const T quotient = a / b;
    const T remainder = a % b;

    // Truncation rounded toward zero; correct it only when there was a
    // remainder and the operands disagreed in sign, which is exactly when
    // truncation overshot.
    return quotient - ((remainder != 0) && ((a < 0) != (b < 0)) ? 1 : 0);
}

// The matching modulo: always in [0, |b|), so it pairs with floor_div the way
// remainder pairs with truncating division.
//
//     floor_mod(-1, 16) == 15,   where -1 % 16 == -1
template<typename T>
constexpr typename std::enable_if<is_integer<T>::value, T>::type floor_mod(T a,
                                                                           T b)
{
    return a - floor_div(a, b) * b;
}

// Division rounding away from zero for positive results — "how many cells does
// this span cover", where a partial cell still counts as one.
//
//     ceil_div(32, 16) == 2      ceil_div(33, 16) == 3
template<typename T>
constexpr typename std::enable_if<is_integer<T>::value, T>::type ceil_div(T a,
                                                                          T b)
{
    const T quotient = a / b;
    const T remainder = a % b;

    return quotient + ((remainder != 0) && ((a < 0) == (b < 0)) ? 1 : 0);
}

} // namespace util
