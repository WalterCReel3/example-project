#ifndef WREEL_BASIC_APPLICATION_H
#define WREEL_BASIC_APPLICATION_H

#include <string>
#include <algorithm>
#include <vector>
#include <SDL_ttf.h>
#include <gfx/obj.hpp>
#include <gfx/utils.hpp>
#include <gfx/system.hpp>
#include <SDL.h>
#include <SDL_opengl.h>
#include <util/nocopy.hpp>
#include "input.hpp"

class Application
{
public:
    Application();
    ~Application();

private:
    DISALLOW_COPY_AND_ASSIGN(Application);

public:
    void load_image(const std::string& filename);
    void update_state();
    void render_hud();
    void render_scene();
    void handle_events();
    void game_loop();

private:
    // Flow control
    Uint32 _last_tick;
    bool _exit;

    // Subsystem
    InputManager _input;

    // Gfx resource
    // SDL_Surface *_primary_surface;
    gfx::System* _system;
    gfx::Context* _context;
    gfx::ObjModel _obj_model;
    TTF_Font *_font;
    // Rendering state
    gfx::Pov _pov;
};

#endif

// vim: set sts=2 sw=2 expandtab:
