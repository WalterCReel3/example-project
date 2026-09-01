// game::Level — the collision layer, the spawn table, and what it refuses.
//
// Entirely headless, which is the whole point of the class taking parsed layers
// rather than a gfx::TileMap: there is no Context, no texture and no .tmx here.
// The layers below are built in the test, so what is under test is the
// validation and not loaders::load_tmx, which tests/test_tmx.cc already covers.
//
// The fixture is a 4x3 map of 16px tiles — 64x48 pixels — small enough that
// every boundary number in include/game/level.hpp can be written out and
// checked by eye. Those numbers are asserted here on purpose: the header states
// the spawn rule precisely, and a stated rule with no test is a claim.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <game/collide.hpp>
#include <game/level.hpp>
#include <gfx/tilemap.hpp>

#include <limits>
#include <string>
#include <vector>

namespace
{

const int tile = 16;
const int map_columns = 4;
const int map_rows = 3;

const int pixel_width = map_columns * tile; // 64
const int pixel_height = map_rows * tile;   // 48

gfx::TileLayer layer_named(const std::string& name, int width, int height)
{
    gfx::TileLayer layer;
    layer.name = name;
    layer.width = width;
    layer.height = height;
    layer.tiles.assign(static_cast<std::size_t>(width) * height,
                       gfx::empty_tile);
    return layer;
}

gfx::TileLayer collision_layer()
{
    return layer_named("collision", map_columns, map_rows);
}

void fill(gfx::TileLayer& layer, int column, int row)
{
    layer.tiles[static_cast<std::size_t>(row) * layer.width + column] = 0;
}

gfx::MapObject object(int id, const std::string& type, double x, double y,
                      double w = 0.0, double h = 0.0)
{
    gfx::MapObject out;
    out.id = id;
    out.type = type;
    out.name = type;
    out.x = x;
    out.y = y;
    out.width = w;
    out.height = h;
    return out;
}

gfx::ObjectLayer objects_named(const std::string& name,
                               std::vector<gfx::MapObject> objects)
{
    gfx::ObjectLayer layer;
    layer.name = name;
    layer.objects = std::move(objects);
    return layer;
}

// A level over the standard fixture, with whatever spawns are handed in.
game::Level level_with(std::vector<gfx::MapObject> spawns)
{
    const std::vector<gfx::TileLayer> layers{collision_layer()};
    const std::vector<gfx::ObjectLayer> object_layers{
        objects_named("spawns", std::move(spawns))};

    return game::Level(layers, object_layers, map_columns, map_rows, tile, tile,
                       "fixture.tmx");
}

// Whether a single spawn is accepted. The bounds rule is only reachable through
// the constructor, which is the interface a caller has, so that is what the
// boundary cases below drive.
bool accepts(const gfx::MapObject& spawn)
{
    try {
        level_with({spawn});
    } catch (const game::LevelError&) {
        return false;
    }
    return true;
}

} // namespace

//============================================================================
// What it builds
//============================================================================

TEST_CASE("a well-formed level resolves its collision layer and its spawns")
{
    gfx::TileLayer collision = collision_layer();
    fill(collision, 1, 2);

    const std::vector<gfx::TileLayer> layers{
        layer_named("backdrop", map_columns, map_rows), collision,
        layer_named("decoration", map_columns, map_rows)};
    const std::vector<gfx::ObjectLayer> object_layers{
        objects_named("spawns", {object(1, "player", 16.0, 16.0, 16.0, 16.0),
                                 object(2, "cherry", 32.0, 0.0)})};

    const game::Level level(layers, object_layers, map_columns, map_rows, tile,
                            tile, "fixture.tmx");

    CHECK(level.width() == map_columns);
    CHECK(level.height() == map_rows);
    CHECK(level.tile_width() == tile);
    CHECK(level.tile_height() == tile);
    CHECK(level.pixel_width() == pixel_width);
    CHECK(level.pixel_height() == pixel_height);

    REQUIRE(level.spawns().size() == 2);
    CHECK(level.spawns()[0].type == "player");
    CHECK(level.spawns()[1].id == 2);

    // The `collision` layer was picked out of three, not the first one.
    CHECK(game::solid_at(level.collision(), 1, 2));
    CHECK(!game::solid_at(level.collision(), 0, 0));
}

TEST_CASE("the collision grid answers game::collide over the level's own copy")
{
    std::vector<gfx::TileLayer> layers{collision_layer()};
    fill(layers[0], 2, 0);

    const std::vector<gfx::ObjectLayer> object_layers{
        objects_named("spawns", {})};
    const game::Level level(layers, object_layers, map_columns, map_rows, tile,
                            tile, "fixture.tmx");

    // A 16-wide box at x = 0 pushed right by 32 stops with its RIGHT edge flush
    // on column 2's near edge, so its origin lands at 16 and not at 32.
    const game::Resolution moved = game::slide(
        level.collision(), game::Aabb{0.0, 0.0, 16.0, 16.0}, 32.0, 0.0);
    CHECK(moved.x == doctest::Approx(16.0));
    CHECK(moved.hit_x);

    // The layer was copied at load, so editing the source afterwards does not
    // reach the level. This is the property that lets the drawable gfx::TileMap
    // be moved or dropped independently.
    layers[0].tiles.assign(layers[0].tiles.size(), gfx::empty_tile);
    CHECK(game::solid_at(level.collision(), 2, 0));
}

TEST_CASE("an empty spawns layer is legal")
{
    // The module does not know that a `player` is required and must not learn:
    // that vocabulary belongs to the demo. See decision 8.
    const game::Level level = level_with({});
    CHECK(level.spawns().empty());
}

TEST_CASE("the first layer of each name wins")
{
    gfx::TileLayer first = collision_layer();
    fill(first, 0, 0);
    const gfx::TileLayer second = collision_layer();

    const std::vector<gfx::TileLayer> layers{first, second};
    const std::vector<gfx::ObjectLayer> object_layers{
        objects_named("spawns", {object(1, "player", 0.0, 0.0)}),
        objects_named("spawns", {object(2, "frog", 0.0, 0.0)})};

    const game::Level level(layers, object_layers, map_columns, map_rows, tile,
                            tile, "fixture.tmx");

    CHECK(game::solid_at(level.collision(), 0, 0));
    REQUIRE(level.spawns().size() == 1);
    CHECK(level.spawns()[0].type == "player");
}

//============================================================================
// What it refuses
//============================================================================

TEST_CASE("a missing collision layer is refused")
{
    const std::vector<gfx::TileLayer> layers{
        layer_named("ground", map_columns, map_rows)};
    const std::vector<gfx::ObjectLayer> object_layers{
        objects_named("spawns", {})};

    CHECK_THROWS_AS(game::Level(layers, object_layers, map_columns, map_rows,
                                tile, tile, "fixture.tmx"),
                    game::LevelError);
}

TEST_CASE("a missing spawns layer is refused")
{
    const std::vector<gfx::TileLayer> layers{collision_layer()};
    const std::vector<gfx::ObjectLayer> object_layers{
        objects_named("objects", {object(1, "player", 0.0, 0.0)})};

    CHECK_THROWS_AS(game::Level(layers, object_layers, map_columns, map_rows,
                                tile, tile, "fixture.tmx"),
                    game::LevelError);
}

TEST_CASE("a collision layer that is not the map's size is refused")
{
    const std::vector<gfx::ObjectLayer> object_layers{
        objects_named("spawns", {})};

    // Short: everything past column 2 would be solid, in a map whose art runs
    // to column 3.
    const std::vector<gfx::TileLayer> short_layer{
        layer_named("collision", map_columns - 1, map_rows)};
    CHECK_THROWS_AS(game::Level(short_layer, object_layers, map_columns,
                                map_rows, tile, tile, "fixture.tmx"),
                    game::LevelError);

    // Long, which is the same disagreement the other way round.
    const std::vector<gfx::TileLayer> long_layer{
        layer_named("collision", map_columns, map_rows + 1)};
    CHECK_THROWS_AS(game::Level(long_layer, object_layers, map_columns,
                                map_rows, tile, tile, "fixture.tmx"),
                    game::LevelError);
}

TEST_CASE("a collision layer whose data is not width x height is refused")
{
    const std::vector<gfx::ObjectLayer> object_layers{
        objects_named("spawns", {})};

    // The layer declares the map's size, so the size check passes, but it
    // carries one cell. collide bounds-checks against the declared width and
    // height and then reads unchecked, so solid_at(3, 2) would read 40 bytes
    // past the end of this vector.
    gfx::TileLayer short_data = collision_layer();
    short_data.tiles.assign(1, gfx::empty_tile);
    const std::vector<gfx::TileLayer> short_layers{short_data};
    CHECK_THROWS_AS(game::Level(short_layers, object_layers, map_columns,
                                map_rows, tile, tile, "fixture.tmx"),
                    game::LevelError);

    // Empty is the same defect at its sharpest: every query reads out of
    // bounds.
    gfx::TileLayer no_data = collision_layer();
    no_data.tiles.clear();
    const std::vector<gfx::TileLayer> empty_layers{no_data};
    CHECK_THROWS_AS(game::Level(empty_layers, object_layers, map_columns,
                                map_rows, tile, tile, "fixture.tmx"),
                    game::LevelError);

    // Long is the same disagreement the other way round. It reads in bounds,
    // but the surplus is data the author wrote and nothing will ever consult.
    gfx::TileLayer long_data = collision_layer();
    long_data.tiles.push_back(gfx::empty_tile);
    const std::vector<gfx::TileLayer> long_layers{long_data};
    CHECK_THROWS_AS(game::Level(long_layers, object_layers, map_columns,
                                map_rows, tile, tile, "fixture.tmx"),
                    game::LevelError);
}

TEST_CASE("a collision layer with a draw offset is refused")
{
    const std::vector<gfx::ObjectLayer> object_layers{
        objects_named("spawns", {})};

    gfx::TileLayer offset = collision_layer();
    offset.offset_y = 8;
    const std::vector<gfx::TileLayer> layers{offset};

    // game::TileGrid has no offset, so the solid cells would sit eight pixels
    // from where the layer draws.
    CHECK_THROWS_AS(game::Level(layers, object_layers, map_columns, map_rows,
                                tile, tile, "fixture.tmx"),
                    game::LevelError);
}

TEST_CASE("a non-positive dimension is refused")
{
    const std::vector<gfx::TileLayer> layers{collision_layer()};
    const std::vector<gfx::ObjectLayer> object_layers{
        objects_named("spawns", {})};

    CHECK_THROWS_AS(
        game::Level(layers, object_layers, 0, map_rows, tile, tile, "f.tmx"),
        game::LevelError);
    CHECK_THROWS_AS(game::Level(layers, object_layers, map_columns, map_rows,
                                tile, 0, "f.tmx"),
                    game::LevelError);
}

TEST_CASE("a map whose pixel extent does not fit in an int is refused")
{
    const std::vector<gfx::TileLayer> layers{collision_layer()};
    const std::vector<gfx::ObjectLayer> object_layers{
        objects_named("spawns", {})};

    // pixel_width() and pixel_height() are int products and the spawn rule is
    // stated against them, so the map is refused before anything multiplies.
    // The message is checked because a map this wide also disagrees with the
    // fixture's layer, and that refusal would satisfy CHECK_THROWS_AS on its
    // own — this asserts which of the two fired.
    const int int_max = std::numeric_limits<int>::max();

    try {
        game::Level(layers, object_layers, int_max / tile + 1, map_rows, tile,
                    tile, "fixture.tmx");
        FAIL("expected a LevelError");
    } catch (const game::LevelError& error) {
        CHECK(std::string(error.what())
                  .find("wider than this level can "
                        "measure") != std::string::npos);
    }

    try {
        game::Level(layers, object_layers, map_columns, int_max / tile + 1,
                    tile, tile, "fixture.tmx");
        FAIL("expected a LevelError");
    } catch (const game::LevelError& error) {
        CHECK(std::string(error.what())
                  .find("wider than this level can "
                        "measure") != std::string::npos);
    }
}

//============================================================================
// The spawn bounds rule
//
// Every number below is quoted in include/game/level.hpp. The level is
// [0, 64) x [0, 48).
//============================================================================

TEST_CASE("a spawn box on the last legal pixel is inside, one past is not")
{
    // Right edge exactly on pixel_width(): the box covers the last legal
    // column and not the one after it.
    CHECK(accepts(object(1, "frog", 48.0, 32.0, 16.0, 16.0)));

    // One pixel further right, so the box asks for pixel 64.
    CHECK(!accepts(object(2, "frog", 49.0, 32.0, 16.0, 16.0)));

    // The same on y, independently: legal x, one pixel too far down.
    CHECK(!accepts(object(3, "frog", 48.0, 33.0, 16.0, 16.0)));

    // And flush at the origin.
    CHECK(accepts(object(4, "frog", 0.0, 0.0, 16.0, 16.0)));
}

TEST_CASE("a spawn box straddling the edge is refused")
{
    // THE QUIET CASE. collide would let this one walk back inside on its first
    // frame reporting no hit, after which nothing about the running game says
    // the level data is wrong. Decision 10.
    CHECK(!accepts(object(1, "opossum", -8.0, 0.0, 16.0, 16.0)));

    // Flush outside: the box exactly fills the cell adjacent to column 0, which
    // is collide's documented walks-home case.
    CHECK(!accepts(object(2, "opossum", -16.0, 0.0, 16.0, 16.0)));

    // Far outside, which collide would freeze. Refused for the same reason at
    // the same place, so the two do not have to be told apart.
    CHECK(!accepts(object(3, "opossum", -40.0, 0.0, 16.0, 16.0)));

    // A fraction of a pixel is still outside.
    CHECK(!accepts(object(4, "opossum", -0.5, 0.0, 16.0, 16.0)));

    // Straddling the far edge, which the x + width test catches.
    CHECK(!accepts(object(5, "opossum", 56.0, 0.0, 16.0, 16.0)));

    // The same four on y, which is a separate call to the same rule: a box
    // above the map straddles the top edge exactly as the -8.0 one straddles
    // the left. y >= 0 is stated in the header for an object with extent, so
    // it is asserted here rather than only for point objects.
    CHECK(!accepts(object(6, "opossum", 0.0, -8.0, 16.0, 16.0)));
    CHECK(!accepts(object(7, "opossum", 0.0, -16.0, 16.0, 16.0)));
    CHECK(!accepts(object(8, "opossum", 0.0, -0.5, 16.0, 16.0)));

    // And straddling the bottom edge, the mirror of the x + width case.
    CHECK(!accepts(object(9, "opossum", 0.0, 40.0, 16.0, 16.0)));
}

TEST_CASE("a point spawn is inside up to but not including the far edge")
{
    // Tiled writes a point object as width = 0, height = 0.
    CHECK(accepts(object(1, "cherry", 0.0, 0.0)));
    CHECK(accepts(object(2, "cherry", 63.9, 47.9)));

    // Exactly on the far edge is the first pixel that is not in the map. This
    // is where the empty-box reading would have said yes.
    CHECK(!accepts(object(3, "cherry", 64.0, 0.0)));
    CHECK(!accepts(object(4, "cherry", 0.0, 48.0)));

    // And below the origin.
    CHECK(!accepts(object(5, "cherry", -0.5, 0.0)));
    CHECK(!accepts(object(6, "cherry", 0.0, -0.5)));
}

TEST_CASE("each axis is judged by its own extent")
{
    // Extent on x, none on y: the x edge may land on 64, the y point may not
    // land on 48.
    CHECK(accepts(object(1, "gem", 48.0, 47.0, 16.0, 0.0)));
    CHECK(!accepts(object(2, "gem", 48.0, 48.0, 16.0, 0.0)));

    // The mirror image.
    CHECK(accepts(object(3, "gem", 63.0, 32.0, 0.0, 16.0)));
    CHECK(!accepts(object(4, "gem", 64.0, 32.0, 0.0, 16.0)));

    // A negative extent reads as a point, so it is judged by `< limit` rather
    // than by a right edge that would sit left of its own origin.
    CHECK(accepts(object(5, "gem", 63.0, 47.0, -4.0, -4.0)));
    CHECK(!accepts(object(6, "gem", 64.0, 47.0, -4.0, -4.0)));
}

TEST_CASE("the two tile dimensions are not interchangeable")
{
    // Non-square cells, so transposing tile_width and tile_height anywhere in
    // the class shows up here rather than in a level nobody has built yet. The
    // map is 4x3 cells of 16x8 pixels: 64 wide and 24 tall.
    const int tall = 8;
    const std::vector<gfx::TileLayer> layers{collision_layer()};
    const std::vector<gfx::ObjectLayer> object_layers{
        objects_named("spawns", {})};

    const game::Level level(layers, object_layers, map_columns, map_rows, tile,
                            tall, "fixture.tmx");

    CHECK(level.tile_width() == tile);
    CHECK(level.tile_height() == tall);
    CHECK(level.pixel_width() == 64);
    CHECK(level.pixel_height() == 24);

    // The collision grid gets the same pair the level was given, the right way
    // round: cell (0, 1) spans y in [8, 16), so a point at y = 12 is in row 1.
    const game::TileGrid solid = level.collision();
    CHECK(solid.tile_width == tile);
    CHECK(solid.tile_height == tall);

    // And the spawn bounds follow the short axis, not the long one. y = 24 is
    // the first row of pixels outside a 24-pixel-tall level; under a square
    // reading it would still be inside.
    const std::vector<gfx::ObjectLayer> low{
        objects_named("spawns", {object(1, "frog", 0.0, 24.0)})};
    CHECK_THROWS_AS(game::Level(layers, low, map_columns, map_rows, tile, tall,
                                "fixture.tmx"),
                    game::LevelError);

    const std::vector<gfx::ObjectLayer> just_inside{
        objects_named("spawns", {object(2, "frog", 0.0, 23.0)})};
    CHECK_NOTHROW(game::Level(layers, just_inside, map_columns, map_rows, tile,
                              tall, "fixture.tmx"));
}

//============================================================================
// The messages
//============================================================================

TEST_CASE("a refusal names the source, the object and the level")
{
    try {
        level_with({object(7, "frog", 200.0, 0.0, 16.0, 16.0)});
        FAIL("expected a LevelError");
    } catch (const game::LevelError& error) {
        const std::string message = error.what();

        // The id is what a .tmx is greppable by.
        CHECK(message.find("id=7") != std::string::npos);
        CHECK(message.find("frog") != std::string::npos);
        CHECK(message.find("fixture.tmx") != std::string::npos);
        CHECK(message.find("64x48") != std::string::npos);
    }
}

TEST_CASE("a missing layer names the layer it wanted")
{
    const std::vector<gfx::TileLayer> layers{
        layer_named("ground", map_columns, map_rows)};
    const std::vector<gfx::ObjectLayer> object_layers{
        objects_named("spawns", {})};

    try {
        game::Level(layers, object_layers, map_columns, map_rows, tile, tile,
                    "fixture.tmx");
        FAIL("expected a LevelError");
    } catch (const game::LevelError& error) {
        const std::string message = error.what();
        CHECK(message.find("collision") != std::string::npos);
        CHECK(message.find("fixture.tmx") != std::string::npos);
    }
}

TEST_CASE("an unnamed source leaves the message without a prefix")
{
    const std::vector<gfx::TileLayer> layers{
        layer_named("ground", map_columns, map_rows)};
    const std::vector<gfx::ObjectLayer> object_layers{
        objects_named("spawns", {})};

    try {
        game::Level(layers, object_layers, map_columns, map_rows, tile, tile);
        FAIL("expected a LevelError");
    } catch (const game::LevelError& error) {
        const std::string message = error.what();
        CHECK(message.find("no tile layer") == 0);
    }
}
