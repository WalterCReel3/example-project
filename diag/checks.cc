// The conformance checks.
//
// Each one draws something whose correct result is known, presents, reads the
// frame back and reports what actually landed. None of them trusts a return
// code: on this device SDL_SetTextureBlendMode, SDL_SetTextureColorMod and
// SDL_RenderClear all return 0 and do nothing.
//
// Every check is written to be meaningful on a correct implementation too. Run
// on desktop-software they should all say OK, and that run is the control.
//
// Two rules learned from the first device run, both of which cost findings:
//
//   A check that cannot establish its precondition reports SKIPPED, never a
//   verdict. The clear check painted a white background first and read the
//   screen afterwards; when the background failed to upload it read black,
//   which is also what a working clear-to-black looks like, and reported WRONG
//   about a driver that had not been asked to do anything.
//
//   Nothing here draws a texture larger than the renderer will create. The
//   panel and the texture cap are not the same number on every device — on a
//   Mini Flip they differ by design — so the drawing size is the smaller of
//   the two rather than the window.

#include "diag.hpp"

#include <SDL.h>

#include <algorithm>
#include <cstdlib>

#include <util/format.hpp>

namespace diag
{

namespace
{

constexpr std::uint32_t colour_red = 0xffe00000;
constexpr std::uint32_t colour_green = 0xff00e000;
constexpr std::uint32_t colour_blue = 0xff0000e0;
constexpr std::uint32_t colour_white = 0xffffffff;
constexpr std::uint32_t colour_black = 0xff000000;

// Used only to poison the driver's staging buffer, so it must not collide with
// any colour a check expects to see.
constexpr std::uint32_t colour_magenta = 0xffe000e0;

struct Frame
{
    Image image;
    Readback source = Readback::None;
};

Frame present_and_read(SDL_Renderer* renderer)
{
    Frame frame;
    SDL_RenderPresent(renderer);
    // The mini driver's present is an fbdev pan; give the flip a moment to
    // land before reading the visible buffer back out from under it.
    SDL_Delay(50);
    frame.image = read_frame(renderer, &frame.source);
    return frame;
}

// A texture, and the pixels it was uploaded from, kept together.
//
// The pairing is not tidiness. This driver's SDL_UpdateTexture stores the
// caller's pointer instead of copying, and dereferences it at draw time — so a
// buffer that goes out of scope while its texture is still alive is read after
// free. The first device run proved it the hard way: the helper this replaces
// built the pixels in a local vector, returned the texture, and the tool
// segfaulted on the first check that actually drew anything.
//
// Owning both together makes the lifetime correct on this target and costs
// nothing on any other. gfx::renderer::Layer already does the same, which is
// why Layer has always been safe here and draw_surface() has not.
class Surface
{
public:
    Surface(SDL_Renderer* renderer, int w, int h)
        : _pixels(static_cast<std::size_t>(w) * h, 0)
        , _width(w)
        , _height(h)
    {
        _texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                     SDL_TEXTUREACCESS_STREAMING, w, h);
    }

    ~Surface()
    {
        if (_texture) {
            SDL_DestroyTexture(_texture);
        }
    }

    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;

    bool valid() const { return _texture != nullptr; }
    SDL_Texture* texture() const { return _texture; }

    int pitch() const
    {
        return _width * static_cast<int>(sizeof(std::uint32_t));
    }

    void fill(std::uint32_t argb)
    {
        std::fill(_pixels.begin(), _pixels.end(), argb);
    }

    // Horizontal bands, for the sub-rectangle check.
    void bands(std::uint32_t top, std::uint32_t middle, std::uint32_t bottom)
    {
        for (int y = 0; y < _height; ++y) {
            const std::uint32_t band = y < _height / 3         ? top
                                       : y < (2 * _height) / 3 ? middle
                                                               : bottom;
            for (int x = 0; x < _width; ++x) {
                _pixels[static_cast<std::size_t>(y) * _width + x] = band;
            }
        }
    }

    // Four quadrants, four colours. Asymmetric in both axes, which is what
    // lets it tell a 180-degree rotation from a mirror — a pattern that is
    // only asymmetric horizontally cannot.
    void quadrants()
    {
        for (int y = 0; y < _height; ++y) {
            for (int x = 0; x < _width; ++x) {
                const bool right = x >= _width / 2;
                const bool bottom = y >= _height / 2;
                std::uint32_t argb = colour_red; // top-left
                if (right && !bottom) {
                    argb = colour_green; // top-right
                } else if (!right && bottom) {
                    argb = colour_blue; // bottom-left
                } else if (right && bottom) {
                    argb = colour_white; // bottom-right
                }
                _pixels[static_cast<std::size_t>(y) * _width + x] = argb;
            }
        }
    }

    void upload()
    {
        SDL_UpdateTexture(_texture, nullptr, _pixels.data(), pitch());
    }

private:
    SDL_Texture* _texture = nullptr;
    std::vector<std::uint32_t> _pixels;
    int _width;
    int _height;
};

// Samples well inside a quadrant rather than at the very corner: a scaled or
// rounded blit can land a pixel or two off, and this is not a check about
// sub-pixel placement.
std::uint32_t sample_quadrant(const Image& image, bool right, bool bottom)
{
    const int x = right ? (image.width * 3) / 4 : image.width / 4;
    const int y = bottom ? (image.height * 3) / 4 : image.height / 4;
    return image.at(x, y);
}

std::uint32_t centre_of(const Image& image)
{
    return image.at(image.width / 2, image.height / 2);
}

bool near(std::uint32_t a, std::uint32_t b, int tolerance = 12)
{
    const auto diff = [&](int shift) {
        const int d = static_cast<int>((a >> shift) & 0xff) -
                      static_cast<int>((b >> shift) & 0xff);
        return d < 0 ? -d : d;
    };
    return diff(16) <= tolerance && diff(8) <= tolerance &&
           diff(0) <= tolerance;
}

void clear_to(SDL_Renderer* renderer, std::uint32_t argb)
{
    SDL_SetRenderDrawColor(renderer, (argb >> 16) & 0xff, (argb >> 8) & 0xff,
                           argb & 0xff, 0xff);
    SDL_RenderClear(renderer);
}

SDL_Window* main_window = nullptr;

void output_size(SDL_Renderer* renderer, int* w, int* h)
{
    *w = 0;
    *h = 0;
    if ((SDL_GetRendererOutputSize(renderer, w, h) != 0 || *w <= 0 ||
         *h <= 0) &&
        main_window) {
        SDL_GetWindowSize(main_window, w, h);
    }
    if (*w <= 0 || *h <= 0) {
        *w = 640;
        *h = 480;
    }
}

// The size a full-screen texture may actually be: the output, capped by what
// the renderer will create. These are the same number on a sane device and are
// not on a Mini Flip, whose framebuffer is detected at runtime while the
// texture limit stays at its compiled-in 640x480.
void draw_size(SDL_Renderer* renderer, int* w, int* h)
{
    output_size(renderer, w, h);

    SDL_RendererInfo info;
    if (SDL_GetRendererInfo(renderer, &info) != 0) {
        return;
    }
    if (info.max_texture_width > 0 && *w > info.max_texture_width) {
        *w = info.max_texture_width;
    }
    if (info.max_texture_height > 0 && *h > info.max_texture_height) {
        *h = info.max_texture_height;
    }
}

// Paints the whole screen a known colour by the one route this device is known
// to honour — a full-screen copy of a solid texture — so a check that needs a
// known background does not depend on SDL_RenderClear working.
//
// Returns false if it could not, and every caller treats that as SKIPPED. A
// background that silently failed to paint is what turned the clear check into
// a wrong verdict on the first device run.
bool paint_background(SDL_Renderer* renderer, std::uint32_t argb)
{
    int w = 0;
    int h = 0;
    draw_size(renderer, &w, &h);

    Surface surface(renderer, w, h);
    if (!surface.valid()) {
        return false;
    }
    surface.fill(argb);
    surface.upload();
    return SDL_RenderCopy(renderer, surface.texture(), nullptr, nullptr) == 0;
}

} // namespace

void set_window(SDL_Window* window)
{
    main_window = window;
}

const char* transform_name(Transform transform)
{
    switch (transform) {
    case Transform::Unknown:
        return "unknown";
    case Transform::Identity:
        return "identity";
    case Transform::Rotate180:
        return "rotated 180";
    case Transform::FlipHorizontal:
        return "mirrored horizontally";
    case Transform::FlipVertical:
        return "mirrored vertically";
    }
    return "?";
}

// The §6 experiment, automated. Nothing recorded so far distinguishes "the
// panel is mounted upside down and the fixed E_MI_GFX_ROTATE_180 compensates"
// from "the rotation is a bug", because coppers' full-screen content is a field
// of horizontal bars that looks identical rotated. Four coloured quadrants do
// not.
Transform detect_transform(SDL_Renderer* renderer)
{
    section("Orientation");

    int w = 0;
    int h = 0;
    draw_size(renderer, &w, &h);
    field("drawing size", util::format("%dx%d", w, h));

    Surface surface(renderer, w, h);
    if (!surface.valid()) {
        check("quadrant texture", Verdict::Failed, SDL_GetError());
        return Transform::Unknown;
    }
    surface.quadrants();
    surface.upload();

    SDL_RenderCopy(renderer, surface.texture(), nullptr, nullptr);
    const Frame frame = present_and_read(renderer);

    if (!frame.image.valid()) {
        check("readback", Verdict::Skipped,
              "no readback path — orientation cannot be determined here");
        return Transform::Unknown;
    }

    field("readback via", readback_name(frame.source));

    const std::uint32_t tl = sample_quadrant(frame.image, false, false);
    const std::uint32_t tr = sample_quadrant(frame.image, true, false);
    const std::uint32_t bl = sample_quadrant(frame.image, false, true);
    const std::uint32_t br = sample_quadrant(frame.image, true, true);

    field("expected", "TL=red TR=green BL=blue BR=white");
    field("observed", util::format("TL=%06x TR=%06x BL=%06x BR=%06x",
                                   tl & 0xffffff, tr & 0xffffff,
                                   bl & 0xffffff, br & 0xffffff));

    struct Candidate
    {
        Transform transform;
        std::uint32_t tl, tr, bl, br;
    };

    // Each row is where the four source quadrants end up under that transform.
    const Candidate candidates[] = {
        { Transform::Identity, colour_red, colour_green, colour_blue,
          colour_white },
        { Transform::Rotate180, colour_white, colour_blue, colour_green,
          colour_red },
        { Transform::FlipHorizontal, colour_green, colour_red, colour_white,
          colour_blue },
        { Transform::FlipVertical, colour_blue, colour_white, colour_red,
          colour_green },
    };

    for (const Candidate& candidate : candidates) {
        if (near(tl, candidate.tl) && near(tr, candidate.tr) &&
            near(bl, candidate.bl) && near(br, candidate.br)) {
            check("screen transform",
                  candidate.transform == Transform::Identity ? Verdict::Ok
                                                             : Verdict::Info,
                  util::format("content reaches the panel %s",
                               transform_name(candidate.transform)));
            return candidate.transform;
        }
    }

    check("screen transform", Verdict::Wrong,
          "no simple rotation or mirror explains the result; the frame may be "
          "torn, scaled, or not drawn at all");
    return Transform::Unknown;
}

void check_texture_upload_copies(SDL_Renderer* renderer)
{
    section("Renderer conformance");

    int w = 0;
    int h = 0;
    draw_size(renderer, &w, &h);

    Surface surface(renderer, w, h);
    if (!surface.valid()) {
        check("SDL_UpdateTexture copies", Verdict::Failed, SDL_GetError());
        return;
    }

    surface.fill(colour_green);
    surface.upload();

    // Rewritten in place, and deliberately NOT uploaded again. A driver that
    // copied at upload time still shows green; one that kept the pointer shows
    // red, because it reads the caller's memory at draw time.
    //
    // Freeing the buffer instead would be the more obvious test and is
    // undefined behaviour — it took this tool down on the first device run
    // rather than reporting anything.
    surface.fill(colour_red);

    SDL_RenderCopy(renderer, surface.texture(), nullptr, nullptr);
    const Frame frame = present_and_read(renderer);

    if (!frame.image.valid()) {
        check("SDL_UpdateTexture copies", Verdict::Skipped, "no readback path");
        return;
    }

    const std::uint32_t centre = centre_of(frame.image);
    if (near(centre, colour_green)) {
        check("SDL_UpdateTexture copies", Verdict::Ok,
              "the upload took a copy, as SDL2 specifies");
    } else if (near(centre, colour_red)) {
        check("SDL_UpdateTexture copies", Verdict::Wrong,
              "the driver kept the caller's pointer and read it at draw time. "
              "Any texture whose upload buffer has been freed or reused is a "
              "use-after-free — SDL_CreateTextureFromSurface is one before it "
              "returns");
    } else {
        check("SDL_UpdateTexture copies", Verdict::Wrong,
              util::format("screen is %06x, expected green or red",
                           centre & 0xffffff));
    }
}

void check_clear(SDL_Renderer* renderer)
{
    // Background and clear in ONE frame, with a single present at the end.
    //
    // The obvious sequence — paint, present, clear, present, read — measures
    // the wrong buffer here. This driver presents by panning fbdev between two
    // halves of a 640x960 framebuffer, so the second present flips to the
    // buffer holding the frame from two presents ago. The first device run
    // reported WRONG and named a colour left over from the previous check.
    if (!paint_background(renderer, colour_white)) {
        check("SDL_RenderClear", Verdict::Skipped,
              "could not paint a known background to clear over");
        return;
    }

    clear_to(renderer, colour_blue);
    const Frame frame = present_and_read(renderer);

    if (!frame.image.valid()) {
        check("SDL_RenderClear", Verdict::Skipped, "no readback path");
        return;
    }

    const std::uint32_t centre = centre_of(frame.image);
    if (near(centre, colour_blue)) {
        check("SDL_RenderClear", Verdict::Ok, "screen is the requested colour");
    } else if (near(centre, colour_white)) {
        check("SDL_RenderClear", Verdict::Ignored,
              "screen still holds the previous frame; the clear was queued and "
              "never executed");
    } else {
        check("SDL_RenderClear", Verdict::Wrong,
              util::format("screen is %06x, expected %06x", centre & 0xffffff,
                           colour_blue & 0xffffff));
    }
}

void check_full_copy(SDL_Renderer* renderer, Transform transform)
{
    int w = 0;
    int h = 0;
    draw_size(renderer, &w, &h);

    Surface surface(renderer, w, h);
    if (!surface.valid()) {
        check("SDL_RenderCopy full", Verdict::Failed, SDL_GetError());
        return;
    }
    surface.fill(colour_green);
    surface.upload();

    SDL_RenderCopy(renderer, surface.texture(), nullptr, nullptr);
    const Frame frame = present_and_read(renderer);

    if (!frame.image.valid()) {
        check("SDL_RenderCopy full", Verdict::Skipped, "no readback path");
        return;
    }

    // A full-screen solid is transform-invariant, which is the point of doing
    // it before anything that is not.
    (void)transform;
    const std::uint32_t centre = centre_of(frame.image);
    if (near(centre, colour_green)) {
        check("SDL_RenderCopy full", Verdict::Ok, "");
    } else {
        check("SDL_RenderCopy full", Verdict::Wrong,
              util::format("screen is %06x, expected %06x", centre & 0xffffff,
                           colour_green & 0xffffff));
    }
}

// The atlas case, and the one that blocks software-2d-sprites-tiling. Two known
// defects in this driver make it fail in different ways: the staging copy
// ignores the source rect's y, and the pixel format is inferred from
// pitch / srcrect.w so a narrow sub-rect is read as the wrong format entirely.
void check_sub_rect_copy(SDL_Renderer* renderer, Transform transform)
{
    int w = 0;
    int h = 0;
    draw_size(renderer, &w, &h);

    // Red on top, green in the middle, blue at the bottom. Copying the middle
    // band should paint the screen green; if the source y is ignored, red
    // arrives instead.
    Surface surface(renderer, w, h);
    if (!surface.valid()) {
        check("SDL_RenderCopy sub-rect", Verdict::Failed, SDL_GetError());
        return;
    }
    surface.bands(colour_red, colour_green, colour_blue);
    surface.upload();

    // The staging buffer is poisoned first, and this is the whole difference
    // between a real answer and a lucky one.
    //
    // GFX_Copy memcpy's only srt.h rows into a full-screen staging buffer and
    // then tells the blitter to read at row srt.y — so rows past the copy hold
    // whatever the LAST blit left there. On the first device run the preceding
    // check had filled staging with green, the band being asked for was also
    // green, and this reported OK about a driver that had read stale memory.
    //
    // A full-screen magenta copy in the same frame fills staging with a colour
    // that appears nowhere else, making the three outcomes distinguishable:
    // green is correct, red is "source y ignored", magenta is "read past the
    // rows that were copied".
    Surface poison(renderer, w, h);
    if (!poison.valid()) {
        check("SDL_RenderCopy sub-rect", Verdict::Skipped,
              "could not build the staging poison texture");
        return;
    }
    poison.fill(colour_magenta);
    poison.upload();
    SDL_RenderCopy(renderer, poison.texture(), nullptr, nullptr);

    const SDL_Rect src = { 0, h / 3, w, h / 3 };
    SDL_RenderCopy(renderer, surface.texture(), &src, nullptr);
    const Frame frame = present_and_read(renderer);

    if (!frame.image.valid()) {
        check("SDL_RenderCopy sub-rect", Verdict::Skipped, "no readback path");
        return;
    }

    (void)transform; // a full-width band is symmetric under all four
    const std::uint32_t centre = centre_of(frame.image);

    if (near(centre, colour_green)) {
        check("SDL_RenderCopy sub-rect", Verdict::Ok,
              "the requested band reached the screen");
    } else if (near(centre, colour_red)) {
        check("SDL_RenderCopy sub-rect", Verdict::Wrong,
              "the source rect's y was ignored — row 0 was copied instead of "
              "row y (atlas rendering cannot work)");
    } else if (near(centre, colour_magenta)) {
        check("SDL_RenderCopy sub-rect", Verdict::Wrong,
              "the blitter read past the rows that were staged, and returned "
              "the previous blit's leftovers — an atlas would show whatever "
              "was drawn last frame");
    } else {
        check("SDL_RenderCopy sub-rect", Verdict::Wrong,
              util::format("screen is %06x — none of the requested band, the "
                           "top of the texture, or the staging poison",
                           centre & 0xffffff));
    }
}

// The pixel-format inference, exercised properly.
//
// The driver decides the source format with `rgb565 = (pitch / srt.w) == 2`,
// where pitch is the whole texture's and srt.w is the sub-rectangle's. The
// ratio is only the bytes-per-pixel when the rect spans the full width.
//
// Corrected after the first device run, which reported OK and proved nothing.
// It used an ARGB8888 texture, where 2560/80 = 32, so the driver guessed "not
// RGB565" — and was right by accident. The case that bites is an RGB565
// texture with a narrow rect: the ratio is not 2, so a 16-bit surface is handed
// to the blitter as 32-bit and arrives as noise.
void check_narrow_sub_rect(SDL_Renderer* renderer)
{
    int w = 0;
    int h = 0;
    draw_size(renderer, &w, &h);

    SDL_Texture* texture =
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565,
                          SDL_TEXTUREACCESS_STREAMING, w, h);
    if (!texture) {
        check("sub-rect, RGB565", Verdict::Skipped,
              std::string("no RGB565 texture: ") + SDL_GetError());
        return;
    }

    // Kept alive for the texture's lifetime, for the reason Surface exists.
    // 0x07e0 is pure green in RGB565.
    std::vector<std::uint16_t> pixels(static_cast<std::size_t>(w) * h, 0x07e0);
    SDL_UpdateTexture(texture, nullptr, pixels.data(),
                      w * static_cast<int>(sizeof(std::uint16_t)));

    const SDL_Rect src = { 0, 0, w / 8, h / 8 };
    SDL_RenderCopy(renderer, texture, &src, nullptr);
    const Frame frame = present_and_read(renderer);
    SDL_DestroyTexture(texture);

    if (!frame.image.valid()) {
        check("sub-rect, RGB565", Verdict::Skipped, "no readback path");
        return;
    }

    const std::uint32_t centre = centre_of(frame.image);
    if (near(centre, 0xff00ff00, 24)) {
        check("sub-rect, RGB565", Verdict::Ok,
              "a 16-bit texture kept its format through a narrow sub-rect");
    } else {
        check("sub-rect, RGB565", Verdict::Wrong,
              util::format("a %dx%d sub-rect of a solid green RGB565 texture "
                           "arrived as %06x. pitch/rect.w is %d here, and the "
                           "driver reads that as the bytes per pixel",
                           src.w, src.h, centre & 0xffffff,
                           (w * 2) / (w / 8)));
    }
}

// Where a partial destination rectangle actually lands.
//
// Distinct from the source-rect defects above, and only formulable now that the
// panel orientation is settled: the panel is mounted 180 degrees and the fixed
// E_MI_GFX_ROTATE_180 compensates for it, which is why full-screen output is
// upright. But Mini_QueueCopy also mirrors the destination x by hand:
//
//     dst.x = (vid_win->w - (dstrect->x + dstrect->w)) * scale;
//     dst.y = dstrect->y * scale;
//
// For a full-screen destination that reduces to 0 and is invisible. For a
// partial one it mirrors x a second time — cancelling the rotation on that axis
// — while y is mirrored only once, by the rotation. So a sprite should arrive
// with the correct x and a vertically mirrored y.
void check_dest_placement(SDL_Renderer* renderer, Transform transform)
{
    if (!paint_background(renderer, colour_black)) {
        check("partial destination", Verdict::Skipped,
              "could not paint a background to place against");
        return;
    }

    int w = 0;
    int h = 0;
    draw_size(renderer, &w, &h);

    Surface block(renderer, w / 4, h / 4);
    if (!block.valid()) {
        check("partial destination", Verdict::Failed, SDL_GetError());
        return;
    }
    block.fill(colour_white);
    block.upload();

    // Deliberately off-centre in both axes, so a mirror in either one shows.
    const SDL_Rect dst = { w / 8, h / 8, w / 4, h / 4 };
    SDL_RenderCopy(renderer, block.texture(), nullptr, &dst);
    const Frame frame = present_and_read(renderer);

    if (!frame.image.valid()) {
        check("partial destination", Verdict::Skipped, "no readback path");
        return;
    }

    // Bounding box of everything bright in the frame.
    int min_x = frame.image.width;
    int min_y = frame.image.height;
    int max_x = -1;
    int max_y = -1;
    for (int y = 0; y < frame.image.height; ++y) {
        for (int x = 0; x < frame.image.width; ++x) {
            if (near(frame.image.at(x, y), colour_white, 40)) {
                min_x = std::min(min_x, x);
                min_y = std::min(min_y, y);
                max_x = std::max(max_x, x);
                max_y = std::max(max_y, y);
            }
        }
    }

    if (max_x < 0) {
        check("partial destination", Verdict::Ignored,
              "nothing was drawn — a partial destination rectangle produced no "
              "output at all");
        return;
    }

    const int fb_w = frame.image.width;
    const int fb_h = frame.image.height;
    const int box_w = max_x - min_x + 1;
    const int box_h = max_y - min_y + 1;

    // Where the viewer sees it, which is not where the frame holds it whenever
    // the content reaches the panel rotated. Applied from the transform this
    // run measured rather than assumed: on a correct implementation the two are
    // the same, and un-rotating unconditionally made the desktop control report
    // a fault that was entirely this check's.
    int seen_x = min_x;
    int seen_y = min_y;
    if (transform == Transform::Rotate180) {
        seen_x = fb_w - max_x - 1;
        seen_y = fb_h - max_y - 1;
    } else if (transform != Transform::Identity) {
        check("partial destination", Verdict::Skipped,
              util::format("screen transform is '%s'; this check only maps "
                           "identity and 180",
                           transform_name(transform)));
        return;
    }

    field("requested dst", util::format("%d,%d %dx%d", dst.x, dst.y, dst.w,
                                        dst.h));
    field("in the framebuffer", util::format("%d,%d %dx%d", min_x, min_y,
                                             box_w, box_h));
    field("as seen on the panel", util::format("%d,%d %dx%d", seen_x, seen_y,
                                               box_w, box_h));

    const bool x_ok = std::abs(seen_x - dst.x) <= 2;
    const bool y_ok = std::abs(seen_y - dst.y) <= 2;
    const int mirrored_y = fb_h - dst.y - dst.h;

    if (x_ok && y_ok) {
        check("partial destination", Verdict::Ok, "landed where it was asked");
    } else if (x_ok && std::abs(seen_y - mirrored_y) <= 2) {
        check("partial destination", Verdict::Wrong,
              util::format("x is right and y is mirrored — expected y=%d, got "
                           "y=%d. The driver mirrors destination x by hand on "
                           "top of the panel rotation, which cancels on x and "
                           "does not on y",
                           dst.y, seen_y));
    } else {
        check("partial destination", Verdict::Wrong,
              util::format("expected %d,%d and it is at %d,%d", dst.x, dst.y,
                           seen_x, seen_y));
    }
}

// The biggest forward-looking gap: an alpha sprite over a background. The
// driver hardcodes eSrcDfbBldOp = BLD_ONE and never reads the texture's blend
// mode, so a half-transparent texture arrives opaque.
void check_blend_mode(SDL_Renderer* renderer)
{
    if (!paint_background(renderer, colour_red)) {
        check("SDL_SetTextureBlendMode", Verdict::Skipped,
              "could not paint a background to blend over");
        return;
    }

    int w = 0;
    int h = 0;
    draw_size(renderer, &w, &h);

    // Green at 50% alpha over red should read as a blend of the two.
    Surface surface(renderer, w / 2, h / 2);
    if (!surface.valid()) {
        check("SDL_SetTextureBlendMode", Verdict::Failed, SDL_GetError());
        return;
    }
    surface.fill(0x8000ff00);
    surface.upload();

    if (SDL_SetTextureBlendMode(surface.texture(), SDL_BLENDMODE_BLEND) != 0) {
        check("SDL_SetTextureBlendMode", Verdict::Unsupported, SDL_GetError());
        return;
    }

    SDL_BlendMode got = SDL_BLENDMODE_NONE;
    SDL_GetTextureBlendMode(surface.texture(), &got);

    SDL_RenderCopy(renderer, surface.texture(), nullptr, nullptr);
    const Frame frame = present_and_read(renderer);

    if (!frame.image.valid()) {
        check("SDL_SetTextureBlendMode", Verdict::Skipped, "no readback path");
        return;
    }

    const std::uint32_t centre = centre_of(frame.image);
    const int red = static_cast<int>((centre >> 16) & 0xff);
    const int green = static_cast<int>((centre >> 8) & 0xff);

    if (red > 40 && green > 40) {
        check("SDL_SetTextureBlendMode", Verdict::Ok,
              util::format("blended: %06x over red", centre & 0xffffff));
    } else if (green > 40 && red <= 40) {
        check("SDL_SetTextureBlendMode", Verdict::Ignored,
              util::format("sprite is opaque (%06x) — the blend mode was "
                           "accepted (mode=%d) and never applied",
                           centre & 0xffffff, static_cast<int>(got)));
    } else {
        check("SDL_SetTextureBlendMode", Verdict::Wrong,
              util::format("screen is %06x over a red background",
                           centre & 0xffffff));
    }
}

void check_colour_and_alpha_mod(SDL_Renderer* renderer)
{
    int w = 0;
    int h = 0;
    draw_size(renderer, &w, &h);

    Surface surface(renderer, w, h);
    if (!surface.valid()) {
        check("SDL_SetTextureColorMod", Verdict::Failed, SDL_GetError());
        return;
    }
    surface.fill(colour_white);
    surface.upload();

    // White modulated by pure red should arrive red. Anything that leaves it
    // white means the modulation never reached the blitter.
    SDL_SetTextureColorMod(surface.texture(), 0xff, 0x00, 0x00);
    SDL_RenderCopy(renderer, surface.texture(), nullptr, nullptr);
    const Frame frame = present_and_read(renderer);

    if (!frame.image.valid()) {
        check("SDL_SetTextureColorMod", Verdict::Skipped, "no readback path");
        return;
    }

    const std::uint32_t centre = centre_of(frame.image);
    if (near(centre, 0xffff0000, 24)) {
        check("SDL_SetTextureColorMod", Verdict::Ok, "");
    } else if (near(centre, colour_white, 24)) {
        check("SDL_SetTextureColorMod", Verdict::Ignored,
              "texture arrived unmodulated — no tints, fades or damage "
              "flashes on this target");
    } else {
        check("SDL_SetTextureColorMod", Verdict::Wrong,
              util::format("screen is %06x, expected ff0000",
                           centre & 0xffffff));
    }
}

void check_fill_rect(SDL_Renderer* renderer)
{
    if (!paint_background(renderer, colour_black)) {
        check("SDL_RenderFillRect", Verdict::Skipped,
              "could not paint a background to fill over");
        return;
    }

    int w = 0;
    int h = 0;
    output_size(renderer, &w, &h);

    SDL_SetRenderDrawColor(renderer, 0x00, 0xe0, 0x00, 0xff);
    const SDL_Rect rect = { w / 4, h / 4, w / 2, h / 2 };
    if (SDL_RenderFillRect(renderer, &rect) != 0) {
        check("SDL_RenderFillRect", Verdict::Unsupported, SDL_GetError());
        return;
    }

    const Frame frame = present_and_read(renderer);
    if (!frame.image.valid()) {
        check("SDL_RenderFillRect", Verdict::Skipped, "no readback path");
        return;
    }

    const std::uint32_t centre = centre_of(frame.image);
    if (near(centre, colour_green)) {
        check("SDL_RenderFillRect", Verdict::Ok, "");
    } else if (near(centre, colour_black)) {
        check("SDL_RenderFillRect", Verdict::Ignored,
              "returned success and drew nothing (MI_GFX_QuickFill exists and "
              "is not wired up)");
    } else {
        check("SDL_RenderFillRect", Verdict::Wrong,
              util::format("screen is %06x", centre & 0xffffff));
    }
}

// Not a readback check: this one asks whether the advertised limit is the real
// one, which matters because a texture larger than the panel fails to create
// on this device and the failure looks like a missing element rather than a
// scaled one (D23).
void check_texture_limits(SDL_Renderer* renderer)
{
    section("Texture limits");

    SDL_RendererInfo info;
    if (SDL_GetRendererInfo(renderer, &info) != 0) {
        check("SDL_GetRendererInfo", Verdict::Failed, SDL_GetError());
        return;
    }

    field("advertised max", util::format("%dx%d", info.max_texture_width,
                                         info.max_texture_height));

    const auto try_size = [&](int w, int h) -> bool {
        SDL_Texture* texture =
            SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                              SDL_TEXTUREACCESS_STREAMING, w, h);
        if (texture) {
            SDL_DestroyTexture(texture);
            return true;
        }
        return false;
    };

    if (info.max_texture_width > 0) {
        const bool at_limit =
            try_size(info.max_texture_width, info.max_texture_height);
        check("create at the limit", at_limit ? Verdict::Ok : Verdict::Failed,
              at_limit ? "" : SDL_GetError());

        const bool over_limit =
            try_size(info.max_texture_width + 16, info.max_texture_height);
        check("create over the limit",
              over_limit ? Verdict::Wrong : Verdict::Ok,
              over_limit ? "succeeded past the advertised maximum, so the "
                           "limit is not enforced and not trustworthy"
                         : "refused, as advertised");
    }

    // The panel, versus the cap. On a Mini Flip these disagree: the driver
    // detects a 752x560 framebuffer at startup and the render backend's limit
    // stays at its compiled-in 640x480, so no full-screen layer can exist.
    int ow = 0;
    int oh = 0;
    output_size(renderer, &ow, &oh);
    field("output size", util::format("%dx%d", ow, oh));

    int fbw = 0;
    int fbh = 0;
    if (framebuffer_size(&fbw, &fbh)) {
        field("panel (/dev/fb0)", util::format("%dx%d", fbw, fbh));
        if (info.max_texture_width > 0 &&
            (fbw > info.max_texture_width || fbh > info.max_texture_height)) {
            check("panel fits in a texture", Verdict::Wrong,
                  "the panel is larger than the largest texture this renderer "
                  "will create — a full-screen layer is impossible here");
        } else {
            check("panel fits in a texture", Verdict::Ok, "");
        }
    }
}

void check_render_target(SDL_Renderer* renderer)
{
    SDL_RendererInfo info;
    const bool advertised =
        SDL_GetRendererInfo(renderer, &info) == 0 &&
        (info.flags & SDL_RENDERER_TARGETTEXTURE) != 0;

    field("TARGETTEXTURE advertised", advertised ? "yes" : "no");

    SDL_Texture* target =
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                          SDL_TEXTUREACCESS_TARGET, 64, 64);
    if (!target) {
        check("render to texture",
              advertised ? Verdict::Wrong : Verdict::Unsupported,
              advertised ? std::string("advertised, but the target texture "
                                       "could not be created: ") +
                               SDL_GetError()
                         : "not advertised, and not available");
        return;
    }

    // SDL_GetRenderTarget is NOT the check. It returns renderer->target, which
    // the frontend sets before calling the backend, so it reports success
    // whatever the backend does — that is what made the first device run say
    // OK about a driver whose Mini_SetRenderTarget is `return 0`.
    //
    // The real question is whether anything drawn while the target is current
    // ends up IN the target. So: draw green into it, restore the screen, paint
    // the screen black, then copy the target over the top and read back.
    if (SDL_SetRenderTarget(renderer, target) != 0) {
        check("render to texture", Verdict::Unsupported, SDL_GetError());
        SDL_DestroyTexture(target);
        return;
    }

    {
        Surface green(renderer, 64, 64);
        if (green.valid()) {
            green.fill(colour_green);
            green.upload();
            SDL_RenderCopy(renderer, green.texture(), nullptr, nullptr);
        }
    }

    SDL_SetRenderTarget(renderer, nullptr);

    if (!paint_background(renderer, colour_black)) {
        check("render to texture", Verdict::Skipped,
              "could not paint a background to copy the target over");
        SDL_DestroyTexture(target);
        return;
    }
    SDL_RenderCopy(renderer, target, nullptr, nullptr);
    const Frame frame = present_and_read(renderer);
    SDL_DestroyTexture(target);

    if (!frame.image.valid()) {
        check("render to texture", Verdict::Skipped, "no readback path");
        return;
    }

    const std::uint32_t centre = centre_of(frame.image);
    if (near(centre, colour_green)) {
        check("render to texture", Verdict::Ok,
              "what was drawn while the target was current came back out of "
              "it");
    } else if (near(centre, colour_black)) {
        check("render to texture", Verdict::Ignored,
              "the target texture is empty: SDL_SetRenderTarget reported "
              "success, the draw went somewhere else, and TARGETTEXTURE is "
              "advertised anyway");
    } else {
        check("render to texture", Verdict::Wrong,
              util::format("target contents read back as %06x",
                           centre & 0xffffff));
    }
}

void check_audio()
{
    section("Audio");

    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        check("SDL_InitSubSystem(AUDIO)", Verdict::Failed, SDL_GetError());
        return;
    }

    const char* driver = SDL_GetCurrentAudioDriver();
    field("driver", driver ? driver : "(none)");

    SDL_AudioSpec want;
    SDL_zero(want);
    want.freq = 22050;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 2048;

    SDL_AudioSpec got;
    SDL_zero(got);
    SDL_AudioDeviceID device =
        SDL_OpenAudioDevice(nullptr, 0, &want, &got, 0);

    if (device == 0) {
        // D26: this device's audio failure is a single-owner MI_AO contention,
        // and the whole point of printing SDL_GetError() here is that the
        // driver has historically had nothing to say about it.
        const char* error = SDL_GetError();
        check("SDL_OpenAudioDevice", Verdict::Failed,
              error && *error ? error
                              : "failed with no error string (D26) — the MI_AO "
                                "device is most likely held by another process");
    } else {
        check("SDL_OpenAudioDevice", Verdict::Ok,
              util::format("%d Hz, %d ch, %d samples", got.freq, got.channels,
                           got.samples));
        SDL_CloseAudioDevice(device);
    }

    SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

} // namespace diag
