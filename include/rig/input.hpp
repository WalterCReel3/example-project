#pragma once

#include <string>

//============================================================================
//
// Buttons, from a gamepad or a keyboard
//
//     rig::Pad pad;
//     while (running) {
//         pad.begin_frame();
//         SDL_Event event;
//         while (SDL_PollEvent(&event)) {
//             pad.handle_event(event);
//         }
//         if (pad.pressed(rig::Button::A)) { ... }
//     }
//
// Named actions rather than scancodes and axis indices, because the same demo
// runs on a dev box with a keyboard and on a handheld whose pad nobody has
// enumerated yet.
//
// THREE PATHS, IN PREFERENCE ORDER
//
//   1. SDL_GameController, when SDL recognises the device. Buttons arrive
//      already mapped to A/B/X/Y/dpad, from SDL's own database.
//   2. A raw SDL_Joystick otherwise, with buttons taken by index and the dpad
//      from hat 0. **This mapping is a guess** — there is no way to know what
//      button 3 is on an unrecognised device — so it is logged as one.
//   3. The keyboard, always, in parallel with either.
//
// Path 2 is the one that matters here. planning/2026-07-25-target-validation/
// predicts that a Miyoo Mini will not have an SDL mapping, and skratch's
// hard-coded Xbox 360 axis indices are what that document says "certainly gets
// wrong". So this logs what it found in full — name, GUID, counts, and whether
// a mapping existed — which turns "what does the pad enumerate as" from a
// question into a line in the demo's log.
//
// A Pad initialises the game-controller subsystem itself and shuts it down in
// its destructor, so a program that constructs none pays nothing. Same
// arrangement as audio::Device.
//
//============================================================================

// Must match SDL exactly. The struct tags carry a leading underscore —
// SDL_joystick.h line 77 and SDL_gamecontroller.h line 60 in the pinned 2.32.10
// tree — and SDL_Event is a union, not a struct. A mismatched tag or class-key
// here is a hard error in any translation unit that also includes the real
// header, which is the same trap gfx/renderer/context.hpp documents for
// TTF_Font and audio/music.hpp for Mix_Music.
struct _SDL_Joystick;
typedef struct _SDL_Joystick SDL_Joystick;
struct _SDL_GameController;
typedef struct _SDL_GameController SDL_GameController;
union SDL_Event;

namespace rig
{

enum class Button {
    Up,
    Down,
    Left,
    Right,
    A,
    B,
    X,
    Y,
    L,
    R,
    Start,
    Select,
    Count // not a button; the size of the tables
};

const char* button_name(Button button);

class Pad
{
public:
    Pad();
    ~Pad();

    Pad(const Pad&) = delete;
    Pad& operator=(const Pad&) = delete;

    // Clears the edge flags. Call once per frame before pumping events, or
    // pressed() stays true for as long as the button is held.
    void begin_frame();

    // Feed every event; the ones that are not input are ignored.
    void handle_event(const SDL_Event& event);

    // Held right now.
    bool down(Button button) const;

    // Went down during this frame. What a menu or a toggle wants.
    bool pressed(Button button) const;

    // SDL_QUIT, or a window close. Distinct from Start/Select so a demo can
    // decide what quitting means.
    bool quit_requested() const { return _quit; }

    // What was found, for a log line or the scrolling message. "keyboard only"
    // when no pad is attached.
    const std::string& description() const { return _description; }

    // Whether the attached device came with an SDL mapping. False means the
    // button assignment is the guess described above, which is worth saying out
    // loud on a device nobody has tested.
    bool mapped() const { return _mapped; }

private:
    void open_first_device();
    void close_device();
    void set(Button button, bool is_down);

    SDL_GameController* _controller; // when _mapped
    SDL_Joystick* _joystick;         // when not
    int _instance;

    bool _down[static_cast<int>(Button::Count)];
    bool _pressed[static_cast<int>(Button::Count)];

    std::string _description;
    bool _mapped;
    bool _quit;
    bool _owns_subsystem;
};

} // namespace rig
