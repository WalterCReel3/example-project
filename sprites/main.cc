// sprites — entry point.
//
// Fullscreen by default, like coppers and for the same reason: a handheld has
// no window manager, and taking over the panel is the presentation rather than
// an oversight. --windowed is the dev-box mode.

#include <cstdio>
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
        "usage: sprites [options]\n"
        "\n"
        "  --windowed                run in a window instead of fullscreen\n"
        "  --size <w> <h>            window size (default 640 480)\n"
        "  --fps <n>                 frame cap, 0 for uncapped\n"
        "  --seconds <n>             exit after n seconds; 0 runs until quit\n"
        "  --software                force the software driver\n"
        "  --tilemap                 draw data/sunnyland.tmx behind the\n"
        "                            sprites, and report the fill rate\n"
        "  --no-scroll               hold the tilemap camera still\n"
        "  --tile-scale <n>          draw tiles at n x size\n"
        "  --screenshot <path.bmp>   render a few frames, save, exit\n"
        "  --frames <n>              frames to render before a screenshot\n"
        "  -h, --help                this text\n"
        "\n"
        "keys: Left/Right or A change the large sprite's animation,\n"
        "      Start or Esc quit\n");
}

// Names the offending argument rather than silently taking a default: a
// mistyped --fps that quietly became 0 looks like the option having no effect.
bool parse_int(const char* text, int& out, const char* option)
{
    if (!util::from_string(text, out)) {
        std::printf("sprites: %s expects an integer, got '%s'\n", option, text);
        return false;
    }
    return true;
}

std::string log_path()
{
    const std::string pref = rig::pref_path("sprites");
    if (pref.empty()) {
        return std::string();
    }
    return pref + "sprites.log";
}

} // namespace

int main(int argc, char** argv)
{
    sprites::Options options;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--windowed") {
            options.fullscreen = false;
        } else if (arg == "--software") {
            options.software = true;
        } else if (arg == "--tilemap") {
            options.tilemap = true;
        } else if (arg == "--no-scroll") {
            options.scroll = false;
        } else if (arg == "--tile-scale" && i + 1 < argc) {
            if (!parse_int(argv[++i], options.tile_scale, "--tile-scale")) {
                return 2;
            }
        } else if (arg == "--size" && i + 2 < argc) {
            if (!parse_int(argv[++i], options.width, "--size") ||
                !parse_int(argv[++i], options.height, "--size")) {
                return 2;
            }
        } else if (arg == "--fps" && i + 1 < argc) {
            if (!parse_int(argv[++i], options.target_fps, "--fps")) {
                return 2;
            }
        } else if (arg == "--frames" && i + 1 < argc) {
            if (!parse_int(argv[++i], options.screenshot_frames, "--frames")) {
                return 2;
            }
        } else if (arg == "--seconds" && i + 1 < argc) {
            if (!util::from_string(argv[++i], options.seconds)) {
                std::printf("sprites: --seconds expects a number, got '%s'\n",
                            argv[i]);
                return 2;
            }
        } else if (arg == "--screenshot" && i + 1 < argc) {
            options.screenshot = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            usage();
            return 0;
        } else {
            std::printf("sprites: unrecognised argument '%s'\n", arg.c_str());
            usage();
            return 2;
        }
    }

    // A screenshot run must not take over the display: it is meant to be usable
    // over SSH, and on a dev box it should not steal focus.
    if (!options.screenshot.empty()) {
        options.fullscreen = false;
    }

    const std::string path = log_path();
    if (!path.empty()) {
        util::log_open_file(path.c_str());
    }
    util::log_set_level(util::LogInfo);

    int status = 0;
    try {
        sprites::Demo demo(options);
        if (options.screenshot.empty()) {
            demo.run();
        } else if (!demo.render_to_file(options.screenshot,
                                        options.screenshot_frames)) {
            status = 1;
        }
    } catch (const std::exception& e) {
        util::log_error("sprites: %s", e.what());
        std::printf("sprites: %s\n", e.what());
        status = 1;
    }

    util::log_close_file();
    return status;
}
