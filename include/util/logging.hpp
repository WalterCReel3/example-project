#pragma once

#include <cstdarg>

//============================================================================
//
// Logging
//
// printf-style rather than stream-style, and deliberately free of <iostream>.
// On armv7 the iostream machinery costs about 596 KB statically, which is 1.6x
// the entire <cstdio>-only C++ runtime floor. Nothing that ships may include
// it. See docs/TARGETS.md § "No iostreams in shipped code".
//
// Format strings are checked at compile time: each function carries
// __attribute__((format(printf, ...))) and the project builds with -Wformat=2,
// so a mismatched conversion is a diagnostic rather than undefined behaviour.
// That is what makes printf-style acceptable here rather than a downgrade.
//
//     util::log_debug("loaded %s: %dx%d", path, w, h);
//     util::log_error("could not open %s: %s", path, SDL_GetError());
//
// Each function tests the level before formatting, so a suppressed message
// costs a call and a comparison. Its *arguments* are still evaluated, because
// they are function arguments — so avoid putting real work in them:
//
//     util::log_debug("%s", expensive().c_str());   // runs even when silent
//
// Threading: a message is assembled in full and written with one fwrite, and C
// and POSIX stdio lock the FILE per call, so concurrent writers cannot splice
// one message into another. Nothing in this project creates a thread today; the
// guarantee is here because an SDL_mixer or RtMidi callback is a plausible
// future caller and spliced log lines are miserable to diagnose.
//
// Not covered: log_set_level(), log_open_file() and log_close_file() race with
// concurrent logging. Configure at startup, before any thread exists. A message
// longer than 1024 bytes including its prefix is truncated and marked "...".
//
//============================================================================
namespace util
{

enum LogLevel { LogError = 0, LogWarning = 1, LogInfo = 2, LogDebug = 3 };

// Messages at or below the current level are emitted. Defaults to LogError.
void log_set_level(LogLevel level);
LogLevel log_level();
bool log_enabled(LogLevel level);

// Output goes to stderr until a file is opened, and returns there when it is
// closed. Returns false if the file could not be opened, in which case stderr
// remains the sink — logging never becomes a source of errors itself.
bool log_open_file(const char* path);
void log_close_file();

void log_error(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void log_warning(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void log_info(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void log_debug(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

// For forwarding an already-collected va_list; the four above are the interface
// to use directly.
void log_write_v(LogLevel level, const char* fmt, va_list args);

} // namespace util
