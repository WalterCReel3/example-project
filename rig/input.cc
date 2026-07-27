#include <rig/input.hpp>

#include <SDL.h>

#include <cstring>

#include <util/logging.hpp>

namespace rig
{

namespace
{

const int button_count = static_cast<int>(Button::Count);

const char* const names[] = {"up", "down", "left", "right", "a",     "b",
                             "x",  "y",    "l",    "r",     "start", "select"};

// Keyboard bindings. Letters rather than a chorded layout because the scrolling
// message tells the reader "press a for palette", and that instruction should
// be literally true on a keyboard as well as on a pad.
Button from_key(SDL_Keycode key, bool& found)
{
    found = true;
    switch (key) {
    case SDLK_UP:
        return Button::Up;
    case SDLK_DOWN:
        return Button::Down;
    case SDLK_LEFT:
        return Button::Left;
    case SDLK_RIGHT:
        return Button::Right;
    case SDLK_a:
        return Button::A;
    case SDLK_b:
        return Button::B;
    case SDLK_x:
        return Button::X;
    case SDLK_y:
        return Button::Y;
    case SDLK_q:
        return Button::L;
    case SDLK_w:
        return Button::R;
    case SDLK_RETURN:
        return Button::Start;
    case SDLK_TAB:
        return Button::Select;
    default:
        break;
    }
    found = false;
    return Button::Count;
}

Button from_controller(Uint8 sdl_button, bool& found)
{
    found = true;
    switch (sdl_button) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP:
        return Button::Up;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
        return Button::Down;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
        return Button::Left;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
        return Button::Right;
    case SDL_CONTROLLER_BUTTON_A:
        return Button::A;
    case SDL_CONTROLLER_BUTTON_B:
        return Button::B;
    case SDL_CONTROLLER_BUTTON_X:
        return Button::X;
    case SDL_CONTROLLER_BUTTON_Y:
        return Button::Y;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
        return Button::L;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
        return Button::R;
    case SDL_CONTROLLER_BUTTON_START:
        return Button::Start;
    case SDL_CONTROLLER_BUTTON_BACK:
        return Button::Select;
    default:
        break;
    }
    found = false;
    return Button::Count;
}

// Raw joystick fallback: index order, which is a GUESS.
//
// There is no way to know what button 3 is on a device SDL does not recognise.
// The order below is the most common one on retro handhelds and is very likely
// wrong somewhere. It is here so the demo is controllable at all on an
// unrecognised pad, and the log says plainly that it is unverified — the point
// being to find out what a real device reports, not to pretend this is right.
Button from_joystick_index(int index, bool& found)
{
    found = true;
    switch (index) {
    case 0:
        return Button::A;
    case 1:
        return Button::B;
    case 2:
        return Button::X;
    case 3:
        return Button::Y;
    case 4:
        return Button::L;
    case 5:
        return Button::R;
    case 6:
        return Button::Select;
    case 7:
        return Button::Start;
    default:
        break;
    }
    found = false;
    return Button::Count;
}

} // namespace

const char* button_name(Button button)
{
    const int index = static_cast<int>(button);
    return (index >= 0 && index < button_count) ? names[index] : "?";
}

Pad::Pad()
    : _controller(nullptr)
    , _joystick(nullptr)
    , _instance(-1)
    , _description("keyboard only")
    , _mapped(false)
    , _quit(false)
    , _owns_subsystem(false)
{
    std::memset(_down, 0, sizeof(_down));
    std::memset(_pressed, 0, sizeof(_pressed));

    // JOYSTICK may already be up — gfx::System initialises it — but
    // GAMECONTROLLER usually is not, and SDL_GameControllerOpen needs it.
    // Reference counted by SDL, so initialising it again is harmless.
    if (SDL_WasInit(SDL_INIT_GAMECONTROLLER) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) == 0) {
            _owns_subsystem = true;
        } else {
            util::log_warning("no game controller subsystem: %s",
                              SDL_GetError());
        }
    }

    open_first_device();
}

Pad::~Pad()
{
    close_device();
    if (_owns_subsystem) {
        SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    }
}

void Pad::open_first_device()
{
    const int count = SDL_NumJoysticks();
    if (count < 1) {
        util::log_info("input: no pad attached, keyboard only");
        return;
    }

    // Everything about the device, at info level. This is the answer
    // planning/2026-07-25-target-validation/ step 4 is looking for, and it is
    // cheaper to log it always than to add a flag for it later.
    for (int i = 0; i < count; ++i) {
        const char* name = SDL_JoystickNameForIndex(i);
        char guid[64] = {0};
        SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(i), guid,
                                  sizeof(guid));
        util::log_info("input: pad %d '%s' guid %s, sdl mapping %s", i,
                       name ? name : "unnamed", guid,
                       SDL_IsGameController(i) ? "yes" : "NO");
    }

    if (SDL_IsGameController(0)) {
        _controller = SDL_GameControllerOpen(0);
        if (_controller) {
            _mapped = true;
            const char* name = SDL_GameControllerName(_controller);
            _description = std::string(name ? name : "pad") + " (sdl mapped)";
            _instance = SDL_JoystickInstanceID(
                SDL_GameControllerGetJoystick(_controller));
            util::log_info("input: using %s", _description.c_str());
            return;
        }
        util::log_warning("could not open game controller: %s", SDL_GetError());
    }

    _joystick = SDL_JoystickOpen(0);
    if (!_joystick) {
        util::log_warning("could not open joystick: %s", SDL_GetError());
        return;
    }

    _instance = SDL_JoystickInstanceID(_joystick);
    const char* name = SDL_JoystickName(_joystick);
    _description = std::string(name ? name : "pad") + " (raw, guessed mapping)";

    util::log_warning("input: %s has no SDL mapping — %d axes, %d buttons, "
                      "%d hats. Button order is a GUESS; verify on hardware "
                      "and add a mapping.",
                      name ? name : "pad", SDL_JoystickNumAxes(_joystick),
                      SDL_JoystickNumButtons(_joystick),
                      SDL_JoystickNumHats(_joystick));
}

void Pad::close_device()
{
    if (_controller) {
        SDL_GameControllerClose(_controller);
        _controller = nullptr;
    }
    if (_joystick) {
        SDL_JoystickClose(_joystick);
        _joystick = nullptr;
    }
}

void Pad::begin_frame()
{
    std::memset(_pressed, 0, sizeof(_pressed));
}

void Pad::set(Button button, bool is_down)
{
    const int index = static_cast<int>(button);
    if (index < 0 || index >= button_count) {
        return;
    }
    // The edge is recorded only on a transition, so a held button reports
    // pressed() exactly once however long it is held — and an auto-repeating
    // key event cannot fire a toggle twice.
    if (is_down && !_down[index]) {
        _pressed[index] = true;
    }
    _down[index] = is_down;
}

void Pad::handle_event(const SDL_Event& event)
{
    switch (event.type) {
    case SDL_QUIT:
        _quit = true;
        break;

    case SDL_KEYDOWN:
    case SDL_KEYUP: {
        if (event.key.repeat) {
            break;
        }
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
            _quit = true;
        }
        bool found = false;
        const Button button = from_key(event.key.keysym.sym, found);
        if (found) {
            set(button, event.type == SDL_KEYDOWN);
        }
        break;
    }

    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP: {
        if (event.cbutton.which != _instance) {
            break;
        }
        bool found = false;
        const Button button = from_controller(event.cbutton.button, found);
        if (found) {
            set(button, event.type == SDL_CONTROLLERBUTTONDOWN);
        }
        break;
    }

    case SDL_JOYBUTTONDOWN:
    case SDL_JOYBUTTONUP: {
        // Ignored when a controller is open: SDL raises both the joystick and
        // the controller event for the same press, so handling both would
        // toggle everything twice.
        if (_mapped || event.jbutton.which != _instance) {
            break;
        }
        bool found = false;
        const Button button = from_joystick_index(event.jbutton.button, found);
        if (found) {
            set(button, event.type == SDL_JOYBUTTONDOWN);
        }
        break;
    }

    case SDL_JOYHATMOTION: {
        if (_mapped || event.jhat.which != _instance || event.jhat.hat != 0) {
            break;
        }
        const Uint8 value = event.jhat.value;
        set(Button::Up, (value & SDL_HAT_UP) != 0);
        set(Button::Down, (value & SDL_HAT_DOWN) != 0);
        set(Button::Left, (value & SDL_HAT_LEFT) != 0);
        set(Button::Right, (value & SDL_HAT_RIGHT) != 0);
        break;
    }

    default:
        break;
    }
}

bool Pad::down(Button button) const
{
    const int index = static_cast<int>(button);
    return (index >= 0 && index < button_count) && _down[index];
}

bool Pad::pressed(Button button) const
{
    const int index = static_cast<int>(button);
    return (index >= 0 && index < button_count) && _pressed[index];
}

} // namespace rig
