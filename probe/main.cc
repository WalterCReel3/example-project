// wreel-probe — report what this device's SDL2 stack actually offers.
//
// Bringing up five device classes (Miyoo Mini, RK3326, H700, desktop, Steam)
// otherwise means guessing at capabilities. Run this on the device and let the
// firmware answer.
//
// Exits non-zero if SDL cannot initialise video at all, so it doubles as a
// smoke test under CTest.

#include <SDL.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <audio/device.hpp>

#if defined(WREEL_PROBE_GL)
#include <SDL_opengl.h>
#endif

namespace
{

void heading(const char* title)
{
    std::printf("\n== %s ==\n", title);
}

void field(const char* key, const std::string& value)
{
    std::printf("  %-22s %s\n", key, value.c_str());
}

void field_int(const char* key, int value)
{
    std::printf("  %-22s %d\n", key, value);
}

// Drivers SDL was *compiled* with, independent of what will actually work.
void report_drivers()
{
    heading("Drivers compiled in");

    const int video_count = SDL_GetNumVideoDrivers();
    std::string video;
    for (int i = 0; i < video_count; ++i) {
        if (i > 0) {
            video += ", ";
        }
        video += SDL_GetVideoDriver(i);
    }
    field("video", video.empty() ? "(none)" : video);

    const int audio_count = SDL_GetNumAudioDrivers();
    std::string audio;
    for (int i = 0; i < audio_count; ++i) {
        if (i > 0) {
            audio += ", ";
        }
        audio += SDL_GetAudioDriver(i);
    }
    field("audio", audio.empty() ? "(none)" : audio);

    const char* current = SDL_GetCurrentVideoDriver();
    field("video in use", current ? current : "(none)");
}

// Which SDL_Renderer backends exist, and whether each is hardware accelerated.
// On a Miyoo Mini this should show the software renderer and nothing else.
void report_renderers()
{
    heading("Render backends");

    const int count = SDL_GetNumRenderDrivers();
    if (count <= 0) {
        field("available", "(none)");
        return;
    }

    for (int i = 0; i < count; ++i) {
        SDL_RendererInfo info;
        if (SDL_GetRenderDriverInfo(i, &info) != 0) {
            continue;
        }

        std::string flags;
        if (info.flags & SDL_RENDERER_SOFTWARE) {
            flags += "software ";
        }
        if (info.flags & SDL_RENDERER_ACCELERATED) {
            flags += "accelerated ";
        }
        if (info.flags & SDL_RENDERER_PRESENTVSYNC) {
            flags += "vsync ";
        }
        if (info.flags & SDL_RENDERER_TARGETTEXTURE) {
            flags += "target-texture ";
        }

        std::printf("  [%d] %-12s %s\n", i, info.name, flags.c_str());
        std::printf("       max texture %dx%d, %u formats\n",
                    info.max_texture_width, info.max_texture_height,
                    info.num_texture_formats);
    }
}

void report_displays()
{
    heading("Displays");

    const int displays = SDL_GetNumVideoDisplays();
    if (displays < 1) {
        field("count", "(none detected)");
        return;
    }
    field_int("count", displays);

    for (int d = 0; d < displays; ++d) {
        const char* name = SDL_GetDisplayName(d);
        std::printf("  display %d: %s\n", d, name ? name : "(unnamed)");

        SDL_DisplayMode desktop;
        if (SDL_GetDesktopDisplayMode(d, &desktop) == 0) {
            std::printf("    desktop mode  %dx%d @ %dHz  (%s)\n", desktop.w,
                        desktop.h, desktop.refresh_rate,
                        SDL_GetPixelFormatName(desktop.format));
        }

        const int modes = SDL_GetNumDisplayModes(d);
        std::printf("    %d mode(s)\n", modes);
        // Handhelds typically expose one or two; cap the listing regardless.
        const int limit = modes < 8 ? modes : 8;
        for (int m = 0; m < limit; ++m) {
            SDL_DisplayMode mode;
            if (SDL_GetDisplayMode(d, m, &mode) == 0) {
                std::printf("      %dx%d @ %dHz\n", mode.w, mode.h,
                            mode.refresh_rate);
            }
        }
        if (modes > limit) {
            std::printf("      ... %d more\n", modes - limit);
        }
    }
}

void report_input()
{
    heading("Input");

    field_int("joysticks", SDL_NumJoysticks());

    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        const char* name = SDL_JoystickNameForIndex(i);
        const bool is_gamepad = SDL_IsGameController(i) == SDL_TRUE;
        std::printf("  [%d] %-28s %s\n", i, name ? name : "(unnamed)",
                    is_gamepad ? "(gamepad mapping present)"
                               : "(raw joystick)");

        SDL_Joystick* stick = SDL_JoystickOpen(i);
        if (stick) {
            std::printf("       %d axes, %d buttons, %d hats\n",
                        SDL_JoystickNumAxes(stick),
                        SDL_JoystickNumButtons(stick),
                        SDL_JoystickNumHats(stick));
            SDL_JoystickClose(stick);
        }
    }
}

// Whether audio actually opens is a real per-device unknown: some handheld
// firmwares expose no audio device at all, and those that do often substitute a
// different rate or channel count than requested. Report what was granted.
void report_audio()
{
    heading("Audio");

    std::string codecs;
    for (const std::string& codec : audio::compiled_codecs()) {
        if (!codecs.empty()) {
            codecs += ", ";
        }
        codecs += codec;
    }
    field("codecs compiled", codecs);
    field("codec tier", WREEL_AUDIO_CODEC_TIER);

    const audio::Spec requested;
    field("requested", std::to_string(requested.frequency) + " Hz, " +
                           std::to_string(requested.channels) + " ch, " +
                           std::to_string(requested.buffer) + " buf, " +
                           std::to_string(requested.voices) + " voices");

    // Constructing a Device opens the mixer; it reports unavailable rather than
    // throwing when there is no hardware.
    const audio::Device device;
    if (!device.available()) {
        field("device", "UNAVAILABLE — this device has no usable audio output");
        return;
    }

    const audio::Spec& got = device.actual();
    field("granted", std::to_string(got.frequency) + " Hz, " +
                         std::to_string(got.channels) + " ch, " +
                         std::to_string(got.voices) + " voices");
    field("driver", device.driver_name());

    if (got.frequency != requested.frequency ||
        got.channels != requested.channels) {
        field("note", "device substituted a different format");
    }
}

// A GL context is only attempted where SDL was built with GL support. On
// GPU-less targets this whole function is compiled out.
void report_gl()
{
#if defined(WREEL_PROBE_GL)
    heading("OpenGL");

    SDL_Window* window = SDL_CreateWindow(
        "wreel-probe", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 64, 64,
        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (!window) {
        field("context", std::string("window failed: ") + SDL_GetError());
        return;
    }

    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context) {
        field("context", std::string("failed: ") + SDL_GetError());
        SDL_DestroyWindow(window);
        return;
    }

    const auto* version =
        reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const auto* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const auto* renderer =
        reinterpret_cast<const char*>(glGetString(GL_RENDERER));

    field("version", version ? version : "(unavailable)");
    field("vendor", vendor ? vendor : "(unavailable)");
    field("renderer", renderer ? renderer : "(unavailable)");

    int major = 0;
    int minor = 0;
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major);
    SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor);
    field("context version",
          std::to_string(major) + "." + std::to_string(minor));

    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
#else
    heading("OpenGL");
    field("status", "not compiled in (target has no GPU)");
#endif
}

} // namespace

int main(int, char**)
{
    std::printf("wreel-probe %s  [target: %s, gfx backend: %s]\n",
                WREEL_VERSION, WREEL_TARGET_ID,
#if defined(WREEL_GFX_BACKEND_NAME)
                WREEL_GFX_BACKEND_NAME
#else
                "n/a"
#endif
    );

    SDL_version compiled;
    SDL_version linked;
    SDL_VERSION(&compiled);
    SDL_GetVersion(&linked);
    std::printf("SDL2 compiled %d.%d.%d, linked %d.%d.%d\n", compiled.major,
                compiled.minor, compiled.patch, linked.major, linked.minor,
                linked.patch);

    // Video is required; joystick is best-effort so a headless CI box still
    // gets a useful report.
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init(VIDEO) failed: %s\n", SDL_GetError());
        return 1;
    }
    if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) != 0) {
        std::fprintf(stderr, "warning: joystick subsystem unavailable: %s\n",
                     SDL_GetError());
    }

    std::printf("platform: %s, %d logical cores, %d MB RAM\n",
                SDL_GetPlatform(), SDL_GetCPUCount(), SDL_GetSystemRAM());

    report_drivers();
    report_renderers();
    report_displays();
    report_input();
    report_audio();
    report_gl();

    std::printf("\n");
    SDL_Quit();
    return 0;
}
