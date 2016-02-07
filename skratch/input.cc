#include "input.hpp"

const int InputState::PITCH_UP   = 0;
const int InputState::PITCH_DOWN = 1;
const int InputState::YAW_UP     = 2;
const int InputState::YAW_DOWN   = 3;
const int InputState::ROLL_UP    = 4;
const int InputState::ROLL_DOWN  = 5;
const int InputState::FORWARD    = 6;
const int InputState::BACKWARD   = 7;
const int InputState::S_LEFT     = 8;
const int InputState::S_RIGHT    = 9;
const int InputState::UP         = 10;
const int InputState::DOWN       = 11;
const int InputState::EXIT       = 12;

InputManager::InputManager()
    : _input_state(),
      _joystick(NULL)
{
}

void InputManager::init()
{
    int njoy = SDL_NumJoysticks();

    if (njoy > 0) {
        // Init the first available joystick
        SDL_JoystickEventState(SDL_ENABLE);
        _joystick = SDL_JoystickOpen(0);
        // TODO: Inspect the name to get the right mapping
        // std::cout << SDL_JoystickName(0) << std::endl;
    }
}

void InputManager::on_key_down(const SDL_Event& event)
{
    switch (event.key.keysym.sym) {
    case SDLK_ESCAPE:
        _input_state.input_tab[InputState::EXIT] = true;
        break;
    case SDLK_SPACE:
        _input_state.input_tab[InputState::UP] = true;
        break;
    case SDLK_LCTRL:
        _input_state.input_tab[InputState::DOWN] = true;
        break;
    case SDLK_UP:
        _input_state.input_tab[InputState::PITCH_DOWN] = true;
        break;
    case SDLK_DOWN:
        _input_state.input_tab[InputState::PITCH_UP] = true;
        break;
    case SDLK_RIGHT:
        _input_state.input_tab[InputState::YAW_UP] = true;
        break;
    case SDLK_LEFT:
        _input_state.input_tab[InputState::YAW_DOWN] = true;
        break;
    case SDLK_w:
        _input_state.input_tab[InputState::FORWARD] = true;
        break;
    case SDLK_s:
        _input_state.input_tab[InputState::BACKWARD] = true;
        break;
    case SDLK_a:
        _input_state.input_tab[InputState::S_LEFT] = true;
        break;
    case SDLK_d:
        _input_state.input_tab[InputState::S_RIGHT] = true;
        break;
    default:
        break;
    }
}

void InputManager::on_key_up(const SDL_Event& event)
{
    switch (event.key.keysym.sym) {
    case SDLK_SPACE:
        _input_state.input_tab[InputState::UP] = false;
        break;
    case SDLK_LCTRL:
        _input_state.input_tab[InputState::DOWN] = false;
        break;
    case SDLK_UP:
        _input_state.input_tab[InputState::PITCH_DOWN] = false;
        break;
    case SDLK_DOWN:
        _input_state.input_tab[InputState::PITCH_UP] = false;
        break;
    case SDLK_RIGHT:
        _input_state.input_tab[InputState::YAW_UP] = false;
        break;
    case SDLK_LEFT:
        _input_state.input_tab[InputState::YAW_DOWN] = false;
        break;
    case SDLK_w:
        _input_state.input_tab[InputState::FORWARD] = false;
        break;
    case SDLK_s:
        _input_state.input_tab[InputState::BACKWARD] = false;
        break;
    case SDLK_a:
        _input_state.input_tab[InputState::S_LEFT] = false;
        break;
    case SDLK_d:
        _input_state.input_tab[InputState::S_RIGHT] = false;
        break;
    default:
        break;
    }
}

void InputManager::on_mouse_motion(const SDL_Event& event)
{
    _input_state.mouse_rel_x = event.motion.xrel;
    _input_state.mouse_rel_y = event.motion.yrel;
}

void InputManager::on_joy_motion(const SDL_Event& event)
{
    int value = 0;
    // trim the value down for the usual non-centering issue
    if ((event.jaxis.value > 9000) || (event.jaxis.value < -9000)) {
        value = event.jaxis.value;
    }

    int axis = event.jaxis.axis;
    switch (axis) {
    case 0:
        _input_state.joy_val_0 = value;
        break;
    case 1:
        _input_state.joy_val_1 = value;
        break;
    case 2:
        _input_state.joy_val_2 = value;
        break;
    case 3:
        _input_state.joy_val_3 = value;
        break;
    case 4:
        _input_state.joy_val_4 = value;
        break;
    case 5:
        _input_state.joy_val_5 = value;
        break;
    }
}

void InputManager::on_joy_hat(const SDL_Event& event)
{
    _input_state.input_tab[InputState::FORWARD] = false;
    _input_state.input_tab[InputState::BACKWARD] = false;
    _input_state.input_tab[InputState::S_LEFT] = false;
    _input_state.input_tab[InputState::S_RIGHT] = false;

    if (event.jhat.value & SDL_HAT_UP) {
        _input_state.input_tab[InputState::FORWARD] = true;
    } else if (event.jhat.value & SDL_HAT_DOWN) {
        _input_state.input_tab[InputState::BACKWARD] = true;
    } else if (event.jhat.value & SDL_HAT_LEFT) {
        _input_state.input_tab[InputState::S_LEFT] = true;
    } else if (event.jhat.value & SDL_HAT_RIGHT) {
        _input_state.input_tab[InputState::S_RIGHT] = true;
    }
}

void InputManager::translate_input(const SDL_Event& event)
{
    switch (event.type) {
    case SDL_KEYDOWN:
        on_key_down(event);
        // cout << "SDL EVENTS: key down ("
        //      << SDL_GetKeyName(event.key.keysym.sym)
        //      << ")"
        //      << endl;
        break;
    case SDL_KEYUP:
        on_key_up(event);
        // cout << "SDL EVENTS: key up   ("
        //      << SDL_GetKeyName(event.key.keysym.sym)
        //      << ")"
        //      << endl;
        break;
    case SDL_MOUSEMOTION:
        // cout << "SDL EVENTS: Mouse rel (" << event.motion.xrel
        //      << "," << event.motion.yrel << ")" << endl;
        on_mouse_motion(event);
        break;
    case SDL_JOYAXISMOTION:
        on_joy_motion(event);
        break;
    case SDL_JOYHATMOTION:
        on_joy_hat(event);
        break;
    case SDL_QUIT:
        _input_state.input_tab[InputState::EXIT] = true;
        break;
    default:
        // nop
        break;
    }
}

// vim: set sts=2 sw=2 expandtab:
