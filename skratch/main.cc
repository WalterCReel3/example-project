// skratch — entry point.
//
// The log goes to SDL_GetPrefPath rather than runlog.txt in the working
// directory. A handheld launches this from a read-only mount or from whatever
// directory the firmware's launcher happens to be in, so writing beside the
// binary either fails silently or litters the SD card. Flagged in
// planning/2026-07-25-packaging-distribution/.
//
// It also used its own `std::ofstream logging` global, which put <fstream> into
// a shipped executable — 596 KB of iostreams on armv7, against docs/TARGETS.md
// § 1a. That is now util::log_*, like everything else.

#include <SDL.h>

#include <rig/assets.hpp>
#include <util/logging.hpp>

#include <cstdio>
#include <exception>
#include <string>

#include "application.hpp"

namespace
{

// Returns the log path, and an empty string if there is no writable directory —
// in which case logging stays on stderr rather than failing.
std::string log_path()
{
    const std::string pref = rig::pref_path("skratch");
    if (pref.empty()) {
        return std::string();
    }
    return pref + "skratch.log";
}

} // namespace

// --screenshot <path> renders a couple of frames, writes the last to a BMP and
// exits. It is the only way to verify this renderer draws: there is no headless
// GL, so gfx::gles2 has no unit tests, and it also makes a device checkable
// over SSH where nobody can see the panel.
int main(int argc, char** argv)
{
    std::string screenshot;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--screenshot" && i + 1 < argc) {
            screenshot = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::printf("usage: skratch [--screenshot <path.bmp>]\n");
            return 0;
        } else {
            std::printf("skratch: unrecognised argument '%s'\n", arg.c_str());
            return 2;
        }
    }

    // Before SDL_Init: SDL_GetPrefPath does not need the video subsystem, and a
    // failure during construction should already be logged somewhere.
    const std::string path = log_path();
    if (!path.empty()) {
        util::log_open_file(path.c_str());
    }
    util::log_set_level(util::LogInfo);

    if (!path.empty()) {
        util::log_info("skratch starting; log at %s", path.c_str());
    }

    int status = 0;
    try {
        Application app;
        if (screenshot.empty()) {
            app.game_loop();
        } else if (!app.render_to_file(screenshot)) {
            status = 1;
        }
    } catch (const std::exception& e) {
        util::log_error("skratch: %s", e.what());
        status = 1;
    }

    util::log_close_file();
    return status;
}
