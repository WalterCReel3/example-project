#include <loaders/obj.hpp>
#include <SDL.h>
#include <SDL_image.h>
#include <climits>
#include <cstdio>
#include <cmath>
#include <iostream>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <math/vector.hpp>
#include "application.hpp"
#include "globals.hpp"

using namespace std;
using namespace gfx;
using math::Vector3;

Application::Application()
    : _last_tick(0)
    , _exit(false)
    , _input()
    , _system(System::get_instance())
    , _context(0)
    , _obj_model()
    , _font(NULL)
    , _pov()
{
    _input.init();
    // _font = TTF_OpenFontIndex("data/FreebooterUpdated.ttf", 30, 0);
    _font = TTF_OpenFontIndex("data/Speedy.fon", 10, 0);
    if (!_font) {
        throw std::runtime_error("Could not load font resource");
    }

    _context = _system->create_context(string("Skratch"));

    loaders::load_obj_model(string("data/ico.obj"), _obj_model);
    logging << _obj_model.vertices.size() << std::endl;
    _obj_model.build_vbo();
}

Application::~Application()
{
    if (_system) {
        delete _system;
    }
}

void Application::load_image(const string& filename)
{
    // test_image = SDL_LoadBMP(filename.c_str());
    // if (test_image == NULL) {
    //   throw runtime_error("Couldn't load image resource " + filename); // }
}

void Application::handle_events()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // TODO: Eventually move to SDL_GetKeyboardState
        _input.translate_input(event);
    }
}

void Application::render_hud()
{
    char buf[4096];
    SDL_Color color;
    SDL_Rect t_rect;

    t_rect.x = 10;
    t_rect.y = 10;

    color.r = 255;
    color.g = 255;
    color.b = 255;
    // color.unused = 0;

    InputState& input = _input.get_state();

    snprintf(buf, sizeof(buf),
             "Pos (%f, %f, %f) Orien (%f, %f, %f) Joy (%d, %d, %d, %d, %d, %d)",
             _pov.position.x, _pov.position.y, _pov.position.z,
             _pov.orientation.yaw(), _pov.orientation.pitch(),
             _pov.orientation.roll(), input.joy_val_0, input.joy_val_1,
             input.joy_val_2, input.joy_val_3, input.joy_val_4,
             input.joy_val_5);

    _context->set_ortho();
    render_text(buf, _font, color, &t_rect);
    _context->unset_ortho();
}

void Application::render_scene()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    // Camera angle
    glRotatef(_pov.orientation.pitch(), 1.0f, 0.0f, 0.0f);
    glRotatef(_pov.orientation.yaw(),   0.0f, 1.0f, 0.0f);
    glRotatef(_pov.orientation.roll(),  0.0f, 0.0f, 1.0f);

    // Camera position
    glTranslatef(-_pov.position.x,
                 -_pov.position.y,
                 -_pov.position.z);

    for (int r = 0; r < 20; r++) {
        for (int c = 0; c < 20; c++) {
            _obj_model.render((float)r*10, 0.0f, (float)c*10);
        }
    }

    render_hud();
    SDL_GL_SwapWindow(_context->window());
}

void Application::update_state()
{
    Orientation &orien = _pov.orientation;
    Vector3 &pos = _pov.position;
    float d_angle = 0.15f;

    InputState& input = _input.get_state();

    if (input.input_tab[InputState::EXIT]) {
        _exit = true;
    }
    if (input.input_tab[InputState::PITCH_UP]) {
        orien.setPitch(orien.pitch() + d_angle);
    }
    if (input.input_tab[InputState::PITCH_DOWN]) {
        orien.setPitch(orien.pitch() - d_angle);
    }
    if (input.input_tab[InputState::YAW_UP]) {
        orien.setYaw(orien.yaw() + d_angle);
    }
    if (input.input_tab[InputState::YAW_DOWN]) {
        orien.setYaw(orien.yaw() - d_angle);
    }
    if (input.input_tab[InputState::ROLL_UP]) {
        orien.setRoll(orien.roll() + d_angle);
    }
    if (input.input_tab[InputState::ROLL_DOWN]) {
        orien.setRoll(orien.roll() - d_angle);
    }
    if (input.input_tab[InputState::FORWARD]) {
        pos.z = (pos.z - 0.1);
    }
    if (input.input_tab[InputState::BACKWARD]) {
        pos.z = (pos.z + 0.1);
    }
    if (input.input_tab[InputState::S_LEFT]) {
        pos.x = (pos.x - 0.1);
    }
    if (input.input_tab[InputState::S_RIGHT]) {
        pos.x = (pos.x + 0.1);
    }
    if (input.input_tab[InputState::UP]) {
        pos.y = (pos.y + 0.1);
    }
    if (input.input_tab[InputState::DOWN]) {
        pos.y = (pos.y - 0.1);
    }

    // Scale the joy values for position change
    float val0 = ((float)input.joy_val_0 / ((float)SHRT_MAX*2.0f));
    float val1 = ((float)input.joy_val_1 / ((float)SHRT_MAX*2.0f));

    pos.x += val0;
    pos.z += val1;

    orien.setYaw(orien.yaw() + input.mouse_rel_x + (input.joy_val_4 / 5000));
    orien.setPitch(orien.pitch() + input.mouse_rel_y + (input.joy_val_3 / 5000));
    if (orien.pitch() < -70.0) {
        orien.setPitch(-70.0);
    }
    if (orien.pitch() > 70.0) {
        orien.setPitch(70.0);
    }

    input.mouse_rel_x = 0;
    input.mouse_rel_y = 0;
}

void Application::game_loop()
{
    _last_tick = SDL_GetTicks();
    SDL_Event event;
    SDL_PollEvent(&event);
    while (!_exit) {
        handle_events();
        update_state();
        render_scene();
        SDL_Delay(10);
    }
}

// vim: sts=2 sw=2 expandtab:
