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
