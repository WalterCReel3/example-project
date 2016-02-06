#ifndef __WREEL_UTILS_HPP
#define __WREEL_UTILS_HPP

#include <cstdarg>
#include <cstdio>
#include <string>

namespace util
{

inline std::string
format(const char *fmt, ...)
{
    using namespace std;
    va_list args;
    va_start(args, fmt);
    int sz = vsnprintf(0, 0, fmt, args);
    string buffer(sz, '\0');
    vsnprintf(&(buffer[0]), sz, fmt, args);
    return string(buffer);
}

} // namespace util

// vim: set sts=2 sw=2 expandtab:
#endif
