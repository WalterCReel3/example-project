#include "demo.hpp"

#include <SDL.h>
#include <SDL_ttf.h>

#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <vector>

#include <rig/assets.hpp>
#include <util/format.hpp>
#include <util/logging.hpp>

#include "greetz.hpp"

namespace coppers
{

namespace
{

// Exponential smoothing on the per-stage timings, for the same reason
// rig::FrameTiming smooths its frame rate: an unsmoothed sub-millisecond figure
// is unreadable on a panel.
void accumulate(double& average, double sample)
{
    if (average <= 0.0) {
        average = sample;
    } else {
        average += 0.05 * (sample - average);
    }
}

} // namespace

Demo::Demo(const Options& options)
    : _options(options)
    , _exit(false)
    , _hud_measured(false)
    , _layer_only(false)
    , _system()
    , _context()
    , _layer()
    , _field()
    , _glyphs()
#ifdef WREEL_COPPERS_TEXTURE_SCROLLER
    , _texture_scroller()
#endif
    , _cpu_scroller()
    , _message()
    , _playlist()
    , _font(nullptr)
    , _pad()
    , _clock(options.target_fps)
    , _plot_ms(0.0)
    , _blit_ms(0.0)
    , _present_ms(0.0)
{
    // 640x480 requested rather than the Flip's 750x560: with
    // FULLSCREEN_DESKTOP the request is ignored in favour of the panel's own
    // mode, so this only sizes the desktop window in emulator mode. The layer
    // below is sized from what SDL actually gave.
    _context.reset(
        new gfx::renderer::Context("coppers", 640, 480, _options.fullscreen));

    // 20 columns, 3 rows, first glyph is ASCII 32. Passed in rather than
    // hard-coded in GlyphSheet so a different sheet is a call-site change; see
    // data/PROVENANCE.md, which explains why this file will have to be replaced
    // before anything ships.
    _glyphs.reset(
        new GlyphSheet(rig::asset_path("glyphs-16x16.png"), 20, 3, 32));

#ifdef WREEL_COPPERS_TEXTURE_SCROLLER
    _texture_scroller = make_texture_scroller(*_context, *_glyphs);
#else
    // Not built for this target, so the option cannot be honoured whatever the
    // command line said. See coppers/CMakeLists.txt.
    _options.cpu_scroller = true;
#endif
    _cpu_scroller = make_cpu_scroller(*_glyphs);

    // Where blending is a no-op, the HUD is plotted into the layer rather than
    // drawn as a texture.
    //
    // The Miyoo Mini's render backend is the case in hand, and blending is the
    // whole of the reason. draw_text() rasterises with TTF_RenderUTF8_Blended
    // and uploads through SDL_CreateTextureFromSurface, which sets
    // SDL_BLENDMODE_BLEND; this backend accepts the mode and never applies it,
    // so the text's rectangle would arrive opaque over the copper bars instead
    // of just its glyphs. Plotting into the layer composites on the CPU, where
    // alpha works.
    //
    // Detected at runtime by name rather than compiled in per target, because
    // three different SDL2 builds for this device have now been seen and the
    // name is the only thing that reliably tells them apart. A firmware that
    // ships a fixed one will simply stop matching.
    //
    // The flags and the B button still work: this changes the default, not the
    // capability, so the comparison the demo exists for stays available
    // wherever it means anything.
    if (_context->driver_name() == "Miyoo Mini") {
        _layer_only = true;

        // Logged whenever the driver matches, not only when something had to be
        // overridden. On the target that omits the texture scroller at build
        // time there is nothing left to override, and a silent run would leave
        // no record of *why* the HUD is in the layer.
        util::log_warning("%s does not apply blend modes; plotting the HUD "
                          "into the layer",
                          _context->driver_name().c_str());

        // A separate cause behind the same name, kept separate because the two
        // will stop being true at different times. The texture scroller blits
        // glyphs out of an atlas, and this backend stages a source
        // sub-rectangle from row 0 while telling the blitter to read from the
        // rect's y. Normally moot — the scroller is not built for this target —
        // and it matters only if WREEL_COPPERS_TEXTURE_SCROLLER is forced on.
        _options.cpu_scroller = true;
    }

    if (!_options.mute) {
        // Two of the three filenames contain spaces, which is why these are
        // data rather than a glob or a shell-assembled path.
        std::vector<std::string> tracks;
        tracks.push_back("complications.mod");
        tracks.push_back("complications ii.mod");
        tracks.push_back("her bloody weekend.mod");
        _playlist.reset(new Playlist(tracks));
    }

    // Builds the layer, the bar field and the message together, so the
    // constructor and the X button cannot drift apart.
    set_layer_height(_options.layer_height);

    // The HUD is the only other text, so a missing font costs the HUD rather
    // than the demo — the bars and the scroller are the parts being measured.
    const std::string font_path = rig::asset_path("Speedy.fon");
    _font = TTF_OpenFontIndex(font_path.c_str(), 10, 0);
    if (!_font) {
        util::log_warning("no HUD: could not load %s: %s", font_path.c_str(),
                          TTF_GetError());
    }

    util::log_info("coppers: window %dx%d, layer %dx%d, driver %s (%s), "
                   "input %s",
                   _context->width(), _context->height(), _layer->width(),
                   _layer->height(), _context->driver_name().c_str(),
                   _context->accelerated() ? "accelerated" : "software",
                   _pad.description().c_str());
}

Demo::~Demo()
{
    if (_font) {
        TTF_CloseFont(_font);
    }
}

void Demo::handle_events()
{
    _pad.begin_frame();

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        _pad.handle_event(event);
    }

    if (_pad.quit_requested() || _pad.pressed(rig::Button::Start)) {
        _exit = true;
    }

    // Left and right step the playlist. Held-down repeat is deliberately not
    // wanted here: pressed() is an edge, so holding left does not tear through
    // every track.
    if (_pad.pressed(rig::Button::Left)) {
        if (_playlist) {
            _playlist->previous();
        }
    }
    if (_pad.pressed(rig::Button::Right)) {
        if (_playlist) {
            _playlist->next();
        }
    }

    if (_pad.pressed(rig::Button::A)) {
        _field->next_palette();
        util::log_info("palette: %s", _field->palette_name());
    }

    if (_pad.pressed(rig::Button::B)) {
#ifdef WREEL_COPPERS_TEXTURE_SCROLLER
        _options.cpu_scroller = !_options.cpu_scroller;
        util::log_info("scroller: %s", scroller().name());
#else
        util::log_info("scroller: %s only; the texture path is not built for "
                       "this target",
                       scroller().name());
#endif
    }

    if (_pad.pressed(rig::Button::X)) {
        // Round trip between the panel's own resolution and a 240-line layer.
        // Which of those is faster depends on the driver and inverts between
        // them, which is exactly why this is a button and not a constant — see
        // planning/2026-07-25-target-validation/results.md.
        const int native = _context->height();
        set_layer_height(_layer->height() == native ? 240 : native);
    }

    if (_pad.pressed(rig::Button::Y)) {
        _options.hud = !_options.hud;
    }
}

void Demo::set_layer_height(int height)
{
    const int window_h = _context->height();
    const int window_w = _context->width();

    int layer_h = height > 0 ? height : window_h;
    if (layer_h > window_h) {
        layer_h = window_h;
    }
    if (layer_h < 1) {
        layer_h = 1;
    }

    int layer_w = window_h > 0 ? (window_w * layer_h + window_h / 2) / window_h
                               : window_w;
    if (layer_w < 1) {
        layer_w = 1;
    }

    // Replaced rather than resized: a streaming texture's dimensions are fixed
    // when it is created.
    _layer.reset(new gfx::renderer::Layer(*_context, layer_w, layer_h));
    _field.reset(new BarField(layer_h, _field ? _field->palette_index() : 0));

    // The message quotes the layer size, so it is rebuilt too. Cheap, and once
    // per button press.
    _message =
        to_sheet_text(build_greetz(*_context, layer_w, layer_h,
                                   _playlist ? &_playlist->device() : nullptr),
                      *_glyphs);

    // The per-stage averages describe the old resolution and would take a
    // second to converge, reading as a slow transition rather than a step.
    _plot_ms = 0.0;
    _blit_ms = 0.0;
    _present_ms = 0.0;

    util::log_info("layer: %dx%d", layer_w, layer_h);
}

Scroller& Demo::scroller()
{
#ifdef WREEL_COPPERS_TEXTURE_SCROLLER
    return _options.cpu_scroller ? *_cpu_scroller : *_texture_scroller;
#else
    return *_cpu_scroller;
#endif
}

double Demo::scroller_cost_us() const
{
#ifdef WREEL_COPPERS_TEXTURE_SCROLLER
    return _options.cpu_scroller ? _cpu_scroller->cost_us()
                                 : _texture_scroller->cost_us();
#else
    return _cpu_scroller->cost_us();
#endif
}

ScrollState Demo::scroll_state(double t, int target_w, int target_h) const
{
    ScrollState state;
    state.text = _message;

    // Scale so a glyph is a readable fraction of the target height whatever the
    // resolution: a fixed pixel scale would be unreadable at 240 lines and tiny
    // at 560. A 16px cell at /240 gives 32px glyphs on a 480-line panel, which
    // is about twenty characters across — a scroller, rather than three
    // enormous letters.
    state.scale = target_h / 240;
    if (state.scale < 1) {
        state.scale = 1;
    }

    const double width =
        static_cast<double>(_message.size()) *
        static_cast<double>(_glyphs->cell_width() * state.scale);

    // Wrapped rather than clamped, so the message runs forever. std::fmod on
    // the travelled distance keeps the arithmetic exact regardless of how long
    // the demo has been up, which a running subtraction would not.
    const double travelled = t * _options.scroll_speed;
    const double span = width + static_cast<double>(target_w);
    state.x = static_cast<double>(target_w) - std::fmod(travelled, span);

    // Two thirds down, clear of the HUD and roughly where a cracktro puts it.
    state.y = (target_h * 2) / 3;

    // Follows the palette so the switch recolours the text with the bars — that
    // is what reading the sheet as a 1-bit mask bought — but pushed halfway to
    // white. The palette colour alone is exactly the colour of the bars the
    // text crosses, so it disappeared against them; the shadow below handles
    // the rest.
    const Palette& p = palettes()[_field->palette_index()];
    state.r = static_cast<unsigned char>(128 + p.r / 2);
    state.g = static_cast<unsigned char>(128 + p.g / 2);
    state.b = static_cast<unsigned char>(128 + p.b / 2);

    state.shadow = true;
    state.shadow_r = p.bg_r / 2;
    state.shadow_g = p.bg_g / 2;
    state.shadow_b = p.bg_b / 2;

    return state;
}

void Demo::draw_frame(double t)
{
    const double plot_start = rig::FrameClock::now();

    _field->resolve(t);

    {
        gfx::renderer::LayerLock pixels = _layer->lock();
        if (!pixels) {
            // A failed lock costs this frame, not the run.
            return;
        }

        const std::uint32_t* rows = _field->rows();
        const int width = pixels.width();
        for (int y = 0; y < pixels.height(); ++y) {
            std::uint32_t* row = pixels.row(y);
            const std::uint32_t color = rows[y];
            // A scanline is one colour, so this is the whole effect. Left as a
            // plain loop rather than std::fill_n because it is the thing being
            // measured and the generated code should be obvious.
            for (int x = 0; x < width; ++x) {
                row[x] = color;
            }
        }

        // The CPU scroller draws here, inside the lock, at the LAYER's
        // resolution — so its text is part of the framebuffer and scales up
        // with everything else. The texture scroller cannot run here; it needs
        // the layer composited first.
        if (scroller().phase() == ScrollPhase::Plot) {
            scroller().plot(pixels,
                            scroll_state(t, pixels.width(), pixels.height()));
        }

        // The HUD goes in here too where the renderer does not blend. Its cost
        // then lands in the plot stage rather than the blit stage, which is
        // worth knowing when reading the numbers: the
        // instrument is inside the thing it measures. --no-hud remains the way
        // to take a clean reading, and the exit summary is unaffected either
        // way.
        if (_options.hud && _layer_only) {
            plot_hud(pixels);
        }
    } // unlocked here, which is what makes the plot timing meaningful

    const double blit_start = rig::FrameClock::now();
    accumulate(_plot_ms, (blit_start - plot_start) * 1000.0);

    // No clear(): the layer covers the whole target with an opaque blit, so
    // clearing first would write every pixel twice. That is 1.2 MB of pointless
    // memory traffic per frame at 640x480, which is not nothing on this target.
    _layer->draw();

    // The texture scroller draws here instead, at the TARGET's resolution, so
    // its text stays crisp when the layer is scaled up. Toggling B at
    // --layer-height 240 makes that difference visible, which is the comparison
    // this demo is for.
    if (scroller().phase() == ScrollPhase::Composite) {
        scroller().composite(
            *_context, scroll_state(t, _context->width(), _context->height()));
    }

    // Drawn as a texture only where that works. Where it does not, it was
    // already plotted into the layer above.
    if (_options.hud && !_layer_only) {
        draw_hud();
    }

    // SDL batches render commands and executes them at present, so without this
    // the blit stage measures ~0.000 ms and present carries the cost of both.
    // That reads as "scaling is free", which is the opposite of what the
    // numbers say once they are attributed correctly.
    //
    // Flushing here does defeat batching, which is a real if small distortion
    // of what is being measured. Accepted deliberately: this demo's job is to
    // produce a fill-rate figure, and two draw calls have nothing worth
    // batching.
    SDL_RenderFlush(_context->renderer());

    accumulate(_blit_ms, (rig::FrameClock::now() - blit_start) * 1000.0);
}

// Two lines rather than one, because a texture cannot be wider than the device
// allows and the Miyoo Mini allows exactly the panel: 640x480. The single line
// this used to be rasterised past that, so every frame failed to upload and the
// HUD simply was not there — on the dev box the same string is fine, because a
// desktop driver's limit is 16384. Anything drawn as one texture has to fit the
// panel on this hardware; see docs/TARGETS.md.
std::string Demo::hud_text() const
{
    return util::format("%.1f fps  plot %.2f  blit %.2f  present %.2f%s",
                        _clock.fps(), _plot_ms, _blit_ms, _present_ms,
                        _clock.clamped() ? "  STALL" : "");
}

std::string Demo::hud_text_second() const
{
    return util::format("%dx%d->%dx%d  scroll %s %.0f us  %s  %s",
                        _layer->width(), _layer->height(), _context->width(),
                        _context->height(),
                        _options.cpu_scroller ? "cpu" : "tex",
                        scroller_cost_us(), _field->palette_name(),
                        _playlist && !_playlist->current().empty()
                            ? _playlist->current().c_str()
                            : "silent");
}

// Copies a rasterised text surface into the locked layer, alpha-blended.
//
// This exists so the HUD keeps SDL_ttf and Speedy.fon where the renderer cannot
// draw a sub-rectangle. It deliberately does NOT use the glyph sheet: the sheet
// is 16px uppercase ASCII 32-91, so a sixty-character line of figures would
// neither fit nor read — and more importantly, decision 2 of this demo's
// snapshot pins the HUD to a different mechanism from the scroller on purpose,
// so that the instrument does not move with the thing it measures. Only the
// transport changes here, not the rasteriser.
//
// It wants to live in gfx::renderer rather than in a demo: compositing a
// surface into a layer is what every sprite on this target will have to do.
// Left here until software-2d-sprites-tiling needs it, so the interface is
// designed against two callers rather than guessed at with one.
void blit_surface_into_layer(gfx::renderer::LayerLock& pixels,
                             SDL_Surface* surface, int left, int top)
{
    SDL_Surface* argb =
        SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ARGB8888, 0);
    if (!argb) {
        return;
    }

    const std::uint32_t* src = static_cast<const std::uint32_t*>(argb->pixels);
    const int src_stride =
        argb->pitch / static_cast<int>(sizeof(std::uint32_t));

    for (int y = 0; y < argb->h; ++y) {
        const int dy = top + y;
        if (dy < 0 || dy >= pixels.height()) {
            continue;
        }

        std::uint32_t* row = pixels.row(dy);

        for (int x = 0; x < argb->w; ++x) {
            const int dx = left + x;
            if (dx < 0 || dx >= pixels.width()) {
                continue;
            }

            const std::uint32_t s = src[y * src_stride + x];
            const unsigned a = (s >> 24) & 0xffu;
            if (a == 0) {
                continue;
            }
            if (a == 255) {
                row[dx] = s;
                continue;
            }

            // Blended rather than thresholded, because TTF_RenderUTF8_Blended
            // antialiases and a 1-bit test would make the HUD crawl.
            const std::uint32_t d = row[dx];
            const unsigned inv = 255u - a;
            const unsigned r =
                (((s >> 16) & 0xffu) * a + ((d >> 16) & 0xffu) * inv) / 255u;
            const unsigned g =
                (((s >> 8) & 0xffu) * a + ((d >> 8) & 0xffu) * inv) / 255u;
            const unsigned b = ((s & 0xffu) * a + (d & 0xffu) * inv) / 255u;
            row[dx] = gfx::renderer::Layer::pack(static_cast<unsigned char>(r),
                                                 static_cast<unsigned char>(g),
                                                 static_cast<unsigned char>(b));
        }
    }

    SDL_FreeSurface(argb);
}

void Demo::plot_hud(gfx::renderer::LayerLock& pixels)
{
    if (!_font) {
        return;
    }

    SDL_Color white;
    white.r = 255;
    white.g = 255;
    white.b = 255;
    white.a = 255;

    int y = 2;

    for (int i = 0; i < 2; ++i) {
        const std::string text = i == 0 ? hud_text() : hud_text_second();

        SDL_Surface* rendered =
            TTF_RenderUTF8_Blended(_font, text.c_str(), white);
        if (!rendered) {
            continue;
        }

        blit_surface_into_layer(pixels, rendered, 2, y);

        if (!_hud_measured && i == 1) {
            _hud_measured = true;
            util::log_info("hud: plotted into the %dx%d layer, lines %dpx tall",
                           pixels.width(), pixels.height(), rendered->h);
        }

        y += rendered->h + 1;
        SDL_FreeSurface(rendered);
    }
}

void Demo::draw_hud()
{
    if (!_font) {
        return;
    }

    gfx::renderer::Color white;
    white.r = 255;
    white.g = 255;
    white.b = 255;
    white.a = 255;

    gfx::renderer::Rect rect;
    rect.x = 4;
    rect.y = 4;
    rect.w = 0;
    rect.h = 0;

    // Rasterised and uploaded per frame, which is what draw_text does. Every
    // figure in the string changes every frame so caching would miss, and the
    // cost lands in the blit stage where the HUD's own overhead is visible
    // rather than hidden inside the plot measurement.
    _context->draw_text(hud_text(), _font, white, &rect);

    // Below the first, using the height draw_text reported rather than a
    // constant, so it follows whatever the font actually rasterised to.
    gfx::renderer::Rect second;
    second.x = 4;
    second.y = 4 + (rect.h > 0 ? rect.h : 12);
    second.w = 0;
    second.h = 0;

    _context->draw_text(hud_text_second(), _font, white, &second);

    // Once, not per frame. A texture cannot exceed the panel on this hardware,
    // so how wide these lines actually rasterise is the difference between a
    // HUD and 2437 identical upload failures — which is what the first device
    // run produced. Recording the measured widths means the next reader does
    // not have to trust that the split was wide enough.
    if (!_hud_measured) {
        _hud_measured = true;
        util::log_info("hud: line widths %dpx and %dpx, output %dpx wide",
                       rect.w, second.w, _context->width());
    }
}

void Demo::run()
{
    // Discards the time spent opening the window, the font and the layer.
    _clock.reset();

    while (!_exit) {
        _clock.tick();
        handle_events();

        draw_frame(_clock.elapsed());

        const double present_start = rig::FrameClock::now();
        _context->present();
        accumulate(_present_ms,
                   (rig::FrameClock::now() - present_start) * 1000.0);

        if (_options.seconds > 0.0 && _clock.elapsed() >= _options.seconds) {
            _exit = true;
        }
    }

    // The measurement, in a form that can be pasted into a results file. Layer
    // and window size are included because the numbers mean nothing without
    // them, and the driver because a software figure and an opengles2 one are
    // not comparable.
    util::log_info("coppers: %s %dx%d layer, %dx%d window, %llu frames, "
                   "%.1f fps, plot %.3f blit %.3f present %.3f ms, "
                   "scroller %s %.0f us",
                   _context->driver_name().c_str(), _layer->width(),
                   _layer->height(), _context->width(), _context->height(),
                   static_cast<unsigned long long>(_clock.frames()),
                   _clock.fps(), _plot_ms, _blit_ms, _present_ms,
                   scroller().name(), scroller_cost_us());
}

bool Demo::render_to_file(const std::string& path, int frames)
{
    if (frames < 1) {
        frames = 1;
    }

    // Keep the plotted pixels, so there is something to save even where the
    // renderer cannot be read back. This costs a full-frame copy per lock and
    // is switched on only here — a timed run must not pay for it, or the
    // screenshot path would change the numbers the demo exists to produce.
    _layer->set_readback(true);

    // Frames at a fixed 1/60 step rather than at wall-clock time, so the image
    // is reproducible: a screenshot that depends on how long the machine took
    // to get here cannot be compared against a previous one.
    for (int i = 0; i < frames; ++i) {
        handle_events();
        draw_frame(static_cast<double>(i) / 60.0);
        if (i + 1 < frames) {
            _context->present();
        }
    }

    // Before present(), which is what save_screenshot documents.
    if (_context->save_screenshot(path)) {
        return true;
    }

    // The renderer could not read itself back. That is not an edge case on the
    // target this demo was written for: the Miyoo Mini's SDL2 returns
    // SDL_Unsupported from SDL_RenderReadPixels, so this is the path that runs
    // there, and without it a screenshot is impossible on the one device where
    // it is the only way to see the output.
    //
    // The two images are not identical. This one is the layer as plotted —
    // before scaling to the window, and without anything drawn over it through
    // the renderer, which on that device is nothing that works anyway.
    util::log_warning(
        "screenshot: renderer read-back failed, saving the layer instead");
    return _layer->save_bmp(path);
}

} // namespace coppers
