#ifndef WREEL_UTIL_FORMAT_HPP
#define WREEL_UTIL_FORMAT_HPP

#include <cstdarg>
#include <cstdio>
#include <string>

namespace util
{

// printf-style formatting into a std::string.
//
// Used on the error paths of util/posix/fileimpl.cc and util/mswin/fileimpl.cc,
// so a bug here corrupts exception messages — which is how it went unnoticed.
//
// The original had three defects:
//   1. It reused `args` for the second vsnprintf. A va_list is consumed by
//      vsnprintf, so reading it again is undefined behaviour; on x86-64 it
//      typically yielded garbage or a crash.
//   2. No va_end, so the list was never released.
//   3. It sized the buffer at `sz` and passed `sz` as the limit, but vsnprintf
//      writes at most limit-1 characters plus a NUL — silently truncating the
//      last character of every message.
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
    std::vsnprintf(&buffer[0], buffer.size(), fmt, args);
    va_end(args);

    buffer.resize(static_cast<std::size_t>(sz));
    return buffer;
}

} // namespace util

#endif
