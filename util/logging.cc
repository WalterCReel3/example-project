#include <util/logging.hpp>

#include <cstddef>
#include <cstdio>

namespace util
{

namespace
{

// One message, prefix and newline included. Sized to hold a long asset path
// plus a diagnostic from SDL comfortably; anything longer is truncated rather
// than split across writes.
const std::size_t message_max = 1024;

LogLevel current_level = LogError;

// Null means "use stderr", resolved at write time rather than stored, so no
// static initialisation order question arises.
std::FILE* output_file = nullptr;

std::FILE* sink()
{
    return output_file ? output_file : stderr;
}

const char* level_tag(LogLevel level)
{
    switch (level) {
    case LogError:
        return "E";
    case LogWarning:
        return "W";
    case LogInfo:
        return "I";
    case LogDebug:
        return "D";
    }
    return "?";
}

} // namespace

void log_set_level(LogLevel level)
{
    current_level = level;
}

LogLevel log_level()
{
    return current_level;
}

bool log_enabled(LogLevel level)
{
    return level <= current_level;
}

bool log_open_file(const char* path)
{
    log_close_file();
    if (!path) {
        return false;
    }
    // Deliberately not util::File: that reports failure by throwing, and a
    // logger must not become a source of exceptions on an error path.
    std::FILE* opened = std::fopen(path, "w");
    if (!opened) {
        return false;
    }
    output_file = opened;
    return true;
}

void log_close_file()
{
    if (output_file) {
        std::fclose(output_file);
        output_file = nullptr;
    }
}

void log_write_v(LogLevel level, const char* fmt, va_list args)
{
    if (!log_enabled(level)) {
        return;
    }
    // Assembled in full and written with a single fwrite, because C and POSIX
    // stdio lock the FILE per call: one call per message means a message cannot
    // interleave with another writer's. Three calls — prefix, body, newline —
    // could, producing spliced lines. The cost of that guarantee is the fixed
    // buffer below, hence a truncation limit.
    char buffer[message_max];

    const int prefixed =
        std::snprintf(buffer, sizeof buffer, "[%s] ", level_tag(level));
    if (prefixed < 0) {
        return;
    }
    std::size_t used = static_cast<std::size_t>(prefixed);

    const int written =
        std::vsnprintf(buffer + used, sizeof buffer - used, fmt, args);
    if (written < 0) {
        return;
    }
    // vsnprintf reports what it *would* have written, so this can exceed the
    // buffer.
    used += static_cast<std::size_t>(written);

    const std::size_t limit = sizeof buffer - 1; // leave room for the newline
    if (used > limit) {
        used = limit;
        // Mark the cut, so a truncated line is not mistaken for a complete one.
        buffer[used - 3] = '.';
        buffer[used - 2] = '.';
        buffer[used - 1] = '.';
    }
    buffer[used++] = '\n';

    std::FILE* out = sink();
    std::fwrite(buffer, 1, used, out);
    // Errors are the one level worth a flush, so a crash immediately afterwards
    // still leaves the reason on disk.
    if (level == LogError) {
        std::fflush(out);
    }
}

void log_error(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_write_v(LogError, fmt, args);
    va_end(args);
}

void log_warning(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_write_v(LogWarning, fmt, args);
    va_end(args);
}

void log_info(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_write_v(LogInfo, fmt, args);
    va_end(args);
}

void log_debug(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_write_v(LogDebug, fmt, args);
    va_end(args);
}

} // namespace util
