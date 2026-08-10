// loaders::load_tmx and gfx::TileMap.
//
// The parser half runs headless, like load_sparrow's: a TMX is plain data and
// resolving it needs no renderer. The culling half needs a Context, because
// what it checks is how many tiles a draw actually touched — the map is larger
// than the target on purpose, and "draws the whole map" and "draws the visible
// rectangle" look identical on screen.
//
// data/sunnyland.tmx is the fixture: 64x36 tiles of 16x16, an external
// sunnyland.tsx, CSV layer data and an object layer, which is the shape Tiled
// writes by default.
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

#include <gfx/system.hpp>
#include <gfx/tilemap.hpp>
#include <loaders/image.hpp>
#include <loaders/tmx.hpp>
#include <posix/errors.hpp>

#include <SDL.h>

#include <cstdio>
#include <memory>
#include <string>
#include <utility>

namespace
{

struct VideoFixture {
    VideoFixture()
    {
        SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
        system.reset(new gfx::System());
    }

    std::unique_ptr<gfx::System> system;
};

class ScratchFile
{
public:
    ScratchFile(const std::string& name, const std::string& text)
        : path_(name)
    {
        std::FILE* out = std::fopen(path_.c_str(), "w");
        REQUIRE(out != nullptr);
        std::fwrite(text.data(), 1, text.size(), out);
        std::fclose(out);
    }

    ~ScratchFile() { std::remove(path_.c_str()); }

    ScratchFile(const ScratchFile&) = delete;
    ScratchFile& operator=(const ScratchFile&) = delete;

    const std::string& path() const { return path_; }

private:
    std::string path_;
};

// A one-tileset, one-layer map with whatever attributes the case is about.
std::string map_of(const std::string& map_attributes,
                   const std::string& layer_body)
{
    return "<map version=\"1.10\" orientation=\"orthogonal\" width=\"2\" "
           "height=\"2\" tilewidth=\"16\" tileheight=\"16\" " +
           map_attributes +
           ">\n"
           " <tileset firstgid=\"1\" name=\"t\" tilewidth=\"16\" "
           "tileheight=\"16\" tilecount=\"4\" columns=\"2\">\n"
           "  <image source=\"t.png\" width=\"32\" height=\"32\"/>\n"
           " </tileset>\n"
           " <layer name=\"l\" width=\"2\" height=\"2\">\n" +
           layer_body +
           " </layer>\n"
           "</map>\n";
}

const char* csv_2x2 = "  <data encoding=\"csv\">1,2,\n3,0</data>\n";

// A tileset whose texture is a real image, so fits_texture() means something.
gfx::TileSet load_tileset(gfx::renderer::Context& context,
                          const loaders::TmxMap& map)
{
    SDL_Surface* image = loaders::load_image("data/" + map.tileset.image);
    REQUIRE(image != nullptr);
    gfx::renderer::Texture texture(context, image);
    SDL_FreeSurface(image);

    return gfx::TileSet(std::move(texture), map.tileset.tile_width,
                        map.tileset.tile_height, map.tileset.columns,
                        map.tileset.tile_count, map.tileset.margin,
                        map.tileset.spacing);
}

} // namespace

//============================================================================
// The real fixture
//============================================================================

TEST_CASE("data/sunnyland.tmx parses, external tileset and all")
{
    const loaders::TmxMap map = loaders::load_tmx("data/sunnyland.tmx");

    CHECK(map.width == 64);
    CHECK(map.height == 36);
    CHECK(map.tile_width == 16);
    CHECK(map.tile_height == 16);

    // Read out of sunnyland.tsx, which the map only referenced by name.
    CHECK(map.tileset.name == "sunnyland");
    CHECK(map.tileset.image == "tileset.png");
    CHECK(map.tileset.columns == 25);
    CHECK(map.tileset.tile_count == 575);
    CHECK(map.tileset.image_width == 400);
    CHECK(map.tileset.image_height == 368);

    REQUIRE(map.layers.size() == 3);
    CHECK(map.layers[0].name == "backdrop");
    CHECK(map.layers[1].name == "ground");
    CHECK(map.layers[2].name == "decoration");

    for (const gfx::TileLayer& layer : map.layers) {
        CHECK(layer.width == 64);
        CHECK(layer.height == 36);
        CHECK(layer.tiles.size() == 64u * 36u);
        CHECK(layer.visible);
    }

    // The backdrop is filled edge to edge — that is what makes the fixture the
    // worst case for fill rate rather than a flattering one.
    const gfx::TileLayer& backdrop = map.layers[0];
    for (int tile : backdrop.tiles) {
        REQUIRE(tile != gfx::empty_tile);
    }

    // The decoration layer is mostly empty, which is the other extreme.
    const gfx::TileLayer& decoration = map.layers[2];
    int decorated = 0;
    for (int tile : decoration.tiles) {
        if (tile != gfx::empty_tile) {
            ++decorated;
        }
    }
    CHECK(decorated > 0);
    CHECK(decorated < 64 * 36 / 4);
}

TEST_CASE("the object layer survives the trip")
{
    const loaders::TmxMap map = loaders::load_tmx("data/sunnyland.tmx");

    REQUIRE(map.object_layers.size() == 1);
    CHECK(map.object_layers[0].name == "objects");
    REQUIRE(map.object_layers[0].objects.size() == 2);

    const gfx::MapObject& spawn = map.object_layers[0].objects[0];
    CHECK(spawn.name == "spawn");
    CHECK(spawn.type == "point");
    CHECK(spawn.x == doctest::Approx(64.0));
}

//============================================================================
// Global ids
//============================================================================

TEST_CASE("gids become 0-based tileset indexes and 0 becomes empty_tile")
{
    const ScratchFile file("test_tmx.scratch.tmx", map_of("", csv_2x2));
    const loaders::TmxMap map = loaders::load_tmx(file.path());

    REQUIRE(map.layers.size() == 1);
    const gfx::TileLayer& layer = map.layers[0];

    // firstgid is 1, so gid 1 is tile 0. Nothing downstream should ever see a
    // gid again.
    CHECK(layer.tiles[0] == 0);
    CHECK(layer.tiles[1] == 1);
    CHECK(layer.tiles[2] == 2);
    CHECK(layer.tiles[3] == gfx::empty_tile);

    CHECK(layer(0, 0) == 0);
    CHECK(layer(1, 1) == gfx::empty_tile);

    // at() clamps to empty rather than reading out of bounds, so a viewport
    // walk does not have to.
    CHECK(layer.at(-1, 0) == gfx::empty_tile);
    CHECK(layer.at(0, 99) == gfx::empty_tile);
}

TEST_CASE("a non-default firstgid is subtracted")
{
    std::string text = map_of("", csv_2x2);
    const std::string::size_type at = text.find("firstgid=\"1\"");
    REQUIRE(at != std::string::npos);
    text.replace(at, 12, "firstgid=\"9\"");
    // gids 1..3 are now below firstgid; use ones in range.
    const std::string::size_type data = text.find("1,2,\n3,0");
    text.replace(data, 8, "9,10,\n11,0");

    const ScratchFile file("test_tmx.scratch.tmx", text);
    const loaders::TmxMap map = loaders::load_tmx(file.path());

    CHECK(map.tileset.first_gid == 9);
    CHECK(map.layers[0].tiles[0] == 0);
    CHECK(map.layers[0].tiles[1] == 1);
    CHECK(map.layers[0].tiles[3] == gfx::empty_tile);
}

TEST_CASE("the flip bits are masked off rather than read as a tile id")
{
    // Tiled sets the top bits when a tile is mirrored. Left in place the id
    // would be in the billions, which draws nothing and looks like map data
    // that went missing.
    const unsigned int flipped = 0x80000000u | 2u; // horizontal flip of gid 2
    const std::string data = "  <data encoding=\"csv\">1," +
                             std::to_string(flipped) + ",\n3,0</data>\n";

    const ScratchFile file("test_tmx.scratch.tmx", map_of("", data));
    const loaders::TmxMap map = loaders::load_tmx(file.path());

    CHECK(map.layers[0].tiles[1] == 1);
}

//============================================================================
// Rejected rather than mis-parsed
//============================================================================

TEST_CASE("a non-orthogonal map is rejected")
{
    std::string text = map_of("", csv_2x2);
    text.replace(text.find("orthogonal"), 10, "isometric");

    const ScratchFile iso("test_tmx.scratch.tmx", text);
    CHECK_THROWS_AS(loaders::load_tmx(iso.path()), loaders::TmxFormatError);
}

TEST_CASE(
    "an infinite map is rejected rather than read as a fraction of itself")
{
    const ScratchFile file("test_tmx.scratch.tmx",
                           map_of("infinite=\"1\"", csv_2x2));
    CHECK_THROWS_AS(loaders::load_tmx(file.path()), loaders::TmxFormatError);
}

TEST_CASE("a non-CSV encoding is rejected and names the setting to change")
{
    const ScratchFile base64(
        "test_tmx.scratch.tmx",
        map_of("", "  <data encoding=\"base64\">AQIDAA==</data>\n"));

    try {
        loaders::load_tmx(base64.path());
        FAIL("expected a TmxFormatError");
    } catch (const loaders::TmxFormatError& error) {
        const std::string what = error.what();
        CHECK(what.find("base64") != std::string::npos);
        CHECK(what.find("CSV") != std::string::npos);
    }

    // Tiled's oldest format is <data> with <tile> children and no encoding
    // attribute at all; it must not read as an empty layer.
    const ScratchFile xml("test_tmx.scratch2.tmx",
                          map_of("", "  <data><tile gid=\"1\"/></data>\n"));
    CHECK_THROWS_AS(loaders::load_tmx(xml.path()), loaders::TmxFormatError);
}

TEST_CASE("a layer whose data is the wrong length is rejected")
{
    // Three entries for a 2x2 layer. Read as-is this is a map that is subtly
    // the wrong shape, which presents as art that drifts diagonally.
    const ScratchFile file(
        "test_tmx.scratch.tmx",
        map_of("", "  <data encoding=\"csv\">1,2,3</data>\n"));
    CHECK_THROWS_AS(loaders::load_tmx(file.path()), loaders::TmxFormatError);
}

TEST_CASE("non-numeric CSV data is rejected")
{
    const ScratchFile file(
        "test_tmx.scratch.tmx",
        map_of("", "  <data encoding=\"csv\">1,two,3,0</data>\n"));
    CHECK_THROWS_AS(loaders::load_tmx(file.path()), loaders::TmxFormatError);
}

TEST_CASE("a map with several tilesets is rejected, not half-drawn")
{
    std::string text = map_of("", csv_2x2);
    const std::string second =
        " <tileset firstgid=\"5\" name=\"u\" tilewidth=\"16\" "
        "tileheight=\"16\" "
        "tilecount=\"4\" columns=\"2\">\n"
        "  <image source=\"u.png\" width=\"32\" height=\"32\"/>\n"
        " </tileset>\n";
    text.insert(text.find(" <layer"), second);

    const ScratchFile file("test_tmx.scratch.tmx", text);
    CHECK_THROWS_AS(loaders::load_tmx(file.path()), loaders::TmxFormatError);
}

TEST_CASE("a missing external tileset throws the posix error")
{
    const std::string text =
        "<map version=\"1.10\" orientation=\"orthogonal\" width=\"2\" "
        "height=\"2\" tilewidth=\"16\" tileheight=\"16\">\n"
        " <tileset firstgid=\"1\" source=\"nope.tsx\"/>\n"
        " <layer name=\"l\" width=\"2\" height=\"2\">\n"
        "  <data encoding=\"csv\">1,2,\n3,0</data>\n"
        " </layer>\n"
        "</map>\n";

    const ScratchFile file("test_tmx.scratch.tmx", text);
    CHECK_THROWS_AS(loaders::load_tmx(file.path()), posix::no_such_file);
}

TEST_CASE("a missing map file throws the posix error, not a format error")
{
    CHECK_THROWS_AS(loaders::load_tmx("data/does-not-exist.tmx"),
                    posix::no_such_file);
}

//============================================================================
// TileSet
//============================================================================

TEST_CASE("source rectangles are grid arithmetic, margin and spacing included")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 64, 64, /*fullscreen=*/false,
                                   gfx::renderer::Driver::Software);
    gfx::renderer::Texture texture(context, 64, 64);

    const gfx::TileSet plain(std::move(texture), 16, 16, 4, 16);
    CHECK(plain.source(0).x == 0);
    CHECK(plain.source(0).y == 0);
    CHECK(plain.source(3).x == 48);
    CHECK(plain.source(3).y == 0);
    CHECK(plain.source(4).x == 0);
    CHECK(plain.source(4).y == 16);
    CHECK(plain.source(5).w == 16);

    CHECK(plain.contains(0));
    CHECK(plain.contains(15));
    CHECK_FALSE(plain.contains(16));
    CHECK_FALSE(plain.contains(-1));
    CHECK(plain.fits_texture());

    gfx::renderer::Texture other(context, 64, 64);
    const gfx::TileSet padded(std::move(other), 16, 16, 3, 9, /*margin=*/2,
                              /*spacing=*/1);
    CHECK(padded.source(0).x == 2);
    CHECK(padded.source(1).x == 2 + 17);
    CHECK(padded.source(3).y == 2 + 17);
}

TEST_CASE("fits_texture catches a tileset that outruns its image")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 64, 64, /*fullscreen=*/false,
                                   gfx::renderer::Driver::Software);

    gfx::renderer::Texture small(context, 32, 32);
    // Claims 16 tiles of 16x16 in a 32x32 image, which holds 4.
    const gfx::TileSet lying(std::move(small), 16, 16, 4, 16);
    CHECK_FALSE(lying.fits_texture());
}

//============================================================================
// Culling
//============================================================================

TEST_CASE("draw visits only the tiles that intersect the target")
{
    VideoFixture video;
    // 64x64 target: four 16x16 tiles across and four down.
    gfx::renderer::Context context("test", 64, 64, /*fullscreen=*/false,
                                   gfx::renderer::Driver::Software);

    const loaders::TmxMap map = loaders::load_tmx("data/sunnyland.tmx");
    gfx::TileSet tiles = load_tileset(context, map);
    REQUIRE(tiles.fits_texture());

    const gfx::TileMap drawable(std::move(tiles), map.layers, map.width,
                                map.height);

    CHECK(drawable.pixel_width() == 64 * 16);
    CHECK(drawable.pixel_height() == 36 * 16);
    CHECK(drawable.find("ground") == 1);
    CHECK(drawable.find("nope") == gfx::TileMap::npos);

    // The backdrop alone would be 2304 tiles if nothing culled. A 64x64 target
    // sees at most 4x4 of them.
    const gfx::TileLayer& backdrop = drawable.layers()[0];

    context.clear({0, 0, 0, 255});
    const int drawn = drawable.draw_layer(context, backdrop, 0, 0);
    CHECK(drawn == 16);
}

TEST_CASE("a camera off the map's left edge keeps the partial column")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 64, 64, /*fullscreen=*/false,
                                   gfx::renderer::Driver::Software);

    const loaders::TmxMap map = loaders::load_tmx("data/sunnyland.tmx");
    gfx::TileSet tiles = load_tileset(context, map);
    const gfx::TileMap drawable(std::move(tiles), map.layers, map.width,
                                map.height);
    const gfx::TileLayer& backdrop = drawable.layers()[0];

    // Camera at -8: the map starts half a tile in from the left, so the target
    // shows four whole columns and part of a fifth... except the map's own
    // first column is the leftmost, so it is 4 columns of map over 4 rows.
    context.clear({0, 0, 0, 255});
    CHECK(drawable.draw_layer(context, backdrop, -8, 0) == 4 * 4);

    // Camera at +8, inside the map: now a partial column is exposed on both
    // sides, so five columns are touched. This is the case truncating division
    // gets wrong — it drops the leftmost.
    context.clear({0, 0, 0, 255});
    CHECK(drawable.draw_layer(context, backdrop, 8, 0) == 5 * 4);

    // And on both axes at once.
    context.clear({0, 0, 0, 255});
    CHECK(drawable.draw_layer(context, backdrop, 8, 8) == 5 * 5);
}

TEST_CASE("an invisible layer draws nothing, and scale enlarges tiles")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 64, 64, /*fullscreen=*/false,
                                   gfx::renderer::Driver::Software);

    const loaders::TmxMap map = loaders::load_tmx("data/sunnyland.tmx");
    gfx::TileSet tiles = load_tileset(context, map);
    gfx::TileMap drawable(std::move(tiles), map.layers, map.width, map.height);

    gfx::TileLayer& backdrop = drawable.layers()[0];
    backdrop.visible = false;
    context.clear({0, 0, 0, 255});
    CHECK(drawable.draw_layer(context, backdrop, 0, 0) == 0);

    backdrop.visible = true;

    // At 2x a tile covers 32 pixels, so the same target holds 2x2 of them.
    context.clear({0, 0, 0, 255});
    CHECK(drawable.draw_layer(context, backdrop, 0, 0, /*scale=*/2) == 4);

    // A scale below one draws nothing rather than inverting the rectangle.
    CHECK(drawable.draw_layer(context, backdrop, 0, 0, /*scale=*/0) == 0);
}

TEST_CASE("draw over all layers counts every blit, empties excluded")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 64, 64, /*fullscreen=*/false,
                                   gfx::renderer::Driver::Software);

    const loaders::TmxMap map = loaders::load_tmx("data/sunnyland.tmx");
    gfx::TileSet tiles = load_tileset(context, map);
    const gfx::TileMap drawable(std::move(tiles), map.layers, map.width,
                                map.height);

    // Top-left of the map is all backdrop and nothing else — the ground is at
    // the bottom — so three layers cost one layer's worth of blits.
    context.clear({0, 0, 0, 255});
    CHECK(drawable.draw(context, 0, 0) == 16);

    // Down at the surface, the ground layer contributes as well.
    context.clear({0, 0, 0, 255});
    CHECK(drawable.draw(context, 0, drawable.pixel_height() - 64) > 16);
}
