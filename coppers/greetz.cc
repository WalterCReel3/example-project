#include "greetz.hpp"

#include <SDL.h>

#include <string>

#include <audio/device.hpp>
#include <gfx/renderer/context.hpp>
#include <util/format.hpp>

namespace coppers
{

namespace
{

std::string display_mode()
{
    SDL_DisplayMode mode;
    if (SDL_GetDesktopDisplayMode(0, &mode) != 0) {
        return "unknown display";
    }
    return util::format("%dx%d at %d hz", mode.w, mode.h, mode.refresh_rate);
}

std::string gamepad()
{
    const int count = SDL_NumJoysticks();
    if (count < 1) {
        return "no pad, keys only";
    }
    const char* name = SDL_JoystickNameForIndex(0);
    // The name is the answer to a real open question: skratch hard-codes Xbox
    // 360 axis indices and target-validation predicts that is wrong on a
    // handheld. Putting it in the scroller means a device tells you what it is
    // without needing a console.
    return util::format("%d pad%s, first is %s", count, count == 1 ? "" : "s",
                        name ? name : "unnamed");
}

std::string audio_line(const audio::Device* device)
{
    if (!device) {
        return "muted";
    }
    if (!device->available()) {
        return "no audio device, running in silence";
    }
    const audio::Spec& got = device->actual();
    return util::format("audio %d hz %d ch via %s", got.frequency, got.channels,
                        device->driver_name().c_str());
}

} // namespace

std::string build_greetz(const gfx::renderer::Context& context, int layer_width,
                         int layer_height, const audio::Device* device)
{
    const std::string separator = "   ***   ";

    std::string message;
    message += "wreel presents ... coppers ...";
    message += separator;
    message +=
        util::format("running on %s, %d cores, %d mb ram", SDL_GetPlatform(),
                     SDL_GetCPUCount(), SDL_GetSystemRAM());
    message += separator;
    message += util::format("target %s", WREEL_TARGET_ID);
    message += separator;
    message +=
        util::format("render driver %s, %s", context.driver_name().c_str(),
                     context.accelerated() ? "accelerated" : "all on the cpu");
    message += separator;
    message += util::format("panel %s", display_mode().c_str());
    message += separator;
    message += util::format("plotting %dx%d and scaling to %dx%d", layer_width,
                            layer_height, context.width(), context.height());
    message += separator;
    message += audio_line(device);
    message += separator;
    message += gamepad();
    message += separator;
    message += util::format("video driver %s", SDL_GetCurrentVideoDriver()
                                                   ? SDL_GetCurrentVideoDriver()
                                                   : "none");
    message += separator;
    message +=
        "greetz to sigmastar for the ssd202d ... two cortex a7 and no gpu "
        "at all ... every pixel here was placed by hand";
    message += separator;
    message += "and to sdl2, pugixml, glm, doctest and libxmp ... pinned, "
               "vendored and built from source for five targets";
    message += separator;
    message += "press a for palette ... b to switch the scroller between a "
               "driver blit and a hand written one ... x for resolution ... "
               "y for the numbers";
    message += separator;
    message += "this message never ends, it just wraps around ...";
    message += separator;

    return message;
}

} // namespace coppers
