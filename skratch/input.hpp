#ifndef __WREEL_INPUTS_H__
#define __WREEL_INPUTS_H__

#include <algorithm>
#include <vector>
#include <SDL.h>
#include <util/nocopy.hpp>

class InputState
{
public:
    const static int PITCH_UP;
    const static int PITCH_DOWN;
    const static int YAW_UP;
    const static int YAW_DOWN;
    const static int ROLL_UP;
    const static int ROLL_DOWN;
    const static int FORWARD;
    const static int BACKWARD;
    const static int S_LEFT;
    const static int S_RIGHT;
    const static int UP;
    const static int DOWN;
    const static int EXIT;

public:
    InputState()
        : input_tab()
        , mouse_rel_x(0)
        , mouse_rel_y(0)
        , joy_val_0(0)
        , joy_val_1(0)
        , joy_val_2(0)
        , joy_val_3(0)
        , joy_val_4(0)
        , joy_val_5(0)
    {
        std::fill(input_tab, input_tab + sizeof(input_tab), false);
    }

private:
    DISALLOW_COPY_AND_ASSIGN(InputState);

public:
    bool input_tab[13]; // FIXME: use dynamic event mapping
    int mouse_rel_x;
    int mouse_rel_y;

    // TODO: This should be an enumerated set for a dynamic
    //       number of joysticks
    int joy_val_0; // XBox 360 left joy horz
    int joy_val_1; // XBox 360 left joy vert
    int joy_val_2; // XBox 360 left and right triggers?
    int joy_val_3; // XBox 360 right joy vert
    int joy_val_4; // XBox 360 right joy horz
    int joy_val_5; // XBox 360 right trigger

    // Button0  -> A
    // Button1  -> B
    // Button2  -> X
    // Button3  -> Y
    // Button4  -> LB
    // Button5  -> RB
    // Button6  -> Back
    // Button7  -> Start
    // Button8  -> Xbox
    // Button9  -> Left stick click
    // Button10 -> Right stick click

    // Hat0 -> DPAD
};

class InputManager
{
public:
    InputManager();

public:
    void init();
    void on_key_down(const SDL_Event& event);
    void on_key_up(const SDL_Event& event);
    void on_mouse_motion(const SDL_Event& event);
    void on_joy_motion(const SDL_Event& event);
    void on_joy_hat(const SDL_Event& event);
    void translate_input(const SDL_Event& event);
    InputState& get_state()
    {
        return _input_state;
    }

private:
    InputState _input_state;
    SDL_Joystick *_joystick;
};

// vim: set sts=2 sw=2 expandtab:
#endif
