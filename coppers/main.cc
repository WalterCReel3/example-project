// coppers — entry point.
//
// Fullscreen is the default because that is the presentation, not an oversight:
// a handheld has no window manager. --windowed is the "emulator" mode for a dev
// box, which is an added option rather than a changed default.

#include <SDL.h>

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>

#include <rig/assets.hpp>
#include <util/logging.hpp>
#include <util/number.hpp>

#include "demo.hpp"

namespace
{

void usage()
{
    std::printf(
        "usage: coppers [options]\n"
        "\n"
        "  --windowed                run in a window instead of fullscreen\n"
        "  --layer-height <n>        plot at n scanlines and scale up;\n"
        "                            0 or omitted means match the display\n"
        "  --fps <n>                 frame cap, 0 for uncapped\n"
        "  --seconds <n>             exit after n seconds; 0 runs until quit\n"
        "  --no-hud                  hide the timing overlay\n"
        "  --cpu-scroller            start on the hand-written blitter\n"
        "  --screenshot <path.bmp>   render a few frames, save, exit\n"
        "  --frames <n>              frames to render before a screenshot\n"
        "  -h, --help                this text\n"
        "\n"
        "keys: A palette, Y hud, Esc/Q quit\n");
}

// Reports the offending argument rather than silently taking a default, because
// a mistyped --layer-height that quietly became 0 would look like the option
// having no effect.
bool parse_int(const char* text, int& out, const char* option)
{
    if (!util::from_string(text, out)) {
        std::printf("coppers: %s expects an integer, got '%s'\n", option, text);
        return false;
    }
    return true;
}

std::string log_path()
{
    const std::string pref = rig::pref_path("coppers");
    if (pref.empty()) {
        return std::string();
    }
    return pref + "coppers.log";
}

} // namespace

int main(int argc, char** argv)
{
    coppers::Options options;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--windowed") {
            options.fullscreen = false;
        } else if (arg == "--no-hud") {
            options.hud = false;
        } else if (arg == "--cpu-scroller") {
            options.cpu_scroller = true;
        } else if (arg == "--layer-height" && i + 1 < argc) {
            if (!parse_int(argv[++i], options.layer_height, "--layer-height")) {
                return 2;
            }
        } else if (arg == "--fps" && i + 1 < argc) {
            if (!parse_int(argv[++i], options.target_fps, "--fps")) {
                return 2;
            }
        } else if (arg == "--seconds" && i + 1 < argc) {
            if (!util::from_string(argv[++i], options.seconds)) {
                std::printf("coppers: --seconds expects a number, got '%s'\n",
                            argv[i]);
                return 2;
            }
        } else if (arg == "--frames" && i + 1 < argc) {
            if (!parse_int(argv[++i], options.screenshot_frames, "--frames")) {
                return 2;
            }
        } else if (arg == "--screenshot" && i + 1 < argc) {
            options.screenshot = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            usage();
            return 0;
        } else {
            std::printf("coppers: unrecognised argument '%s'\n", arg.c_str());
            usage();
            return 2;
        }
    }

    // A screenshot run must not also take over the display: it is meant to be
    // usable over SSH, and on a dev box it should not steal focus.
    if (!options.screenshot.empty()) {
        options.fullscreen = false;
    }

    // Before SDL_Init: pref_path needs no video subsystem, and a failure during
    // construction should already have somewhere to be logged.
    const std::string path = log_path();
    if (!path.empty()) {
        util::log_open_file(path.c_str());
    }
    util::log_set_level(util::LogInfo);

    int status = 0;
    try {
        coppers::Demo demo(options);
        if (options.screenshot.empty()) {
            demo.run();
        } else if (!demo.render_to_file(options.screenshot,
                                        options.screenshot_frames)) {
            status = 1;
        }
    } catch (const std::exception& e) {
        util::log_error("coppers: %s", e.what());
        status = 1;
    }

    util::log_close_file();
    return status;
}
