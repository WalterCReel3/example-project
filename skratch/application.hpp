#pragma once

#include <memory>
#include <string>

#include <SDL.h>
#include <SDL_ttf.h>

#include <gfx/gles2/context.hpp>
#include <gfx/gles2/mesh_buffer.hpp>
#include <gfx/gles2/program.hpp>
#include <gfx/gles2/sprite_renderer.hpp>
#include <gfx/system.hpp>

#include <rig/timing.hpp>
#include <util/nocopy.hpp>

#include "camera.hpp"
#include "input.hpp"

//============================================================================
//
// skratch — the worked example of how GL is structured now
//
// This demo is not the game. The game path is gfx::renderer and its own demo;
// see planning/2026-07-25-software-2d-sprites-tiling/. skratch is kept
// deliberately as the contrast between the 2016 fixed-function code this tree
// started with and the shader pipeline that replaced it:
//
//   2016                                    now
//   glRotatef / glTranslatef on the         matrices built with glm and
//   uploaded
//     driver's matrix stack                   as a uniform
//   gluPerspective from GLU                 glm::perspective, in code
//   GLuint handles inside gfx::ObjModel     gfx::Mesh data, gles2::MeshBuffer
//                                            residency, with separate lifetimes
//   set_ortho / unset_ortho pushing and     gles2::SpriteRenderer, whose
//     popping a projection                    projection is a uniform
//   state set globally and implicitly       objects that own what they set
//
// Everything it draws goes through gfx::gles2. There are no gl* calls in this
// application, which is the property that was worth the port: the demo can
// follow the renderer now instead of being welded to one pipeline.
//
//============================================================================
class Application
{
public:
    Application();
    ~Application();

private:
    DISALLOW_COPY_AND_ASSIGN(Application);

public:
    // dt is seconds since the previous frame. Camera motion is expressed as a
    // rate rather than a per-frame increment so the demo moves at the same
    // speed on a dev box and on a device.
    void update_state(float dt);
    void render_scene();
    void handle_events();
    void game_loop();

    // Renders `frames` frames, writes the last one to `path`, and returns
    // without entering the loop. There is no headless GL and therefore no unit
    // test for this renderer, so this is how "does it actually draw?" gets
    // answered — including on a device over SSH, where nobody can see the
    // panel.
    bool render_to_file(const std::string& path, int frames = 2);

private:
    void render_hud();
    std::string hud_text() const;
    void draw_frame();

    // Flow control
    rig::FrameClock _clock;
    bool _exit;

    InputManager _input;

    // First member, so it is destroyed last: SDL_Quit must not run before the
    // window, the GL objects or the font are gone.
    gfx::System _system;

    // Declared in construction order: the context must outlive everything
    // holding GL objects, because destroying it invalidates them.
    std::unique_ptr<gfx::gles2::Context> _context;
    std::unique_ptr<gfx::gles2::Program> _model_program;
    std::unique_ptr<gfx::gles2::MeshBuffer> _model;
    std::unique_ptr<gfx::gles2::SpriteRenderer> _sprites;

    TTF_Font* _font;

    Camera _camera;
};
