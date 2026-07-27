#include "application.hpp"

#include <gfx/gles2/texture.hpp>
#include <gfx/types.hpp>
#include <loaders/obj.hpp>
#include <rig/assets.hpp>
#include <util/format.hpp>
#include <util/logging.hpp>

#include <glm/gtc/matrix_transform.hpp>

#include <climits>
#include <stdexcept>

namespace
{

// GLSL ES 1.00, which is what the Mali handhelds speak and what desktop Mesa
// accepts through an ES profile. Two things are required here and merely
// optional in desktop GLSL, so a shader that omits them works on the dev box
// and fails on the device: the `#version 100` directive, and a precision
// qualifier for every float in the fragment stage.
//
// This is the whole of what the fixed-function pipeline did for the 2016 demo —
// transform by a matrix, interpolate a colour — written out.
const char* const model_vertex = R"(#version 100
uniform mat4 u_mvp;
attribute vec3 a_position;
attribute vec3 a_color;
varying vec3 v_color;
void main()
{
    gl_Position = u_mvp * vec4(a_position, 1.0);
    v_color = a_color;
}
)";

const char* const model_fragment = R"(#version 100
precision mediump float;
varying vec3 v_color;
void main()
{
    gl_FragColor = vec4(v_color, 1.0);
}
)";

// Matching the 2016 demo exactly, so the comparison is like for like:
// gluPerspective(60.0, aspect, 0.1, 100.0), and a 20x20 grid of instances 10
// units apart.
const float field_of_view = 60.0f;
const float near_plane = 0.1f;
const float far_plane = 100.0f;
const int grid = 20;
const float grid_spacing = 10.0f;

} // namespace

Application::Application()
    // Capped rather than uncapped: gfx::gles2::Context never asks for a swap
    // interval, so whether vsync throttles this at all is up to the driver.
    : _clock(60)
    , _exit(false)
    , _input()
    , _system()
    , _context()
    , _model_program()
    , _model()
    , _sprites()
    , _font(nullptr)
    , _camera()
{
    // SDL video, joystick, TTF and IMG are up by now: _system is a member and
    // its constructor has already run. TTF_OpenFontIndex below needs that, and
    // getting the order wrong reports "Library not initialized".
    _input.init();

    const std::string font_path = rig::asset_path("Speedy.fon");
    _font = TTF_OpenFontIndex(font_path.c_str(), 10, 0);
    if (!_font) {
        throw std::runtime_error("could not load " + font_path + ": " +
                                 TTF_GetError());
    }

    // The context comes first and is destroyed last: every GL object below is
    // owned by it, and deleting it invalidates them.
    _context.reset(new gfx::gles2::Context("Skratch", 1280, 720));

    _model_program.reset(
        new gfx::gles2::Program("model", model_vertex, model_fragment));

    // Data and residency are separate objects now. load_obj returns plain
    // vertices, colours and indexes; MeshBuffer uploads a copy, and the
    // CPU-side Mesh goes out of scope here, which is deliberate on a 128 MB
    // device.
    const gfx::Mesh mesh = loaders::load_obj(rig::asset_path("ico.obj"));
    util::log_info("ico.obj: %zu vertices, %zu indexes", mesh.vertices.size(),
                   mesh.indexes.size());
    _model.reset(new gfx::gles2::MeshBuffer(mesh));

    _sprites.reset(
        new gfx::gles2::SpriteRenderer(_context->width(), _context->height()));

    util::log_info("skratch: %s / %s", _context->version().c_str(),
                   _context->renderer_name().c_str());
}

Application::~Application()
{
    if (_font) {
        TTF_CloseFont(_font);
    }
    // The GL objects must be destroyed before the context that owns them.
    // unique_ptr members are destroyed in reverse declaration order, which is
    // already that — stated because the ordering is load-bearing rather than
    // incidental.
}

void Application::handle_events()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // TODO: Eventually move to SDL_GetKeyboardState
        _input.translate_input(event);
    }
}

std::string Application::hud_text() const
{
    const InputState& input = _input.get_state();

    return util::format(
        "Pos (%.2f, %.2f, %.2f) Orien (%.2f, %.2f, %.2f) "
        "Joy (%d, %d, %d, %d, %d, %d)",
        static_cast<double>(_camera.position.x),
        static_cast<double>(_camera.position.y),
        static_cast<double>(_camera.position.z),
        static_cast<double>(_camera.yaw), static_cast<double>(_camera.pitch),
        static_cast<double>(_camera.roll), input.joy_val_0, input.joy_val_1,
        input.joy_val_2, input.joy_val_3, input.joy_val_4, input.joy_val_5);
}

void Application::render_hud()
{
    SDL_Color white;
    white.r = 255;
    white.g = 255;
    white.b = 255;
    white.a = 255;

    const std::string text = hud_text();

    // Rasterised and uploaded every frame, as the 2016 version was. It is the
    // obvious thing to cache, and caching would buy nothing here: every value
    // in this string changes as the camera moves, so it would miss on almost
    // every frame. A *static* HUD element would be worth a Texture held once.
    SDL_Surface* rendered = TTF_RenderUTF8_Blended(_font, text.c_str(), white);
    if (!rendered) {
        util::log_error("could not render HUD text: %s", TTF_GetError());
        return;
    }

    try {
        const gfx::gles2::Texture texture(rendered);
        _sprites->draw(texture, 10, 10);
    } catch (const std::exception& e) {
        // A failed upload should cost the HUD, not the frame.
        util::log_error("HUD: %s", e.what());
    }

    SDL_FreeSurface(rendered);
}

void Application::render_scene()
{
    draw_frame();
    _context->present();
}

// Everything except the swap, so a screenshot can be taken from the back buffer
// while it is still defined.
void Application::draw_frame()
{
    // 0, 0, 0.5 — the same blue the fixed-function set_3d() cleared to.
    _context->clear(0.0f, 0.0f, 0.5f);

    const float aspect = static_cast<float>(_context->width()) /
                         static_cast<float>(_context->height());
    const glm::mat4 projection = glm::perspective(
        glm::radians(field_of_view), aspect, near_plane, far_plane);
    const glm::mat4 view = _camera.view();

    _model_program->use();

    // One MeshBuffer drawn 400 times with a different model matrix. The 2016
    // version pushed and popped the matrix stack per instance and re-bound its
    // buffers inside ObjModel::render; the buffers are still bound per draw
    // here, which is the honest like-for-like and the obvious thing to improve
    // if it ever showed up in a profile.
    for (int r = 0; r < grid; ++r) {
        for (int c = 0; c < grid; ++c) {
            // Negative, matching ObjModel::render's glTranslatef(-x, -y, -z).
            const glm::vec3 offset(-static_cast<float>(r) * grid_spacing, 0.0f,
                                   -static_cast<float>(c) * grid_spacing);
            const glm::mat4 model = glm::translate(glm::mat4(1.0f), offset);

            _model_program->set_uniform("u_mvp", projection * view * model);
            _model->draw(*_model_program, "a_position", "a_color");
        }
    }

    render_hud();
}

bool Application::render_to_file(const std::string& path, int frames)
{
    // More than one frame because a driver may present the first before
    // anything is resident; the last one drawn is the one saved.
    for (int i = 0; i < frames; ++i) {
        handle_events();
        draw_frame();
        if (i + 1 < frames) {
            _context->present();
        }
    }
    return _context->save_screenshot(path);
}

void Application::update_state(float dt)
{
    // Every constant in this function was a per-frame increment, tuned against
    // whatever rate the old SDL_Delay(10) loop happened to produce. They are
    // kept verbatim and scaled by how long this frame actually took relative to
    // 60 Hz, so the demo moves identically at 60 fps and correctly at any other
    // rate. Rewriting them as units-per-second would read better and would
    // quietly retune the demo, which is not what this change is for.
    const float frame_scale = dt * 60.0f;

    const float d_angle = 0.15f * frame_scale;
    const float d_move = 0.1f * frame_scale;
    InputState& input = _input.get_state();

    if (input.input_tab[InputState::EXIT]) {
        _exit = true;
    }
    if (input.input_tab[InputState::PITCH_UP]) {
        _camera.pitch += d_angle;
    }
    if (input.input_tab[InputState::PITCH_DOWN]) {
        _camera.pitch -= d_angle;
    }
    if (input.input_tab[InputState::YAW_UP]) {
        _camera.yaw += d_angle;
    }
    if (input.input_tab[InputState::YAW_DOWN]) {
        _camera.yaw -= d_angle;
    }
    if (input.input_tab[InputState::ROLL_UP]) {
        _camera.roll += d_angle;
    }
    if (input.input_tab[InputState::ROLL_DOWN]) {
        _camera.roll -= d_angle;
    }
    if (input.input_tab[InputState::FORWARD]) {
        _camera.position.z -= d_move;
    }
    if (input.input_tab[InputState::BACKWARD]) {
        _camera.position.z += d_move;
    }
    if (input.input_tab[InputState::S_LEFT]) {
        _camera.position.x -= d_move;
    }
    if (input.input_tab[InputState::S_RIGHT]) {
        _camera.position.x += d_move;
    }
    if (input.input_tab[InputState::UP]) {
        _camera.position.y += d_move;
    }
    if (input.input_tab[InputState::DOWN]) {
        _camera.position.y -= d_move;
    }

    // Analogue sticks are 16-bit signed; scaling by 2*SHRT_MAX gives roughly
    // +/- 0.5 units per 60 Hz frame at full deflection. A held stick is a
    // velocity, so it scales with frame length like the keys above.
    const float scale = static_cast<float>(SHRT_MAX) * 2.0f;
    _camera.position.x +=
        static_cast<float>(input.joy_val_0) / scale * frame_scale;
    _camera.position.z +=
        static_cast<float>(input.joy_val_1) / scale * frame_scale;

    // Mouse deltas are the exception, and must NOT be scaled. input_tab and the
    // stick axes report a state that was held for the whole frame; mouse_rel_*
    // is a displacement already accumulated over it. Scaling it by frame length
    // would make look sensitivity depend on the frame rate, which is the bug
    // this whole change exists to remove.
    _camera.yaw += static_cast<float>(input.mouse_rel_x) +
                   static_cast<float>(input.joy_val_4) / 5000.0f * frame_scale;
    _camera.pitch +=
        static_cast<float>(input.mouse_rel_y) +
        static_cast<float>(input.joy_val_3) / 5000.0f * frame_scale;

    if (_camera.pitch < -70.0f) {
        _camera.pitch = -70.0f;
    }
    if (_camera.pitch > 70.0f) {
        _camera.pitch = 70.0f;
    }

    input.mouse_rel_x = 0;
    input.mouse_rel_y = 0;
}

void Application::game_loop()
{
    // Discards the time spent loading the font, the model and the shaders. That
    // interval would otherwise arrive as the first frame's delta and get
    // clamped, which is harmless but shows up as one stuttered frame at start.
    _clock.reset();

    while (!_exit) {
        // Sleeps out the remainder of the frame's budget, then reports how long
        // the frame actually took.
        const float dt = static_cast<float>(_clock.tick());

        handle_events();
        update_state(dt);
        render_scene();
    }
}
