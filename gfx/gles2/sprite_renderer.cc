#include <gfx/gles2/sprite_renderer.hpp>

#include <gfx/gles2/api.hpp>
#include <gfx/gles2/program.hpp>
#include <gfx/gles2/texture.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>

namespace gfx
{
namespace gles2
{

namespace
{

// GLSL ES 1.00. Two things here are required by ES and merely optional in
// desktop GLSL, which is exactly the trap the gl33-first ordering would have
// walked into: the `#version 100` directive, and an explicit precision for
// every float in the fragment shader. A shader missing the precision qualifier
// compiles on Mesa and fails on Mali.
const char* const vertex_source = R"(#version 100
uniform mat4 u_projection;
uniform vec4 u_rect;          // x, y, w, h in pixels
attribute vec2 a_corner;      // unit quad, 0..1
varying vec2 v_texcoord;
void main()
{
    vec2 pixel = u_rect.xy + a_corner * u_rect.zw;
    gl_Position = u_projection * vec4(pixel, 0.0, 1.0);
    v_texcoord = a_corner;
}
)";

const char* const fragment_source = R"(#version 100
precision mediump float;
uniform sampler2D u_texture;
varying vec2 v_texcoord;
void main()
{
    gl_FragColor = texture2D(u_texture, v_texcoord);
}
)";

// One unit quad, reused for every sprite. The rectangle is a uniform rather
// than per-sprite vertex data, so drawing a different sprite costs two uniform
// writes and no buffer traffic.
const float quad_corners[] = {
    0.0f, 0.0f, // top left
    1.0f, 0.0f, // top right
    0.0f, 1.0f, // bottom left
    1.0f, 1.0f, // bottom right
};

} // namespace

struct SpriteRenderer::Impl {
    Program program;
    GLuint quad;
    glm::mat4 projection;
    int corner_attribute;

    Impl()
        : program("sprite", vertex_source, fragment_source)
        , quad(0)
        , projection(1.0f)
        , corner_attribute(-1)
    {
    }
};

SpriteRenderer::SpriteRenderer(int screen_width, int screen_height)
    : _impl(new Impl())
{
    gl::GenBuffers(1, &_impl->quad);
    gl::BindBuffer(GL_ARRAY_BUFFER, _impl->quad);
    gl::BufferData(GL_ARRAY_BUFFER, sizeof(quad_corners), quad_corners,
                   GL_STATIC_DRAW);
    gl::BindBuffer(GL_ARRAY_BUFFER, 0);

    _impl->corner_attribute = _impl->program.attribute_location("a_corner");

    set_screen_size(screen_width, screen_height);
}

SpriteRenderer::~SpriteRenderer()
{
    if (_impl->quad != 0) {
        gl::DeleteBuffers(1, &_impl->quad);
    }
}

void SpriteRenderer::set_screen_size(int width, int height)
{
    // Top-left origin: `top` is 0 and `bottom` is height, which is the inverse
    // of glm::ortho's usual argument order and is what flips GL's y axis to
    // match SDL's.
    _impl->projection =
        glm::ortho(0.0f, static_cast<float>(width), static_cast<float>(height),
                   0.0f, -1.0f, 1.0f);
}

void SpriteRenderer::draw(const Texture& texture, int x, int y) const
{
    draw(texture, x, y, texture.width(), texture.height());
}

void SpriteRenderer::draw(const Texture& texture, int x, int y, int w,
                          int h) const
{
    if (_impl->corner_attribute < 0) {
        return;
    }

    _impl->program.use();
    _impl->program.set_uniform("u_projection", _impl->projection);
    _impl->program.set_uniform("u_texture", 0);

    const int rect_location = _impl->program.uniform_location("u_rect");
    if (rect_location >= 0) {
        const float rect[4] = {static_cast<float>(x), static_cast<float>(y),
                               static_cast<float>(w), static_cast<float>(h)};
        gl::Uniform4fv(rect_location, 1, rect);
    }

    texture.bind(0);

    const GLuint corner = static_cast<GLuint>(_impl->corner_attribute);
    gl::BindBuffer(GL_ARRAY_BUFFER, _impl->quad);
    gl::EnableVertexAttribArray(corner);
    gl::VertexAttribPointer(corner, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    // A HUD draws over the scene, so depth testing is off for the quad and
    // restored afterwards rather than left off for whatever draws next.
    gl::Disable(GL_DEPTH_TEST);
    gl::DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    gl::Enable(GL_DEPTH_TEST);

    gl::DisableVertexAttribArray(corner);
    gl::BindBuffer(GL_ARRAY_BUFFER, 0);
}

} // namespace gles2
} // namespace gfx
