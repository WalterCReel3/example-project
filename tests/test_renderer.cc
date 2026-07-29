// gfx::renderer — driver selection.
//
// This tests the *selection logic*, not rendering: which driver a Driver value
// resolves to, and whether a request that cannot be satisfied fails or
// degrades. That is the part that goes wrong silently — a PreferAccelerated
// request that quietly fell back everywhere, or an Accelerated request that
// quietly did not, both look fine in a log.
//
// The video driver is pinned to "dummy" by the fixture rather than inherited
// from the environment. cmake/Testing.cmake does set SDL_VIDEODRIVER=dummy, so
// relying on it would work under ctest and then fail for anyone running the
// binary directly on a machine with a GPU — the driver would be "opengl" and
// three assertions here would break for reasons that have nothing to do with
// the code. A test whose result depends on how it was invoked is worse than no
// test.
//
// What this cannot test is the accelerated path itself. Nothing headless can:
// there is no GL under the dummy driver. Whether rk3326 actually gets opengles2
// is answered by wreel-probe on hardware — see
// planning/2026-07-25-target-validation/.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <gfx/renderer/context.hpp>
#include <gfx/renderer/layer.hpp>
#include <gfx/system.hpp>

#include <SDL.h>

#include <memory>
#include <string>

namespace
{

// Pins the video driver, then brings the subsystems up. SDL reads
// SDL_VIDEODRIVER during SDL_Init, so the order matters and gfx::System cannot
// be a plain member.
struct VideoFixture {
    VideoFixture()
    {
        SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
        system.reset(new gfx::System());
    }

    std::unique_ptr<gfx::System> system;
};

} // namespace

TEST_CASE("a context reports the driver it actually got")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 320, 240, /*fullscreen=*/false);

    // Never the "(unknown)" fallback, which would mean SDL_GetRendererInfo
    // failed.
    const std::string driver = context.driver_name();
    CHECK(driver != "(unknown)");
    CHECK(driver != "(unnamed)");
    CHECK(!driver.empty());

    CHECK(context.width() == 320);
    CHECK(context.height() == 240);
}

// Under the dummy video driver there is no GL, so the default must resolve to
// software rather than failing. This is the property that keeps a handheld with
// broken vendor blobs bootable.
TEST_CASE("PreferAccelerated falls back to software rather than throwing")
{
    VideoFixture video;

    gfx::renderer::Context context("test", 320, 240, false,
                                   gfx::renderer::Driver::PreferAccelerated);

    CHECK(context.driver_name() == "software");
    CHECK_FALSE(context.accelerated());
}

TEST_CASE("Software forces the software driver")
{
    VideoFixture video;

    gfx::renderer::Context context("test", 320, 240, false,
                                   gfx::renderer::Driver::Software);

    CHECK(context.driver_name() == "software");
    CHECK_FALSE(context.accelerated());
}

// The counterpart to the fallback test, and the reason Driver has three values
// rather than a bool: a tool checking whether a device is accelerated must be
// able to fail instead of being told "software" and having to notice.
TEST_CASE("Accelerated refuses to downgrade when no GPU driver exists")
{
    VideoFixture video;

    CHECK_THROWS_AS(gfx::renderer::Context("test", 320, 240, false,
                                           gfx::renderer::Driver::Accelerated),
                    std::runtime_error);
}

TEST_CASE("clear and present run against the software driver")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 64, 64, false);

    const gfx::renderer::Color black = {0, 0, 0, 255};
    context.clear(black);
    context.present();

    // Nothing to assert about the pixels through this interface; the point is
    // that the calls are wired to a live renderer and do not crash. A real
    // pixel assertion needs the Texture work in software-2d-sprites-tiling.
    CHECK(context.driver_name() == "software");
}

// ---------------------------------------------------------------------------
// Layer read-back
// ---------------------------------------------------------------------------
//
// This is the path that exists because a renderer cannot always be read back.
// The Miyoo Mini's SDL2 returns SDL_Unsupported from SDL_RenderReadPixels, so
// on the one device where a screenshot is the only way to see the output,
// Context::save_screenshot() has nothing to read and the Layer's own copy is
// what gets saved.
//
// It is tested here rather than left to that device because the fallback runs
// nowhere else: on any machine a developer has, the renderer answers and this
// code never executes. A path that only runs where it cannot be observed is
// exactly the kind that rots.

TEST_CASE("a layer without read-back refuses to save rather than writing "
          "undefined pixels")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 64, 64, false);
    gfx::renderer::Layer layer(context, 32, 16);

    CHECK_FALSE(layer.readback());
    CHECK_FALSE(layer.save_bmp("/tmp/wreel-test-should-not-exist.bmp"));
}

TEST_CASE("a layer with read-back saves what was plotted")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 64, 64, false);
    gfx::renderer::Layer layer(context, 32, 16);

    layer.set_readback(true);
    CHECK(layer.readback());

    const std::uint32_t red = gfx::renderer::Layer::pack(255, 0, 0);
    const std::uint32_t blue = gfx::renderer::Layer::pack(0, 0, 255);

    {
        gfx::renderer::LayerLock pixels = layer.lock();
        REQUIRE(pixels.valid());

        // Two solid halves, so a stride error shows up as a diagonal rather
        // than being invisible in a uniform fill.
        for (int y = 0; y < pixels.height(); ++y) {
            std::uint32_t* row = pixels.row(y);
            for (int x = 0; x < pixels.width(); ++x) {
                row[x] = y < pixels.height() / 2 ? red : blue;
            }
        }
    }

    const std::string path = "/tmp/wreel-test-layer.bmp";
    REQUIRE(layer.save_bmp(path));

    SDL_Surface* loaded = SDL_LoadBMP(path.c_str());
    REQUIRE(loaded != nullptr);
    CHECK(loaded->w == 32);
    CHECK(loaded->h == 16);

    SDL_Surface* argb =
        SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_ARGB8888, 0);
    SDL_FreeSurface(loaded);
    REQUIRE(argb != nullptr);

    const std::uint32_t* read = static_cast<const std::uint32_t*>(argb->pixels);
    const int stride = argb->pitch / static_cast<int>(sizeof(std::uint32_t));

    // Alpha is not preserved by a 24-bit BMP, so compare the colour channels.
    const std::uint32_t rgb = 0x00ffffffu;
    CHECK((read[0] & rgb) == (red & rgb));
    CHECK((read[static_cast<std::ptrdiff_t>(15) * stride + 31] & rgb) ==
          (blue & rgb));

    SDL_FreeSurface(argb);
}

TEST_CASE("turning read-back off releases the buffer and disables saving")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 64, 64, false);
    gfx::renderer::Layer layer(context, 32, 16);

    layer.set_readback(true);
    layer.set_readback(false);

    CHECK_FALSE(layer.readback());
    CHECK_FALSE(layer.save_bmp("/tmp/wreel-test-should-not-exist.bmp"));
}
