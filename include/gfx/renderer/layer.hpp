#pragma once

#include <cstdint>

//============================================================================
//
// A CPU-plotted layer
//
//     gfx::renderer::Layer layer(context, 320, 240);
//     {
//         gfx::renderer::LayerLock pixels = layer.lock();
//         for (int y = 0; y < pixels.height(); ++y) {
//             std::uint32_t* row = pixels.row(y);
//             for (int x = 0; x < pixels.width(); ++x) {
//                 row[x] = colour;
//             }
//         }
//     }                       // unlocked here
//     layer.draw();           // scaled to fill the render target
//
// This is the escape hatch that SDL_Renderer does not otherwise provide, and it
// exists because on a GPU-less device there are no shaders. Everything
// per-pixel — a fade, a palette flash, plasma, raster bars, distortion — needs
// somewhere to plot, and Context's other draw calls cannot express any of it.
// Adding it was a decision rather than a convenience: see
// planning/2026-07-26-coppers-cracktro/ § 1, and the open question it closes in
// planning/2026-07-25-software-2d-sprites-tiling/.
//
// It is NOT a second renderer. The texture underneath is an ordinary
// SDL_Texture drawn with an ordinary blit, so a frame can plot a background
// here and then draw sprites and text over it through the same Context. The
// state-restoration problem that keeps gfx::gles2 separate does not arise —
// that one is specific to mixing our own GL calls into a window SDL_Renderer
// owns.
//
// TWO THINGS TO KNOW
//
// *The layer's size is independent of the window's.* A 320x240 layer on a
// 640x480 panel quarters the pixels the CPU has to plot, and scaling is nearest
// by default so it stays crisp rather than going soft. But it does not quarter
// the frame: the scaling blit still writes every pixel of the render target, it
// just does it in SDL's blitter instead of in your loop. Time the plot and the
// blit separately or that distinction is invisible.
//
// *Locking is a real cost.* SDL may hand back a staging buffer and copy the
// whole thing on unlock, so lock once per frame and plot everything, rather
// than locking per effect. The lock guard exists to make the scope obvious for
// that reason as much as for exception safety.
//
//============================================================================
struct SDL_Renderer;
struct SDL_Texture;

namespace gfx
{
namespace renderer
{

class Context;
struct Rect;

// Writable view of a locked Layer. Unlocks on destruction; move-only, so the
// lock cannot be duplicated. Plotting through a stale copy after an unlock is
// the bug this shape exists to prevent.
class LayerLock
{
public:
    LayerLock(LayerLock&& other) noexcept;
    LayerLock& operator=(LayerLock&& other) noexcept;
    ~LayerLock();

    LayerLock(const LayerLock&) = delete;
    LayerLock& operator=(const LayerLock&) = delete;

    // False if the lock failed, in which case row() must not be called. A
    // failed lock is reported by SDL rather than thrown, and a demo should drop
    // the frame instead of dying.
    bool valid() const { return _pixels != nullptr; }
    explicit operator bool() const { return valid(); }

    int width() const { return _width; }
    int height() const { return _height; }

    // Start of scanline y. Pixels are packed ARGB8888 in native byte order, so
    // pack() below composes them portably.
    //
    // Unchecked: this is the innermost loop of every effect that will use it,
    // and a bounds test per scanline on two Cortex-A7 cores is not free. y must
    // be in [0, height).
    std::uint32_t* row(int y) const
    {
        return _pixels + static_cast<std::ptrdiff_t>(y) * _stride;
    }

    // Distance between scanlines, in pixels. Not necessarily width(): SDL pads
    // rows, so advancing by width() rather than this skews the image — the
    // classic symptom being a picture that shears diagonally.
    int stride() const { return _stride; }

private:
    friend class Layer;
    LayerLock(SDL_Texture* texture, std::uint32_t* pixels, int stride,
              int width, int height);

    SDL_Texture* _texture; // not owned; the Layer owns it
    std::uint32_t* _pixels;
    int _stride;
    int _width;
    int _height;
};

// A streaming texture sized independently of the window, plotted by the CPU.
class Layer
{
public:
    // Throws std::runtime_error if the texture cannot be created. The Context
    // must outlive this: the texture belongs to its renderer.
    Layer(Context& context, int width, int height);
    ~Layer();

    // Owns an SDL_Texture, so copying would double-free it. Not movable either,
    // for now: nothing needs to move one, and a moved-from Layer would need a
    // null-texture state that every method then has to tolerate.
    Layer(const Layer&) = delete;
    Layer& operator=(const Layer&) = delete;

    int width() const { return _width; }
    int height() const { return _height; }

    // Locks for writing. The returned guard unlocks when it goes out of scope.
    LayerLock lock();

    // Blits the whole layer into dst, or over the entire render target when dst
    // is null. Scales if the sizes differ.
    void draw(const Rect* dst = nullptr);

    // Nearest by default, which is what a pixel-art or demo scale wants.
    // Linear is available for a soft upscale.
    void set_smooth(bool smooth);

    // Composes a packed pixel for row(). Named rather than left to the caller
    // because getting the channel order wrong yields a picture that looks right
    // in greyscale and has red and blue swapped in colour.
    static std::uint32_t pack(unsigned char r, unsigned char g, unsigned char b,
                              unsigned char a = 255)
    {
        return (static_cast<std::uint32_t>(a) << 24) |
               (static_cast<std::uint32_t>(r) << 16) |
               (static_cast<std::uint32_t>(g) << 8) |
               static_cast<std::uint32_t>(b);
    }

private:
    SDL_Renderer* _renderer; // not owned
    SDL_Texture* _texture;   // owned
    int _width;
    int _height;
};

} // namespace renderer
} // namespace gfx
