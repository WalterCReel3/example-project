// wreel-diag — draw known content on this device and report what came back.
//
//   wreel-diag                       report to stdout
//   wreel-diag --out diag.txt        and to a file, for pulling off an SD card
//   wreel-diag --keep                leave the last frame up and pause
//
// The exit code is 0 unless a call outright failed. IGNORED and WRONG findings
// do not set it: on the Miyoo Mini they are the expected result, and a tool
// that exited non-zero for them could never be run unattended on the one target
// it was written for.

#include "diag.hpp"

#include <SDL.h>

#include <cstdio>
#include <cstring>

#include <util/format.hpp>

namespace
{

// Deciding how big the window should be, which on this device is a real
// problem rather than a formality.
//
// Corrected after the first device run. The order used to be "desktop mode,
// then the first entry in the mode list", and it produced an 800x600 window on
// a 640x480 panel: SDL_GetDesktopDisplayMode returns SUCCESS with a zeroed
// mode (D22/D24), and the driver's mode list is a fixed set of ten that SDL
// sorts largest-first — so the first entry is a mode this panel does not have.
// Every full-size texture was then refused by a renderer capped at 640x480, and
// the run produced three FAILED lines and no findings.
//
// gfx::renderer::Context solves the same problem by probing four sources; this
// tool does not link gfx on purpose, so it has to do its own probing — and the
// framebuffer device is a better answer than anything SDL offers here, because
// it is the panel rather than a claim about it.
void choose_size(int* width, int* height)
{
    SDL_DisplayMode mode;

    if (SDL_GetDesktopDisplayMode(0, &mode) == 0 && mode.w > 0 && mode.h > 0) {
        *width = mode.w;
        *height = mode.h;
        diag::field("window size from", "desktop display mode");
        return;
    }

    if (diag::framebuffer_size(width, height)) {
        diag::field("window size from",
                    "/dev/fb0 — no desktop mode was reported");
        return;
    }

    // Still consulted, but clamped: the render driver advertises a maximum
    // texture size, and a window larger than it cannot have a full-screen
    // texture drawn into it whatever the mode list says.
    if (SDL_GetNumDisplayModes(0) > 0 && SDL_GetDisplayMode(0, 0, &mode) == 0 &&
        mode.w > 0 && mode.h > 0) {
        *width = mode.w;
        *height = mode.h;

        SDL_RendererInfo info;
        if (SDL_GetRenderDriverInfo(0, &info) == 0 &&
            info.max_texture_width > 0 && info.max_texture_height > 0 &&
            (*width > info.max_texture_width ||
             *height > info.max_texture_height)) {
            *width = info.max_texture_width;
            *height = info.max_texture_height;
            diag::field("window size from",
                        util::format("first display mode %dx%d, clamped to the "
                                     "render driver's %dx%d texture limit",
                                     mode.w, mode.h, *width, *height));
            return;
        }

        diag::field("window size from", "first display mode (no desktop mode)");
        return;
    }

    *width = 640;
    *height = 480;
    diag::field("window size from", "fallback 640x480 — the device reported no "
                                    "usable mode at all");
}

} // namespace

int main(int argc, char** argv)
{
    const char* out = nullptr;
    bool keep = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            out = argv[++i];
        } else if (std::strcmp(argv[i], "--keep") == 0) {
            keep = true;
        } else {
            std::fprintf(stderr, "usage: %s [--out FILE] [--keep]\n", argv[0]);
            return 2;
        }
    }

    diag::report_open(out);
    diag::note(util::format("wreel-diag %s  [target: %s]", WREEL_VERSION,
                            WREEL_TARGET_ID));

    diag::report_environment();

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        diag::check("SDL_Init(VIDEO)", diag::Verdict::Failed, SDL_GetError());
        diag::report_close();
        return 1;
    }

    diag::report_sdl_capability();

    diag::section("Window and renderer");

    int width = 0;
    int height = 0;
    choose_size(&width, &height);
    diag::field("window size", util::format("%dx%d", width, height));

    // Fullscreen, because that is what this project's presentation is and
    // because a windowed request on this driver produces a scale factor from
    // integer division that is 0 for anything wider than the panel.
    SDL_Window* window = SDL_CreateWindow(
        "wreel-diag", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width,
        height, SDL_WINDOW_FULLSCREEN);
    if (!window) {
        diag::check("SDL_CreateWindow", diag::Verdict::Failed, SDL_GetError());
        SDL_Quit();
        diag::report_close();
        return 1;
    }
    diag::set_window(window);

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer) {
        diag::check("SDL_CreateRenderer", diag::Verdict::Failed, SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        diag::report_close();
        return 1;
    }

    SDL_RendererInfo info;
    if (SDL_GetRendererInfo(renderer, &info) == 0) {
        diag::field("renderer selected", info.name);
    }

    int out_w = 0;
    int out_h = 0;
    if (SDL_GetRendererOutputSize(renderer, &out_w, &out_h) == 0) {
        // Reported for its own sake: this driver answers 0x0 and returns
        // success, which is why gfx::renderer::Context probes four sources for
        // its output size instead of trusting this one.
        diag::field("renderer output size",
                    util::format("%dx%d%s", out_w, out_h,
                                 (out_w <= 0 || out_h <= 0)
                                     ? "  <- degenerate, and reported as success"
                                     : ""));
    }

    // Orientation first: the geometric checks below are read through it.
    const diag::Transform transform = diag::detect_transform(renderer);

    // First, because every check below it uploads a texture and none of their
    // results mean anything if uploads do not behave.
    diag::check_texture_upload_copies(renderer);

    diag::check_clear(renderer);
    diag::check_full_copy(renderer, transform);
    diag::check_sub_rect_copy(renderer, transform);
    diag::check_narrow_sub_rect(renderer);
    diag::check_dest_placement(renderer, transform);
    diag::check_blend_mode(renderer);
    diag::check_colour_and_alpha_mod(renderer);
    diag::check_fill_rect(renderer);

    diag::check_texture_limits(renderer);
    diag::check_render_target(renderer);

    diag::check_audio();

    if (keep) {
        diag::blank();
        diag::note("--keep: holding the last frame for 10s");
        SDL_Delay(10000);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    diag::section("Result");
    diag::note("  Findings above are the device's behaviour, not a verdict on "
               "it. Diff this");
    diag::note("  against a desktop-software run: OK there and IGNORED here is "
               "a gap in the");
    diag::note("  driver. IGNORED in both is a mistake in the check.");

    const int code = diag::report_exit_code();
    diag::report_close();
    return code;
}
