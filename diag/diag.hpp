#pragma once

// wreel-diag — what the SDL2 on this device actually DOES.
//
// wreel-probe reports what SDL *claims*: driver names, advertised flags,
// max texture size. That is the right tool for bring-up and the wrong one for
// this question, because the Miyoo Mini's render backend advertises
// capabilities it does not implement and returns success from calls that do
// nothing. A report built from SDL_GetRendererInfo would say the device
// supports render-to-texture and blend modes. It supports neither.
//
// So every check here DRAWS something and READS THE RESULT BACK. The verdict is
// what landed in the framebuffer, not what the API returned.
//
// The reference is the same binary on desktop-software, where SDL's own
// software renderer is a correct implementation of the contract. Two reports
// diffed against each other is the whole method: everything that says OK there
// and IGNORED here is a gap in the driver rather than a mistake in the test.

#include <cstdint>
#include <string>
#include <vector>

struct SDL_Renderer;
struct SDL_Window;

namespace diag
{

// ---------------------------------------------------------------------------
// Reporting
// ---------------------------------------------------------------------------

// Deliberately not pass/fail. On this device several of these are EXPECTED to
// come back IGNORED, and a tool that called that a failure would be reporting
// its own opinion rather than the device's behaviour.
enum class Verdict
{
    Ok,          // behaved as SDL2 specifies
    Ignored,     // the call reported success and changed nothing
    Wrong,       // it changed something, but not what was asked for
    Failed,      // the call itself reported failure
    Unsupported, // the call reported that it does not do this
    Skipped,     // could not be tested here — say why in the detail
    Info,        // a measurement rather than a judgement
};

void report_open(const char* path);
void report_close();

void blank();
void section(const char* title);
void note(const std::string& text);
void field(const char* key, const std::string& value);
void field(const char* key, long value);
void check(const char* name, Verdict verdict, const std::string& detail);

// Non-zero if anything came back Failed. Ignored and Wrong do NOT set it: on
// the Miyoo Mini they are the expected findings, and an exit code that flags
// them would make the tool useless in CI on the one target it matters for.
int report_exit_code();

// ---------------------------------------------------------------------------
// Pixel readback
// ---------------------------------------------------------------------------

// A frame, normalised to 32-bit 0xAARRGGBB whatever the source format was.
struct Image
{
    int width = 0;
    int height = 0;
    std::vector<std::uint32_t> pixels;

    bool valid() const { return width > 0 && height > 0; }
    std::uint32_t at(int x, int y) const;

    // Ignores alpha: the framebuffer's alpha byte is not meaningful on every
    // path, and no check here is about the alpha that reached the panel.
    bool rgb_near(int x, int y, std::uint32_t rgb, int tolerance = 8) const;
};

// How a frame was recovered. Which one is available is itself a finding, so it
// is reported rather than silently chosen.
enum class Readback
{
    None,
    RenderReadPixels, // SDL's own, correct wherever it is implemented
    Framebuffer,      // /dev/fb0 after present — ground truth for the panel
};

const char* readback_name(Readback source);

// Tries SDL_RenderReadPixels first, then the framebuffer. `source` reports
// which answered.
Image read_frame(SDL_Renderer* renderer, Readback* source);

// Opens /dev/fb0 and reports its geometry without drawing anything. Returns
// false if there is no framebuffer device, which is the desktop case.
bool framebuffer_probe(std::string* description);

// The panel's real size, straight from the framebuffer device.
//
// Worth more than SDL's answer on this target and it is not close. The video
// driver reports success from SDL_GetDesktopDisplayMode with a zeroed mode
// (D22/D24), and its mode list is a fixed set of ten that includes 800x600 on a
// 640x480 panel — so "the first mode" builds an 800x600 window whose textures
// are all refused by a renderer capped at 640x480. That is exactly how the
// first device run of this tool failed.
bool framebuffer_size(int* width, int* height);

// ---------------------------------------------------------------------------
// Checks
// ---------------------------------------------------------------------------

void report_environment();
void report_sdl_capability();

// The checks fall back to the window's size when the renderer reports a
// degenerate output, which this device does. Passed in rather than recovered
// with SDL_RenderGetWindow, because that call arrived in 2.0.22 and the escape
// hatch for this target still links a 2.0.20 runtime.
void set_window(SDL_Window* window);

// The transform between what we asked SDL to draw and what reached the panel.
// The Miyoo Mini's backend passes a fixed E_MI_GFX_ROTATE_180 to every blit and
// mirrors the destination x, and nothing recorded so far distinguishes "the
// panel is mounted upside down and this compensates" from "this is a bug".
// Asymmetric content and a readback settle it.
enum class Transform
{
    Unknown,
    Identity,
    Rotate180,
    FlipHorizontal,
    FlipVertical,
};

const char* transform_name(Transform transform);

// Draws a four-colour quadrant pattern full-screen and reads it back. Runs
// before the geometric checks so they can map their expectations through it.
Transform detect_transform(SDL_Renderer* renderer);

// Does SDL_UpdateTexture copy the caller's pixels, or keep the pointer?
//
// SDL2's contract is that it copies. This driver stores the pointer in a table
// and dereferences it at draw time, which makes SDL_CreateTextureFromSurface a
// use-after-free before it even returns.
//
// Checked WITHOUT freeing anything: the buffer is uploaded, then overwritten in
// place with a second colour and never uploaded again. A conforming driver
// still shows the first colour; one holding the pointer shows the second. The
// obvious test — free the buffer and draw — is undefined behaviour, and it took
// this tool down on the first device run rather than reporting anything.
void check_texture_upload_copies(SDL_Renderer* renderer);

void check_clear(SDL_Renderer* renderer);
void check_full_copy(SDL_Renderer* renderer, Transform transform);
void check_sub_rect_copy(SDL_Renderer* renderer, Transform transform);

// A sub-rect narrower than the texture, which fails differently: the driver
// derives the pixel format from pitch / rect width, so the ratio is only the
// bytes-per-pixel when the rect spans the full texture.
void check_narrow_sub_rect(SDL_Renderer* renderer);

// Where a partial destination rectangle lands, now that the panel is known to
// be mounted 180 degrees. The driver mirrors destination x by hand on top of
// the rotation that compensates for the mounting, which cancels on x and does
// not on y.
void check_dest_placement(SDL_Renderer* renderer, Transform transform);
void check_blend_mode(SDL_Renderer* renderer);
void check_colour_and_alpha_mod(SDL_Renderer* renderer);
void check_fill_rect(SDL_Renderer* renderer);
void check_texture_limits(SDL_Renderer* renderer);
void check_render_target(SDL_Renderer* renderer);
void check_audio();

} // namespace diag
