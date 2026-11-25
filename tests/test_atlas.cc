// gfx::Atlas — named regions of one texture.
//
// Two halves. Name resolution and the frame table are arithmetic and would run
// anywhere; drawing is asserted against actual pixels, which needs a live
// renderer, so the whole file runs under SDL_VIDEODRIVER=dummy rather than
// splitting over two executables.
//
// The pixel cases are the ones tests/test_renderer.cc left a note for: it could
// check that clear() and present() were wired to a renderer but not what landed
// where, because there was no way to blit part of a texture. That is what
// Texture and Context::draw(src, dst) added, and asserting on it here is the
// point — a source rectangle off by one cell, or a trim offset applied in the
// wrong direction, both draw something perfectly plausible.
//
// Contexts here ask for Driver::Software explicitly. These cases are about
// atlas, animation and tilemap logic, not about driver selection — that is
// test_renderer's job — and leaving the choice open is not harmless: the
// miyoomini build compiles SDL's SSD202D render backend in, so off-device
// (under qemu) a PreferAccelerated request selects `mini`, finds no MI_GFX, and
// segfaults. Asking for the driver these cases actually want makes them
// deterministic on every target.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <gfx/atlas.hpp>
#include <gfx/system.hpp>
#include <loaders/image.hpp>
#include <loaders/sparrow.hpp>

#include <SDL.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{

// SDL reads SDL_VIDEODRIVER during SDL_Init, so the order matters and
// gfx::System cannot be a plain member. Same shape as tests/test_renderer.cc.
struct VideoFixture {
    VideoFixture()
    {
        SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
        system.reset(new gfx::System());
    }

    std::unique_ptr<gfx::System> system;
};

const std::uint32_t red = 0xffff0000u;
const std::uint32_t green = 0xff00ff00u;
const std::uint32_t blue = 0xff0000ffu;
const std::uint32_t white = 0xffffffffu;
const std::uint32_t black = 0xff000000u;

// A 4x1 sheet of four 1x1 cells, each a different colour, so "which cell got
// blitted" is answerable from one pixel.
SDL_Surface* make_sheet()
{
    SDL_Surface* sheet =
        SDL_CreateRGBSurfaceWithFormat(0, 4, 1, 32, SDL_PIXELFORMAT_ARGB8888);
    REQUIRE(sheet != nullptr);

    std::uint32_t* pixels = static_cast<std::uint32_t*>(sheet->pixels);
    pixels[0] = red;
    pixels[1] = green;
    pixels[2] = blue;
    pixels[3] = white;

    return sheet;
}

gfx::Atlas::Frames sheet_frames()
{
    gfx::Atlas::Frames frames;
    frames.push_back(gfx::AtlasFrame{"red", {0, 0, 1, 1}, 0, 0, 0, 0});
    frames.push_back(gfx::AtlasFrame{"green", {1, 0, 1, 1}, 0, 0, 0, 0});
    frames.push_back(gfx::AtlasFrame{"blue", {2, 0, 1, 1}, 0, 0, 0, 0});
    frames.push_back(gfx::AtlasFrame{"white", {3, 0, 1, 1}, 0, 0, 0, 0});
    return frames;
}

gfx::Atlas make_atlas(gfx::renderer::Context& context,
                      gfx::Atlas::Frames frames)
{
    SDL_Surface* sheet = make_sheet();
    gfx::renderer::Texture texture(context, sheet);
    SDL_FreeSurface(sheet);

    // Nothing here blends, and leaving it on would make a failed assertion
    // ambiguous between "wrong cell" and "blended with the clear colour".
    texture.set_blend(false);

    return gfx::Atlas(std::move(texture), std::move(frames));
}

// The rendered frame, read back as ARGB8888 so a pixel is one comparison.
class Screenshot
{
public:
    explicit Screenshot(const gfx::renderer::Context& context)
        : path_("/tmp/wreel-test-atlas.bmp")
        , surface_(nullptr)
    {
        // Before present(): the back buffer's contents are not guaranteed to
        // survive the swap.
        REQUIRE(context.save_screenshot(path_));

        SDL_Surface* loaded = SDL_LoadBMP(path_.c_str());
        REQUIRE(loaded != nullptr);
        surface_ =
            SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_ARGB8888, 0);
        SDL_FreeSurface(loaded);
        REQUIRE(surface_ != nullptr);
    }

    ~Screenshot()
    {
        if (surface_ != nullptr) {
            SDL_FreeSurface(surface_);
        }
        std::remove(path_.c_str());
    }

    Screenshot(const Screenshot&) = delete;
    Screenshot& operator=(const Screenshot&) = delete;

    // Colour channels only: a 24-bit BMP does not preserve alpha, which is the
    // same allowance tests/test_renderer.cc makes.
    std::uint32_t at(int x, int y) const
    {
        const int stride =
            surface_->pitch / static_cast<int>(sizeof(std::uint32_t));
        const std::uint32_t* pixels =
            static_cast<const std::uint32_t*>(surface_->pixels);
        return pixels[static_cast<std::ptrdiff_t>(y) * stride + x] |
               0xff000000u;
    }

private:
    std::string path_;
    SDL_Surface* surface_;
};

} // namespace

//============================================================================
// Name resolution
//============================================================================

TEST_CASE("find resolves a name to its index, and npos for one it does not "
          "have")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 8, 8, /*fullscreen=*/false,
                                   gfx::renderer::Driver::Software);
    const gfx::Atlas atlas = make_atlas(context, sheet_frames());

    CHECK(atlas.size() == 4);
    CHECK_FALSE(atlas.empty());

    CHECK(atlas.find("red") == 0);
    CHECK(atlas.find("blue") == 2);
    CHECK(atlas[atlas.find("white")].source.x == 3);

    CHECK(atlas.find("mauve") == gfx::Atlas::npos);

    // Names are compared whole, not by prefix.
    CHECK(atlas.find("re") == gfx::Atlas::npos);
    CHECK(atlas.find("") == gfx::Atlas::npos);
}

TEST_CASE("a duplicated name resolves to the first in document order")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 8, 8, /*fullscreen=*/false,
                                   gfx::renderer::Driver::Software);

    gfx::Atlas::Frames frames = sheet_frames();
    frames[2].id = "red";
    const gfx::Atlas atlas = make_atlas(context, std::move(frames));

    CHECK(atlas.find("red") == 0);
}

//============================================================================
// contains_frames
//============================================================================

TEST_CASE("contains_frames accepts a frame table that fits its sheet")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 8, 8, /*fullscreen=*/false,
                                   gfx::renderer::Driver::Software);
    const gfx::Atlas atlas = make_atlas(context, sheet_frames());

    CHECK(atlas.texture().width() == 4);
    CHECK(atlas.contains_frames());
}

TEST_CASE("contains_frames rejects a frame running off the sheet")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 8, 8, /*fullscreen=*/false,
                                   gfx::renderer::Driver::Software);

    // The usual cause is an atlas paired with the wrong image, which SDL does
    // not report: the blit clamps or reads neighbouring cells.
    gfx::Atlas::Frames frames = sheet_frames();
    frames[3].source.w = 2;
    const gfx::Atlas atlas = make_atlas(context, std::move(frames));

    CHECK_FALSE(atlas.contains_frames());
}

//============================================================================
// Frame sizing
//============================================================================

TEST_CASE("an untrimmed frame reports its region size, a trimmed one its frame "
          "size")
{
    const gfx::AtlasFrame plain{"plain", {0, 0, 20, 30}, 0, 0, 0, 0};
    CHECK_FALSE(plain.trimmed());
    CHECK(plain.width() == 20);
    CHECK(plain.height() == 30);

    // Two frames of one animation trimmed differently must still occupy the
    // same box, which is what keeps the sprite from jittering.
    const gfx::AtlasFrame trimmed{"trimmed", {0, 0, 20, 30}, 2, 4, 24, 34};
    CHECK(trimmed.trimmed());
    CHECK(trimmed.width() == 24);
    CHECK(trimmed.height() == 34);
}

//============================================================================
// Drawing
//============================================================================

TEST_CASE("draw blits the named cell, not a neighbouring one")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 8, 8, /*fullscreen=*/false,
                                   gfx::renderer::Driver::Software);
    const gfx::Atlas atlas = make_atlas(context, sheet_frames());

    context.clear({0, 0, 0, 255});
    atlas.draw(context, atlas.find("blue"), 3, 5);

    const Screenshot frame(context);

    CHECK(frame.at(3, 5) == blue);

    // The cells either side of it in the sheet, so an off-by-one source
    // rectangle fails rather than drawing a plausible colour.
    CHECK(frame.at(3, 5) != green);
    CHECK(frame.at(3, 5) != white);

    // And nothing anywhere else.
    CHECK(frame.at(0, 0) == black);
    CHECK(frame.at(4, 5) == black);
    CHECK(frame.at(3, 6) == black);
}

TEST_CASE("draw places an untrimmed frame at exactly the given point")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 8, 8, /*fullscreen=*/false,
                                   gfx::renderer::Driver::Software);
    const gfx::Atlas atlas = make_atlas(context, sheet_frames());

    context.clear({0, 0, 0, 255});
    atlas.draw(context, atlas.find("red"), 0, 0);
    atlas.draw(context, atlas.find("green"), 7, 7);

    const Screenshot frame(context);

    CHECK(frame.at(0, 0) == red);
    CHECK(frame.at(7, 7) == green);
}

TEST_CASE("scale multiplies the frame and its trim offset together")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 32, 32, /*fullscreen=*/false,
                                   gfx::renderer::Driver::Software);

    // A 1x1 cell sitting 2 across and 1 down inside its untrimmed box.
    gfx::Atlas::Frames frames = sheet_frames();
    frames[2].trim_x = 2;
    frames[2].trim_y = 1;
    frames[2].frame_width = 4;
    frames[2].frame_height = 4;
    const gfx::Atlas atlas = make_atlas(context, std::move(frames));

    context.clear({0, 0, 0, 255});
    atlas.draw(context, atlas.find("blue"), 4, 4, /*scale=*/3);

    const Screenshot frame(context);

    // Origin 4 plus a trim of 2 scaled by 3 — the offset scales with the art,
    // which is the whole reason scale belongs in here rather than at the call
    // site. Unscaled trim would put this at (6,5).
    CHECK(frame.at(4 + 6, 4 + 3) == blue);
    CHECK(frame.at(6, 5) == black);

    // And it covers 3x3 rather than one pixel.
    CHECK(frame.at(4 + 6 + 2, 4 + 3 + 2) == blue);
    CHECK(frame.at(4 + 6 + 3, 4 + 3) == black);
}

TEST_CASE("a scale below one draws nothing rather than inverting the rect")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 16, 16, /*fullscreen=*/false,
                                   gfx::renderer::Driver::Software);
    const gfx::Atlas atlas = make_atlas(context, sheet_frames());

    context.clear({0, 0, 0, 255});
    atlas.draw(context, atlas.find("red"), 4, 4, /*scale=*/0);
    atlas.draw(context, atlas.find("green"), 8, 8, /*scale=*/-2);

    const Screenshot frame(context);
    CHECK(frame.at(4, 4) == black);
    CHECK(frame.at(8, 8) == black);
}

//============================================================================
// The real atlas
//
// Everything above uses a synthetic 4x1 sheet, which tests the code against
// itself. These pair data/foxy.xml with the data/foxy.png it names — generated
// together by tools/pack_atlas.py — and are what would catch the two halves
// disagreeing.
//============================================================================

TEST_CASE("data/foxy.xml and data/foxy.png describe the same sheet")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 64, 64, /*fullscreen=*/false,
                                   gfx::renderer::Driver::Software);

    const loaders::SparrowAtlas parsed = loaders::load_sparrow("data/foxy.xml");

    SDL_Surface* image = loaders::load_image("data/" + parsed.image_path);
    REQUIRE(image != nullptr);
    gfx::renderer::Texture sheet(context, image);
    SDL_FreeSurface(image);

    const gfx::Atlas atlas(std::move(sheet), parsed.frames);

    CHECK(atlas.size() == 36);

    // The assertion this case exists for: every frame the document declares
    // lies inside the image it names. A packer that laid out to one size and
    // wrote another would pass every parser test and fail here.
    CHECK(atlas.contains_frames());

    CHECK(atlas.find("run.000") != gfx::Atlas::npos);
    CHECK(atlas.find("idle.003") != gfx::Atlas::npos);
    CHECK(atlas.find("run.006") == gfx::Atlas::npos);
}

TEST_CASE("a trimmed frame from the real atlas draws inside its frame box")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 64, 64, /*fullscreen=*/false,
                                   gfx::renderer::Driver::Software);

    const loaders::SparrowAtlas parsed = loaders::load_sparrow("data/foxy.xml");
    SDL_Surface* image = loaders::load_image("data/" + parsed.image_path);
    REQUIRE(image != nullptr);
    gfx::renderer::Texture sheet(context, image);
    SDL_FreeSurface(image);

    const gfx::Atlas atlas(std::move(sheet), parsed.frames);
    const gfx::Atlas::Index idle = atlas.find("idle.000");
    REQUIRE(idle != gfx::Atlas::npos);

    const gfx::AtlasFrame& frame = atlas[idle];
    REQUIRE(frame.trimmed());

    context.clear({0, 0, 0, 255});
    atlas.draw(context, idle, 10, 10);

    const Screenshot shot(context);

    // The bounding box of everything that got drawn, rather than a check per
    // pixel: one failure that reports where the sprite actually landed is worth
    // more than four thousand that report that it did not land here.
    int drawn = 0;
    int min_x = 64;
    int min_y = 64;
    int max_x = -1;
    int max_y = -1;
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
            if (shot.at(x, y) != black) {
                ++drawn;
                min_x = std::min(min_x, x);
                min_y = std::min(min_y, y);
                max_x = std::max(max_x, x);
                max_y = std::max(max_y, y);
            }
        }
    }

    // Something was drawn, and all of it fell within the 33x32 box the frame
    // claims. Both halves matter: a dropped trim offset still draws, and a
    // negated one draws above and left of where it was asked to.
    CHECK(drawn > 0);
    CHECK(min_x >= 10);
    CHECK(min_y >= 10);
    CHECK(max_x < 10 + frame.width());
    CHECK(max_y < 10 + frame.height());

    // And it sits where the trim says, not flush against the corner — which is
    // what a silently dropped offset would produce.
    CHECK(min_x == 10 + frame.trim_x);
    CHECK(min_y == 10 + frame.trim_y);
}

TEST_CASE("draw shifts a trimmed frame by its trim offset")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 8, 8, /*fullscreen=*/false,
                                   gfx::renderer::Driver::Software);

    // The cropped image sits 2 across and 1 down inside its untrimmed frame.
    gfx::Atlas::Frames frames = sheet_frames();
    frames[2].trim_x = 2;
    frames[2].trim_y = 1;
    frames[2].frame_width = 4;
    frames[2].frame_height = 4;
    const gfx::Atlas atlas = make_atlas(context, std::move(frames));

    context.clear({0, 0, 0, 255});

    // (1,1) is where the untrimmed frame's top-left goes, so the pixel lands at
    // (3,2).
    atlas.draw(context, atlas.find("blue"), 1, 1);

    const Screenshot frame(context);

    CHECK(frame.at(3, 2) == blue);

    // Where it would have landed if the offset were dropped, and where it would
    // have landed if the sign were wrong. Both are the failures this case
    // exists for.
    CHECK(frame.at(1, 1) == black);
    CHECK(frame.at(0, 0) == black);
}

//============================================================================
// Mirroring
//
// The synthetic sheet here is two rows of six distinct colours, and the frame
// under test is three columns from the middle of it. Both choices are what the
// assertions need: a one-pixel cell is its own mirror image, and a frame at the
// sheet's left edge cannot tell a reflected source rectangle from an unmoved
// one when the sheet is as wide as the frame.
//============================================================================

namespace
{

const std::uint32_t yellow = 0xffffff00u;
const std::uint32_t cyan = 0xff00ffffu;

const std::uint32_t strip[6] = {red, green, blue, white, yellow, cyan};

// Row 0 is `strip`; row 1 is `strip` backwards, so a reversal applied to one
// row and not the other is visible.
SDL_Surface* make_strip(Uint32 format)
{
    SDL_Surface* argb =
        SDL_CreateRGBSurfaceWithFormat(0, 6, 2, 32, SDL_PIXELFORMAT_ARGB8888);
    REQUIRE(argb != nullptr);

    for (int x = 0; x < 6; ++x) {
        std::uint32_t* top = static_cast<std::uint32_t*>(argb->pixels);
        std::uint32_t* bottom = reinterpret_cast<std::uint32_t*>(
            static_cast<Uint8*>(argb->pixels) + argb->pitch);
        top[x] = strip[x];
        bottom[x] = strip[5 - x];
    }

    if (format == SDL_PIXELFORMAT_ARGB8888) {
        return argb;
    }

    SDL_Surface* converted = SDL_ConvertSurfaceFormat(argb, format, 0);
    SDL_FreeSurface(argb);
    REQUIRE(converted != nullptr);
    return converted;
}

// Columns 1..3 of the strip — green, blue, white on the top row — sitting one
// pixel in from the left of an 8-wide untrimmed box, so the margins are 1 on
// the left and 4 on the right.
gfx::Atlas::Frames strip_frames()
{
    gfx::Atlas::Frames frames;
    frames.push_back(gfx::AtlasFrame{"plain", {1, 0, 3, 2}, 0, 0, 0, 0});
    frames.push_back(gfx::AtlasFrame{"trimmed", {1, 0, 3, 2}, 1, 0, 8, 2});
    return frames;
}

gfx::Atlas make_mirrorable(gfx::renderer::Context& context,
                           Uint32 format = SDL_PIXELFORMAT_ARGB8888)
{
    SDL_Surface* sheet = make_strip(format);
    gfx::Atlas atlas(context, sheet, strip_frames());
    SDL_FreeSurface(sheet);
    return atlas;
}

} // namespace

TEST_CASE("only an atlas built from a surface is mirrorable")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 8, 8, /*fullscreen=*/false,
                                   gfx::renderer::Driver::Software);

    CHECK_FALSE(make_atlas(context, sheet_frames()).mirrorable());

    const gfx::Atlas atlas = make_mirrorable(context);
    CHECK(atlas.mirrorable());
    CHECK(atlas.texture().width() == 6);
    CHECK(atlas.texture().height() == 2);
    CHECK(atlas.contains_frames());

    CHECK_THROWS_AS(gfx::Atlas(context, nullptr, strip_frames()),
                    std::runtime_error);
}

TEST_CASE("a mirrorable atlas draws unmirrored exactly as a plain one does")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 16, 8, /*fullscreen=*/false,
                                   gfx::renderer::Driver::Software);
    const gfx::Atlas atlas = make_mirrorable(context);

    context.clear({0, 0, 0, 255});
    atlas.draw(context, atlas.find("plain"), 2, 3);
    atlas.draw(context, atlas.find("trimmed"), 8, 5, /*scale=*/1,
               gfx::Mirror::None);

    const Screenshot frame(context);

    CHECK(frame.at(1, 3) == black);
    CHECK(frame.at(2, 3) == green);
    CHECK(frame.at(3, 3) == blue);
    CHECK(frame.at(4, 3) == white);
    CHECK(frame.at(5, 3) == black);

    CHECK(frame.at(2, 4) == yellow);
    CHECK(frame.at(3, 4) == white);
    CHECK(frame.at(4, 4) == blue);

    // Trimmed: one pixel in from the box's left edge at 8.
    CHECK(frame.at(8, 5) == black);
    CHECK(frame.at(9, 5) == green);
    CHECK(frame.at(11, 5) == white);
    CHECK(frame.at(12, 5) == black);
}

TEST_CASE("a mirrored cell lands reversed, in the place it was asked for")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 8, 8, /*fullscreen=*/false,
                                   gfx::renderer::Driver::Software);
    const gfx::Atlas atlas = make_mirrorable(context);

    context.clear({0, 0, 0, 255});
    atlas.draw(context, atlas.find("plain"), 2, 3, /*scale=*/1,
               gfx::Mirror::Horizontal);

    const Screenshot frame(context);

    // The same three columns, right to left. The neighbours either side in the
    // sheet are red and yellow on this row, which is what a source rectangle
    // reflected about the wrong width, or not reflected, would bring in.
    CHECK(frame.at(2, 3) == white);
    CHECK(frame.at(3, 3) == blue);
    CHECK(frame.at(4, 3) == green);

    // Rows are reversed each in place, not exchanged.
    CHECK(frame.at(2, 4) == blue);
    CHECK(frame.at(3, 4) == white);
    CHECK(frame.at(4, 4) == yellow);

    // An untrimmed frame does not move.
    CHECK(frame.at(1, 3) == black);
    CHECK(frame.at(5, 3) == black);
    CHECK(frame.at(2, 2) == black);
    CHECK(frame.at(2, 5) == black);
}

TEST_CASE("a mirrored trimmed frame swaps its margins inside the untrimmed box")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 16, 8, /*fullscreen=*/false,
                                   gfx::renderer::Driver::Software);
    const gfx::Atlas atlas = make_mirrorable(context);

    context.clear({0, 0, 0, 255});
    atlas.draw(context, atlas.find("trimmed"), 4, 1, /*scale=*/1,
               gfx::Mirror::Horizontal);

    const Screenshot frame(context);

    // The box is columns 4..11. Unmirrored the art is at 5..7, a margin of 1
    // on the left and 4 on the right; mirrored it is at 8..10, with 4 on the
    // left and 1 on the right.
    CHECK(frame.at(8, 1) == white);
    CHECK(frame.at(9, 1) == blue);
    CHECK(frame.at(10, 1) == green);
    CHECK(frame.at(11, 1) == black);

    // Where reversing about the trimmed region would have left it, and where a
    // negated offset would have.
    CHECK(frame.at(5, 1) == black);
    CHECK(frame.at(6, 1) == black);
    CHECK(frame.at(7, 1) == black);
    CHECK(frame.at(3, 1) == black);
}

TEST_CASE("the mirrored margin scales with the art")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 32, 8, /*fullscreen=*/false,
                                   gfx::renderer::Driver::Software);
    const gfx::Atlas atlas = make_mirrorable(context);

    context.clear({0, 0, 0, 255});
    atlas.draw(context, atlas.find("trimmed"), 3, 0, /*scale=*/3,
               gfx::Mirror::Horizontal);

    const Screenshot frame(context);

    // A left margin of 4 at 3x: the art starts at 3 + 12 and runs nine pixels.
    CHECK(frame.at(14, 0) == black);
    CHECK(frame.at(15, 0) == white);
    CHECK(frame.at(17, 0) == white);
    CHECK(frame.at(18, 0) == blue);
    CHECK(frame.at(21, 0) == green);
    CHECK(frame.at(23, 0) == green);
    CHECK(frame.at(24, 0) == black);

    // The box is 24 wide from 3, so the right margin of 1 is three pixels and
    // nothing is drawn past column 26.
    CHECK(frame.at(27, 0) == black);
}

TEST_CASE("a sheet that is not 32 bits per pixel mirrors pixel by pixel")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 8, 8, /*fullscreen=*/false,
                                   gfx::renderer::Driver::Software);

    // Three bytes a pixel: reversing the row bytewise would exchange red and
    // blue within each pixel as well as reversing the columns.
    const gfx::Atlas atlas = make_mirrorable(context, SDL_PIXELFORMAT_RGB24);
    REQUIRE(atlas.mirrorable());

    context.clear({0, 0, 0, 255});
    atlas.draw(context, atlas.find("plain"), 0, 0, /*scale=*/1,
               gfx::Mirror::Horizontal);

    const Screenshot frame(context);

    CHECK(frame.at(0, 0) == white);
    CHECK(frame.at(1, 0) == blue);
    CHECK(frame.at(2, 0) == green);
    CHECK(frame.at(0, 1) == blue);
    CHECK(frame.at(2, 1) == yellow);
}

TEST_CASE("an atlas without a mirrored sheet refuses a mirror request")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 8, 8, /*fullscreen=*/false,
                                   gfx::renderer::Driver::Software);

    gfx::Atlas::Frames frames = sheet_frames();
    frames.push_back(gfx::AtlasFrame{"pair", {1, 0, 2, 1}, 1, 0, 6, 1});
    const gfx::Atlas atlas = make_atlas(context, std::move(frames));
    REQUIRE_FALSE(atlas.mirrorable());

    context.clear({0, 0, 0, 255});
    CHECK_THROWS_AS(atlas.draw(context, atlas.find("pair"), 0, 0, /*scale=*/1,
                               gfx::Mirror::Horizontal),
                    std::logic_error);

    // And it drew nothing on the way out, mirrored or otherwise.
    atlas.draw(context, atlas.find("pair"), 0, 2);

    const Screenshot frame(context);

    CHECK(frame.at(1, 0) == black);
    CHECK(frame.at(2, 0) == black);
    CHECK(frame.at(3, 0) == black);
    CHECK(frame.at(1, 2) == green);
    CHECK(frame.at(2, 2) == blue);
}

TEST_CASE("a real trimmed frame mirrors about its logical box, pixel for pixel")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 64, 128, /*fullscreen=*/false,
                                   gfx::renderer::Driver::Software);

    const loaders::SparrowAtlas parsed =
        loaders::load_sparrow("data/critters.xml");
    SDL_Surface* image = loaders::load_image("data/" + parsed.image_path);
    REQUIRE(image != nullptr);
    const gfx::Atlas atlas(context, image, parsed.frames);
    SDL_FreeSurface(image);

    // The frame with the most lopsided margins in the sheet: 1 on the left
    // and 7 on the right. A frame with equal margins cannot tell the two
    // centre lines apart.
    const gfx::Atlas::Index walk = atlas.find("opossum-walk.000");
    REQUIRE(walk != gfx::Atlas::npos);
    const gfx::AtlasFrame& cell = atlas[walk];
    REQUIRE(cell.trimmed());
    REQUIRE(cell.trim_x != cell.width() - cell.trim_x - cell.source.w);

    const int left = 10;
    const int plain_top = 10;
    const int mirrored_top = 70;

    context.clear({0, 0, 0, 255});
    atlas.draw(context, walk, left, plain_top);
    atlas.draw(context, walk, left, mirrored_top, /*scale=*/1,
               gfx::Mirror::Horizontal);

    const Screenshot shot(context);

    int drawn = 0;
    int mismatched = 0;
    int min_x = 64;
    for (int row = 0; row < cell.height(); ++row) {
        for (int column = 0; column < cell.width(); ++column) {
            const std::uint32_t plain = shot.at(left + column, plain_top + row);
            const std::uint32_t mirrored =
                shot.at(left + cell.width() - 1 - column, mirrored_top + row);

            if (plain != black) {
                ++drawn;
            }
            if (plain != mirrored) {
                ++mismatched;
            }
            if (shot.at(left + column, mirrored_top + row) != black) {
                min_x = std::min(min_x, left + column);
            }
        }
    }

    CHECK(drawn > 0);
    CHECK(mismatched == 0);
    CHECK(min_x == left + cell.width() - cell.trim_x - cell.source.w);
}
