#pragma once

#include <memory>

#include <util/nocopy.hpp>

namespace gfx
{
namespace gles2
{

class Program;
class Texture;

//============================================================================
//
// Draws textures as screen-space quads
//
// The replacement for gfx::Context::set_ortho() / unset_ortho() and
// gfx::render_text(), which pushed a projection onto the fixed-function matrix
// stack, drew, and popped it. Here the projection is a uniform, built once per
// resize, and the quad is a vertex buffer.
//
// Pixel coordinates with the origin at the top left, which is what a HUD is
// naturally expressed in and what SDL uses everywhere else. GL's own convention
// is bottom-left, and reconciling the two once here is better than at every
// call site.
//
//     SpriteRenderer sprites(context.width(), context.height());
//     sprites.draw(text_texture, 10, 10);
//
//============================================================================
class SpriteRenderer
{
public:
    // Compiles its shader, so a GL context must be current. Throws ShaderError
    // if that fails.
    SpriteRenderer(int screen_width, int screen_height);
    ~SpriteRenderer();

private:
    DISALLOW_COPY_AND_ASSIGN(SpriteRenderer);

public:
    // Call on resize, and on rotation: the projection is baked from these.
    void set_screen_size(int width, int height);

    // Draws at the texture's own size, top-left at (x, y).
    void draw(const Texture& texture, int x, int y) const;

    // Draws scaled into w by h.
    void draw(const Texture& texture, int x, int y, int w, int h) const;

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

} // namespace gles2
} // namespace gfx
