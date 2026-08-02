// Facts about the machine, gathered before anything is drawn.
//
// Most of this is unremarkable on a desktop and load-bearing on a handheld,
// where the answer to "which libSDL2 am I actually running against" has been
// wrong twice: the bundle ships one beside the binary, the firmware has its
// own, and a bad LD_LIBRARY_PATH silently picks the other one. /proc/self/maps
// settles it — it reports the file the loader really opened.

#include "diag.hpp"

#include <SDL.h>

#include <cstdio>
#include <cstring>
#include <string>

#include <sys/utsname.h>

#include <util/format.hpp>

namespace diag
{

namespace
{

// Whole small file into a string. Everything read here is proc, sysfs or a
// firmware config: all tiny, none seekable in a useful way.
std::string slurp(const char* path, std::size_t limit = 8192)
{
    std::FILE* file = std::fopen(path, "rb");
    if (!file) {
        return std::string();
    }

    std::string text;
    char buffer[1024];
    std::size_t got = 0;
    while ((got = std::fread(buffer, 1, sizeof(buffer), file)) > 0) {
        text.append(buffer, got);
        if (text.size() >= limit) {
            break;
        }
    }
    std::fclose(file);
    return text;
}

std::string first_line(const std::string& text)
{
    const std::size_t end = text.find('\n');
    return end == std::string::npos ? text : text.substr(0, end);
}

// Which shared objects the loader actually resolved, for the ones that have
// ever been in question here.
void report_loaded_libraries()
{
    const std::string maps = slurp("/proc/self/maps", 262144);
    if (maps.empty()) {
        field("loaded libraries", "(/proc/self/maps unavailable)");
        return;
    }

    static const char* const interesting[] = { "libSDL2", "libEGL", "libGLESv2",
                                               "libjson-c", "libmi_" };

    bool any = false;
    std::size_t start = 0;
    std::string seen;
    while (start < maps.size()) {
        const std::size_t end = maps.find('\n', start);
        const std::string line =
            maps.substr(start, end == std::string::npos ? std::string::npos
                                                        : end - start);
        start = end == std::string::npos ? maps.size() : end + 1;

        const std::size_t slash = line.find('/');
        if (slash == std::string::npos) {
            continue;
        }
        const std::string path = line.substr(slash);

        for (const char* needle : interesting) {
            if (path.find(needle) == std::string::npos) {
                continue;
            }
            // One line per library, not one per mapping — a shared object
            // shows up three or four times with different permissions.
            if (seen.find("|" + path + "|") != std::string::npos) {
                break;
            }
            seen += "|" + path + "|";
            field("loaded", path);
            any = true;
            break;
        }
    }

    if (!any) {
        field("loaded libraries",
              "none of libSDL2/libEGL/libGLESv2/libjson-c/libmi_* — a static "
              "build, or a stripped /proc");
    }
}

} // namespace

void report_environment()
{
    section("Machine");

    utsname host;
    if (uname(&host) == 0) {
        field("kernel", util::format("%s %s %s", host.sysname, host.release,
                                     host.machine));
    }

    field("platform", SDL_GetPlatform());
    field("cores", SDL_GetCPUCount());
    field("RAM (MB)", SDL_GetSystemRAM());

    const std::string model = first_line(slurp("/sys/firmware/devicetree/base/model"));
    if (!model.empty()) {
        field("board", model);
    }

    section("Framebuffer");

    std::string fb;
    if (framebuffer_probe(&fb)) {
        field("/dev/fb0", fb);
    } else {
        field("/dev/fb0", "absent — readback will use SDL_RenderReadPixels or "
                          "nothing");
    }

    section("Firmware");

    // The file the audio driver used to read with json-c to overwrite the
    // system volume on every open. Nothing reads it now — the firmware owns
    // that setting and we leave it alone — but it is reported because it says
    // what the firmware thinks the volume is, which is the first thing worth
    // knowing when a device comes back silent.
    const std::string system_json = slurp("/appconfigs/system.json", 4096);
    if (system_json.empty()) {
        field("/appconfigs/system.json", "absent");
    } else {
        field("/appconfigs/system.json",
              util::format("%zu bytes", system_json.size()));
        const std::size_t vol = system_json.find("\"vol\"");
        if (vol != std::string::npos) {
            field("  vol entry", system_json.substr(vol, 24));
        }
    }

    for (const char* path : { "/mnt/SDCARD/.tmp_update", "/customer/app",
                              "/dev/mi_ao", "/dev/input/event0" }) {
        std::FILE* probe = std::fopen(path, "rb");
        field(path, probe ? "present" : "absent");
        if (probe) {
            std::fclose(probe);
        }
    }

    section("Loader");
    report_loaded_libraries();
}

void report_sdl_capability()
{
    section("SDL2");

    SDL_version compiled;
    SDL_version linked;
    SDL_VERSION(&compiled);
    SDL_GetVersion(&linked);
    field("compiled against",
          util::format("%d.%d.%d", compiled.major, compiled.minor,
                       compiled.patch));
    field("linked against", util::format("%d.%d.%d", linked.major,
                                         linked.minor, linked.patch));
    field("revision", SDL_GetRevision());

    if (compiled.major != linked.major || compiled.minor != linked.minor ||
        compiled.patch != linked.patch) {
        check("header/runtime match", Verdict::Wrong,
              "the headers this was built against and the library it loaded "
              "are different versions");
    } else {
        check("header/runtime match", Verdict::Ok, "");
    }

    std::string video;
    for (int i = 0; i < SDL_GetNumVideoDrivers(); ++i) {
        if (!video.empty()) {
            video += ", ";
        }
        video += SDL_GetVideoDriver(i);
    }
    field("video drivers", video.empty() ? "(none)" : video);

    const char* current = SDL_GetCurrentVideoDriver();
    field("video in use", current ? current : "(none)");

    std::string audio;
    for (int i = 0; i < SDL_GetNumAudioDrivers(); ++i) {
        if (!audio.empty()) {
            audio += ", ";
        }
        audio += SDL_GetAudioDriver(i);
    }
    field("audio drivers", audio.empty() ? "(none)" : audio);

    for (int i = 0; i < SDL_GetNumRenderDrivers(); ++i) {
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
        field(util::format("render driver %d", i).c_str(),
              util::format("%-12s %s max %dx%d", info.name, flags.c_str(),
                           info.max_texture_width, info.max_texture_height));
    }

    section("Display");

    const int displays = SDL_GetNumVideoDisplays();
    field("displays", displays);

    for (int d = 0; d < displays; ++d) {
        SDL_DisplayMode desktop;
        if (SDL_GetDesktopDisplayMode(d, &desktop) == 0) {
            field("desktop mode",
                  util::format("%dx%d @ %dHz %s", desktop.w, desktop.h,
                               desktop.refresh_rate,
                               SDL_GetPixelFormatName(desktop.format)));
        } else {
            // D22/D24: this driver adds display modes and never sets the
            // desktop one, so anything sizing itself from the desktop mode
            // gets nothing.
            check("desktop mode", Verdict::Failed, SDL_GetError());
        }

        SDL_Rect bounds;
        if (SDL_GetDisplayBounds(d, &bounds) == 0) {
            field("display bounds", util::format("%dx%d at %d,%d", bounds.w,
                                                 bounds.h, bounds.x, bounds.y));
        } else {
            check("display bounds", Verdict::Failed, SDL_GetError());
        }

        field("modes", SDL_GetNumDisplayModes(d));
    }
}

} // namespace diag
