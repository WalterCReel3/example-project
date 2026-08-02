// Report formatting. Plain text, fixed columns, one finding per line — the
// output is meant to be diffed between a device run and a desktop one, so
// stability of layout matters more than prettiness.
//
// This writes its own output rather than going through util::log_*, for two
// reasons that only apply to a report: it must go to stdout AND to the file at
// once (log_open_file redirects, it does not tee, and on a handheld the file is
// the copy you get to keep), and it is not levelled — every line here is the
// product, so there is nothing to filter.

#include "diag.hpp"

#include <cstdarg>
#include <cstdio>

namespace diag
{

namespace
{

int failures = 0;
std::FILE* out_file = nullptr;

void emit(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

void emit(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    std::vfprintf(stdout, fmt, args);
    va_end(args);
    std::fputc('\n', stdout);

    if (out_file) {
        va_start(args, fmt);
        std::vfprintf(out_file, fmt, args);
        va_end(args);
        std::fputc('\n', out_file);
    }
}

const char* verdict_text(Verdict verdict)
{
    switch (verdict) {
    case Verdict::Ok:
        return "OK";
    case Verdict::Ignored:
        return "IGNORED";
    case Verdict::Wrong:
        return "WRONG";
    case Verdict::Failed:
        return "FAILED";
    case Verdict::Unsupported:
        return "UNSUPPORTED";
    case Verdict::Skipped:
        return "SKIPPED";
    case Verdict::Info:
        return "info";
    }
    return "?";
}

} // namespace

void report_open(const char* path)
{
    // Unbuffered: a check that hangs the device — and drawing to a panel
    // through a vendor blitter is a place where that can happen — should still
    // leave everything up to that point on disk.
    std::setvbuf(stdout, nullptr, _IOLBF, 0);

    if (path && *path) {
        out_file = std::fopen(path, "w");
        if (!out_file) {
            std::fprintf(stderr,
                         "warning: cannot write '%s'; stdout only\n", path);
        } else {
            std::setvbuf(out_file, nullptr, _IOLBF, 0);
        }
    }
}

void report_close()
{
    if (out_file) {
        std::fclose(out_file);
        out_file = nullptr;
    }
}

void blank()
{
    emit("%s", "");
}

void section(const char* title)
{
    blank();
    emit("== %s ==", title);
}

void field(const char* key, const std::string& value)
{
    emit("  %-24s %s", key, value.c_str());
}

void field(const char* key, long value)
{
    emit("  %-24s %ld", key, value);
}

void check(const char* name, Verdict verdict, const std::string& detail)
{
    if (verdict == Verdict::Failed) {
        ++failures;
    }
    emit("  %-24s %-12s %s", name, verdict_text(verdict), detail.c_str());
}

void note(const std::string& text)
{
    emit("%s", text.c_str());
}

int report_exit_code()
{
    return failures == 0 ? 0 : 1;
}

} // namespace diag
