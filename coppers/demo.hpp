#pragma once

#include <memory>
#include <string>

#include <SDL_ttf.h>

#include <gfx/renderer/context.hpp>
#include <gfx/renderer/layer.hpp>
#include <gfx/system.hpp>
#include <rig/input.hpp>
#include <rig/timing.hpp>

#include "bars.hpp"
#include "glyphs.hpp"
#include "playlist.hpp"
#include "scroller.hpp"

//============================================================================
//
// coppers — a copper-bar cracktro, and the instrument that measures the
// software driver's fill rate
//
// This is the gfx::renderer demo. It never touches gfx::gles2 and it builds on
// every target, including the GPU-less Miyoo Mini, which is the whole point:
// skratch is the modern-GL showcase and this is the one that runs on the
// hardware the project is actually aimed at.
//
// See planning/2026-07-26-coppers-cracktro/.
//
//============================================================================
namespace coppers
{

struct Options {
    bool fullscreen = true;
    // 0 means "match the window", any other value is the layer height and the
    // width follows the window's aspect. 240 gives the classic look and a
    // quarter of the plotting cost of a 480-line panel.
    int layer_height = 0;
    int target_fps = 60;
    bool hud = true;
    // Pixels per second the message travels. Fast enough to read as a scroller
    // rather than a caption — at 32px glyphs this is about five characters a
    // second. Negative would run it backwards, which nobody asked for.
    double scroll_speed = 160.0;
    // Start on the hand-written blitter instead of the driver one.
    bool cpu_scroller = false;
    // Silence. Useful when timing, since the mixer callback competes for the
    // same two cores the plotting loop is on.
    bool mute = false;
    std::string screenshot; // non-empty: render a few frames, save, exit
    int screenshot_frames = 2;
    // 0 runs until quit. Non-zero exits after that many seconds, which is how a
    // fill-rate measurement gets taken on a device over SSH — there is no
    // keyboard to press Escape on, and a fixed duration is repeatable in a way
    // that "however long I left it running" is not.
    double seconds = 0.0;
};

class Demo
{
public:
    explicit Demo(const Options& options);
    ~Demo();

    Demo(const Demo&) = delete;
    Demo& operator=(const Demo&) = delete;

    void run();

    // Renders `frames` frames, writes the last to `path` and returns without
    // entering the loop. The gfx::renderer counterpart of skratch's
    // --screenshot, and how this gets checked on a device over SSH.
    bool render_to_file(const std::string& path, int frames);

private:
    void handle_events();
    void draw_frame(double t);
    void draw_hud();
    std::string hud_text() const;
    // Rebuilds the layer, the bar field and the message at a new height. The
    // layer's size is fixed at construction, so switching resolution means
    // replacing it rather than resizing it.
    void set_layer_height(int height);
    Scroller& scroller();
    // Const, because hud_text() is and reading a cost does not need mutable
    // access to the scroller.
    double scroller_cost_us() const;
    ScrollState scroll_state(double t, int target_w, int target_h) const;

    Options _options;
    bool _exit;

    // First member so it is destroyed last: SDL_Quit must not run before the
    // window, the layer or the font are gone.
    gfx::System _system;

    std::unique_ptr<gfx::renderer::Context> _context;
    std::unique_ptr<gfx::renderer::Layer> _layer;
    std::unique_ptr<BarField> _field;

    // One sheet, two consumers: the texture scroller uploads it and the CPU one
    // reads its mask. Declared before them because both hold a reference to it.
    std::unique_ptr<GlyphSheet> _glyphs;
    std::unique_ptr<Scroller> _texture_scroller;
    std::unique_ptr<Scroller> _cpu_scroller;
    std::string _message;

    std::unique_ptr<Playlist> _playlist;

    TTF_Font* _font;

    rig::Pad _pad;
    rig::FrameClock _clock;

    // Per-stage costs in milliseconds, smoothed. Reported separately because a
    // lower internal resolution cuts the plot and leaves the blit and the
    // present alone — one combined figure would make a 4x reduction in plotting
    // look like no change at all. That distinction is the measurement this demo
    // exists to take.
    double _plot_ms;
    double _blit_ms;
    double _present_ms;
};

} // namespace coppers
