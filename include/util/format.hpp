#pragma once

#include <cstdarg>
#include <cstdio>
#include <string>

namespace util
{

// printf-style formatting into a std::string.
//
// Used on the error paths of util/posix/fileimpl.cc and util/mswin/fileimpl.cc,
// so a bug here corrupts exception messages rather than failing visibly.
inline std::string format(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    // A separate copy for measuring, since the measuring pass consumes it.
    va_list measure;
    va_copy(measure, args);
    const int sz = std::vsnprintf(nullptr, 0, fmt, measure);
    va_end(measure);

    if (sz < 0) {
        va_end(args);
        return std::string();
    }

    // +1 for the NUL vsnprintf always writes, then trimmed back off.
    std::string buffer(static_cast<std::size_t>(sz) + 1, '\0');
    std::vsnprintf(buffer.data(), buffer.size(), fmt, args);
    va_end(args);

    buffer.resize(static_cast<std::size_t>(sz));
    return buffer;
}

} // namespace util
