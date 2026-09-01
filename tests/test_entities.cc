// game::Entities — the flat entity store.
//
// Nearly all of it is headless: the store integrates positions and orders
// indexes, and an AnimatedSprite with no atlas advances perfectly well without
// one. The cases that need a Context are the ones that read pixels back, and
// both groups of them are at the foot of the file: which sprite ends up on top
// and which pixel a world coordinate lands in are only observable there.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <game/collide.hpp>
#include <game/entities.hpp>
#include <gfx/animation.hpp>
#include <gfx/atlas.hpp>
#include <gfx/system.hpp>
#include <gfx/tilemap.hpp>

#include <SDL.h>

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

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

game::Entity at(double x, double y, int layer = 0)
{
    game::Entity entity;
    entity.x = x;
    entity.y = y;
    entity.layer = layer;
    return entity;
}

} // namespace

//============================================================================
// Handles
//============================================================================

TEST_CASE("add returns a live handle and size counts live entities")
{
    game::Entities entities;
    CHECK(entities.empty());

    const game::Entities::Id a = entities.add(at(1.0, 2.0));
    const game::Entities::Id b = entities.add(at(3.0, 4.0));

    CHECK(entities.size() == 2u);
    CHECK(entities.alive(a));
    CHECK(entities.alive(b));
    CHECK(a != b);

    REQUIRE(entities.find(a) != nullptr);
    CHECK(entities.find(a)->x == doctest::Approx(1.0));
    CHECK(entities.find(b)->y == doctest::Approx(4.0));
}

TEST_CASE("a removed handle goes stale and find returns null")
{
    game::Entities entities;
    const game::Entities::Id id = entities.add(at(1.0, 1.0));

    entities.remove(id);

    CHECK(entities.size() == 0u);
    CHECK_FALSE(entities.alive(id));
    CHECK(entities.find(id) == nullptr);

    // Removing twice is not an error.
    entities.remove(id);
    CHECK(entities.size() == 0u);
}

TEST_CASE("a reused slot does not answer to the old handle")
{
    // The bug the generation exists for. With bare indexes, the handle to a
    // removed entity addresses whatever takes its slot, and the symptom is one
    // entity inheriting another's position or animation.
    game::Entities entities;

    const game::Entities::Id first = entities.add(at(10.0, 10.0));
    entities.remove(first);

    const game::Entities::Id second = entities.add(at(99.0, 99.0));

    // The slot was reused — that is the point of the free list.
    CHECK(second.index == first.index);

    // But the old handle does not reach it.
    CHECK_FALSE(entities.alive(first));
    CHECK(entities.find(first) == nullptr);

    CHECK(entities.alive(second));
    REQUIRE(entities.find(second) != nullptr);
    CHECK(entities.find(second)->x == doctest::Approx(99.0));
}

TEST_CASE("none() is never a live handle, in a fresh store or a used one")
{
    game::Entities entities;
    CHECK_FALSE(entities.alive(game::Entities::none()));

    entities.add(at(0.0, 0.0));
    CHECK_FALSE(entities.alive(game::Entities::none()));
    CHECK(entities.find(game::Entities::none()) == nullptr);
}

TEST_CASE("clear empties the store and strands every handle")
{
    game::Entities entities;
    const game::Entities::Id a = entities.add(at(1.0, 1.0));
    const game::Entities::Id b = entities.add(at(2.0, 2.0));

    entities.clear();

    CHECK(entities.empty());
    CHECK_FALSE(entities.alive(a));
    CHECK_FALSE(entities.alive(b));

    // A handle from before the clear must not come back to life against the
    // slot its index now names.
    const game::Entities::Id fresh = entities.add(at(5.0, 5.0));
    CHECK(entities.alive(fresh));
    CHECK_FALSE(entities.alive(a));
}

//============================================================================
// The collision box and the behaviour tag
//============================================================================

TEST_CASE("a default entity is untagged and carries an empty box")
{
    const game::Entity entity;

    // The sentinel has to be outside the range a consumer enum writes by
    // accident. A consumer's `enum class Behaviour { Player, ... }` makes
    // Player 0, so a 0 sentinel would make every player look untagged.
    CHECK(game::no_behaviour < 0);
    CHECK(entity.behaviour == game::no_behaviour);
    CHECK_FALSE(entity.behaviour == 0);

    // Empty, not a unit box: test_collide pins that an empty box overlaps
    // nothing, so an entity nobody gave a box to is inert rather than a point
    // collider at its origin.
    CHECK(entity.box.width == doctest::Approx(0.0));
    CHECK(entity.box.height == doctest::Approx(0.0));
}

TEST_CASE("the box and the tag round-trip through a handle")
{
    game::Entities entities;

    game::Entity actor = at(30.0, 40.0);
    actor.box = game::Aabb{2.0, 4.0, 12.0, 20.0};
    actor.behaviour = 3;

    const game::Entities::Id id = entities.add(actor);

    REQUIRE(entities.find(id) != nullptr);
    const game::Entity& stored = *entities.find(id);
    CHECK(stored.behaviour == 3);
    CHECK(stored.box.x == doctest::Approx(2.0));
    CHECK(stored.box.y == doctest::Approx(4.0));
    CHECK(stored.box.width == doctest::Approx(12.0));
    CHECK(stored.box.height == doctest::Approx(20.0));
}

TEST_CASE("world_box offsets the box by the entity position")
{
    // The box is stored relative to (x, y). Getting the direction of that
    // addition wrong is invisible at the origin, so this uses a position and
    // an offset that are both non-zero and different from each other.
    game::Entity actor = at(100.0, 200.0);
    actor.box = game::Aabb{3.0, 5.0, 10.0, 14.0};

    const game::Aabb world = actor.world_box();
    CHECK(world.x == doctest::Approx(103.0));
    CHECK(world.y == doctest::Approx(205.0));

    // The extent is not a coordinate and must not be translated with them.
    CHECK(world.width == doctest::Approx(10.0));
    CHECK(world.height == doctest::Approx(14.0));
}

TEST_CASE("a spawn rectangle fills the box without a conversion")
{
    // The header documents this exact assignment, so it is pinned rather than
    // left as prose. A Tiled object measures from the map origin, which is the
    // space Entity::x/y is already in, so the offset for a box that is the
    // whole object is zero and world_box() reproduces the object's rectangle.
    gfx::MapObject spawn;
    spawn.x = 64.0;
    spawn.y = 48.0;
    spawn.width = 16.0;
    spawn.height = 24.0;

    game::Entity actor;
    actor.x = spawn.x;
    actor.y = spawn.y;
    actor.box = game::Aabb{0.0, 0.0, spawn.width, spawn.height};

    const game::Aabb world = actor.world_box();
    CHECK(world.x == doctest::Approx(spawn.x));
    CHECK(world.y == doctest::Approx(spawn.y));
    CHECK(world.width == doctest::Approx(spawn.width));
    CHECK(world.height == doctest::Approx(spawn.height));
}

TEST_CASE("the world box follows the entity as update integrates it")
{
    // The reason the box is relative: position is integrated in one place and
    // the box has to move with it without anyone remembering to move it.
    game::Entities entities;

    game::Entity mover = at(0.0, 0.0);
    mover.velocity_x = 50.0;
    mover.box = game::Aabb{1.0, 0.0, 8.0, 8.0};
    const game::Entities::Id id = entities.add(mover);

    entities.update(2.0);

    REQUIRE(entities.find(id) != nullptr);
    const game::Aabb world = entities.find(id)->world_box();
    CHECK(world.x == doctest::Approx(101.0));

    // The stored box is untouched by the move; only its origin changed.
    CHECK(entities.find(id)->box.x == doctest::Approx(1.0));
}

TEST_CASE("a reused slot does not inherit the previous box or tag")
{
    // Caller-visible property: an entity handed to add() is the entity that
    // comes back, whether or not its slot was somebody else's a moment ago.
    game::Entities entities;

    game::Entity tagged = at(10.0, 10.0);
    tagged.box = game::Aabb{1.0, 2.0, 30.0, 40.0};
    tagged.behaviour = 7;

    const game::Entities::Id first = entities.add(tagged);
    entities.remove(first);

    const game::Entities::Id second = entities.add(at(99.0, 99.0));

    // The slot really was reused, so there was something to inherit.
    REQUIRE(second.index == first.index);

    REQUIRE(entities.find(second) != nullptr);
    const game::Entity& fresh = *entities.find(second);
    CHECK(fresh.behaviour == game::no_behaviour);
    CHECK(fresh.box.width == doctest::Approx(0.0));
    CHECK(fresh.box.height == doctest::Approx(0.0));
    CHECK(fresh.box.x == doctest::Approx(0.0));
    CHECK(fresh.box.y == doctest::Approx(0.0));
}

TEST_CASE("a reused slot receives the new entity's box and tag")
{
    // The other half of the case above, and the half that has teeth. There the
    // incoming entity carries the defaults and the assertions look for the
    // defaults — which is what remove()'s clear already left in the slot, so
    // add() storing nothing at all would pass it. Here the incoming entity is
    // non-default in every field the previous test checks, so only an add()
    // that actually writes the slot can satisfy it.
    game::Entities entities;

    game::Entity previous = at(10.0, 10.0);
    previous.box = game::Aabb{1.0, 2.0, 30.0, 40.0};
    previous.behaviour = 7;

    const game::Entities::Id first = entities.add(previous);
    entities.remove(first);

    game::Entity replacement = at(99.0, 99.0, 3);
    replacement.box = game::Aabb{-4.0, -5.0, 12.0, 24.0};
    replacement.behaviour = 42;
    replacement.velocity_x = 8.0;
    replacement.scale = 2;

    const game::Entities::Id second = entities.add(replacement);

    // The slot really was reused, so this is the reuse path and not a
    // push_back.
    REQUIRE(second.index == first.index);

    REQUIRE(entities.find(second) != nullptr);
    const game::Entity& stored = *entities.find(second);
    CHECK(stored.behaviour == 42);
    CHECK(stored.box.x == doctest::Approx(-4.0));
    CHECK(stored.box.y == doctest::Approx(-5.0));
    CHECK(stored.box.width == doctest::Approx(12.0));
    CHECK(stored.box.height == doctest::Approx(24.0));
    CHECK(stored.x == doctest::Approx(99.0));
    CHECK(stored.y == doctest::Approx(99.0));
    CHECK(stored.layer == 3);
    CHECK(stored.scale == 2);
    CHECK(stored.velocity_x == doctest::Approx(8.0));

    // And the composed box is the new one against the new origin, not a
    // survivor of the previous tenant.
    const game::Aabb world = stored.world_box();
    CHECK(world.x == doctest::Approx(95.0));
    CHECK(world.y == doctest::Approx(94.0));
    CHECK(world.width == doctest::Approx(12.0));
}

//============================================================================
// The update tick
//============================================================================

TEST_CASE("update integrates velocity against the delta")
{
    game::Entities entities;

    game::Entity moving = at(0.0, 0.0);
    moving.velocity_x = 60.0;
    moving.velocity_y = -30.0;
    const game::Entities::Id id = entities.add(moving);

    entities.update(0.5);

    REQUIRE(entities.find(id) != nullptr);
    CHECK(entities.find(id)->x == doctest::Approx(30.0));
    CHECK(entities.find(id)->y == doctest::Approx(-15.0));

    // Position is a double so that slow movement accumulates instead of being
    // rounded away every frame.
    game::Entity slow = at(0.0, 0.0);
    slow.velocity_x = 1.0;
    const game::Entities::Id crawler = entities.add(slow);

    for (int i = 0; i < 10; ++i) {
        entities.update(0.1);
    }
    CHECK(entities.find(crawler)->x == doctest::Approx(1.0));
}

TEST_CASE("update ignores a zero or negative delta and skips dead slots")
{
    game::Entities entities;

    game::Entity moving = at(5.0, 0.0);
    moving.velocity_x = 100.0;
    const game::Entities::Id id = entities.add(moving);

    entities.update(0.0);
    CHECK(entities.find(id)->x == doctest::Approx(5.0));
    entities.update(-1.0);
    CHECK(entities.find(id)->x == doctest::Approx(5.0));

    // A removed entity must not keep integrating in its slot.
    const game::Entities::Id dead = entities.add(moving);
    entities.remove(dead);
    entities.update(1.0);
    CHECK(entities.size() == 1u);
}

TEST_CASE("each visits live entities only")
{
    game::Entities entities;
    entities.add(at(1.0, 0.0));
    const game::Entities::Id gone = entities.add(at(2.0, 0.0));
    entities.add(at(3.0, 0.0));
    entities.remove(gone);

    std::vector<double> seen;
    entities.each([&seen](game::Entity& e) { seen.push_back(e.x); });

    REQUIRE(seen.size() == 2u);
    CHECK(seen[0] == doctest::Approx(1.0));
    CHECK(seen[1] == doctest::Approx(3.0));
}

//============================================================================
// Draw ordering
//============================================================================

TEST_CASE("draw counts live entities and skips removed ones")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 64, 64, /*fullscreen=*/false);

    game::Entities entities;
    entities.add(at(0.0, 0.0));
    const game::Entities::Id gone = entities.add(at(1.0, 1.0));
    entities.add(at(2.0, 2.0));
    entities.remove(gone);

    // The sprites have no atlas, so nothing lands on screen; what is being
    // checked is that the store visited two entities and not three.
    CHECK(entities.draw(context, 0, 0) == 2);
}

TEST_CASE("drawing does not reorder the store")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 64, 64, /*fullscreen=*/false);

    game::Entities entities;

    // Deliberately added out of draw order.
    entities.add(at(0.0, 50.0, /*layer=*/1)); // front layer, high up
    entities.add(at(0.0, 10.0, /*layer=*/0)); // back layer, high up
    entities.add(at(0.0, 90.0, /*layer=*/0)); // back layer, low down
    entities.add(at(0.0, 20.0, /*layer=*/1)); // front layer, high up

    REQUIRE(entities.draw(context, 0, 0) == 4);

    // The sort is over a private index vector, not over the slots, so the
    // positions handed out by add() stay put and each() still reports the order
    // things were added in. What the sort actually orders is pinned in pixels
    // at the foot of this file.
    std::vector<std::pair<int, double>> keys;
    entities.each([&keys](game::Entity& e) { keys.push_back({e.layer, e.y}); });

    REQUIRE(keys.size() == 4u);
    CHECK(keys[0] == std::make_pair(1, 50.0));
    CHECK(keys[1] == std::make_pair(0, 10.0));
    CHECK(keys[2] == std::make_pair(0, 90.0));
    CHECK(keys[3] == std::make_pair(1, 20.0));
}

TEST_CASE("an empty store draws nothing without touching the renderer")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 64, 64, /*fullscreen=*/false);

    game::Entities entities;
    CHECK(entities.draw(context, 0, 0) == 0);
}

//============================================================================
// Blit rounding
//============================================================================

namespace
{

// A one-pixel opaque white atlas, so "where did this land" has exactly one
// answer. The frame is untrimmed, which keeps the trim offset out of the
// arithmetic under test.
gfx::Atlas white_pixel(gfx::renderer::Context& context)
{
    SDL_Surface* surface =
        SDL_CreateRGBSurfaceWithFormat(0, 1, 1, 32, SDL_PIXELFORMAT_RGBA32);
    REQUIRE(surface != nullptr);
    SDL_FillRect(surface, nullptr,
                 SDL_MapRGBA(surface->format, 255, 255, 255, 255));

    gfx::renderer::Texture sheet(context, surface);
    SDL_FreeSurface(surface);

    gfx::AtlasFrame frame;
    frame.id = "dot.000";
    frame.source = gfx::renderer::Rect{0, 0, 1, 1};

    gfx::Atlas::Frames frames;
    frames.push_back(frame);

    return gfx::Atlas(std::move(sheet), std::move(frames));
}

// The whole framebuffer as ARGB8888, row-major, one entry per pixel.
std::vector<std::uint32_t> read_framebuffer(gfx::renderer::Context& context)
{
    const int width = context.width();
    const int height = context.height();

    std::vector<std::uint32_t> pixels(
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0);
    REQUIRE(SDL_RenderReadPixels(context.renderer(), nullptr,
                                 SDL_PIXELFORMAT_ARGB8888, pixels.data(),
                                 width * 4) == 0);

    return pixels;
}

// Draws one entity at (x, y) with the given camera and reports the column the
// white pixel landed in, or -1 if nothing was lit. Column, not row: the y cases
// below read the row out of the same helper by swapping the axes at the call
// site.
int lit_column(gfx::renderer::Context& context, const gfx::Atlas& atlas,
               const gfx::Animation& animation, double x, double y,
               int camera_x, int camera_y)
{
    game::Entities entities;
    game::Entity entity = at(x, y);
    entity.sprite = gfx::AnimatedSprite(atlas, animation);
    entities.add(std::move(entity));

    context.clear(gfx::renderer::Color{0, 0, 0, 255});
    REQUIRE(entities.draw(context, camera_x, camera_y) == 1);

    const int width = context.width();
    const int height = context.height();
    const std::vector<std::uint32_t> pixels = read_framebuffer(context);

    for (int row = 0; row < height; ++row) {
        for (int column = 0; column < width; ++column) {
            const std::uint32_t pixel =
                pixels[static_cast<std::size_t>(row) *
                           static_cast<std::size_t>(width) +
                       static_cast<std::size_t>(column)];
            if ((pixel & 0x00ffffffu) != 0u) {
                return column;
            }
        }
    }

    return -1;
}

// Same, reporting the row.
int lit_row(gfx::renderer::Context& context, const gfx::Atlas& atlas,
            const gfx::Animation& animation, double x, double y, int camera_x,
            int camera_y)
{
    game::Entities entities;
    game::Entity entity = at(x, y);
    entity.sprite = gfx::AnimatedSprite(atlas, animation);
    entities.add(std::move(entity));

    context.clear(gfx::renderer::Color{0, 0, 0, 255});
    REQUIRE(entities.draw(context, camera_x, camera_y) == 1);

    const int width = context.width();
    const int height = context.height();
    const std::vector<std::uint32_t> pixels = read_framebuffer(context);

    for (int row = 0; row < height; ++row) {
        for (int column = 0; column < width; ++column) {
            const std::uint32_t pixel =
                pixels[static_cast<std::size_t>(row) *
                           static_cast<std::size_t>(width) +
                       static_cast<std::size_t>(column)];
            if ((pixel & 0x00ffffffu) != 0u) {
                return row;
            }
        }
    }

    return -1;
}

gfx::Animation held_frame()
{
    gfx::Animation animation;
    animation.name = "dot";
    animation.frames.push_back(0);
    animation.seconds_per_frame = 0.0; // a single held pose
    return animation;
}

} // namespace

TEST_CASE("a negative world x floors to its pixel rather than truncating")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 64, 64, /*fullscreen=*/false);
    const gfx::Atlas atlas = white_pixel(context);
    const gfx::Animation animation = held_frame();

    // The camera is negative so that a negative world x is on screen at all;
    // it shifts every column by the same whole number and so cannot itself
    // change the rounding.
    const int camera = -32;

    // floor(-0.5) == -1, so column 31. Truncation toward zero would give 0 and
    // put this in column 32, on top of the positive case below.
    CHECK(lit_column(context, atlas, animation, -0.5, 0.0, camera, 0) == 31);
    CHECK(lit_column(context, atlas, animation, -0.001, 0.0, camera, 0) == 31);
    CHECK(lit_column(context, atlas, animation, -1.0, 0.0, camera, 0) == 31);
}

TEST_CASE("the pixel column at the world origin is one pixel wide, not two")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 64, 64, /*fullscreen=*/false);
    const gfx::Atlas atlas = white_pixel(context);
    const gfx::Animation animation = held_frame();

    const int camera = -32;

    // This is the case the rounding exists for. Truncation maps the whole open
    // interval (-1, 1) — two pixels of travel — onto one column, so an entity
    // easing left across the origin holds still for twice as long as it does
    // anywhere else and then jumps. Every column must span exactly one unit.
    const int left =
        lit_column(context, atlas, animation, -0.5, 0.0, camera, 0);
    const int right =
        lit_column(context, atlas, animation, 0.5, 0.0, camera, 0);

    CHECK(left == 31);
    CHECK(right == 32);
    CHECK(right - left == 1);

    // And the step is uniform on either side of it, so nothing was traded for
    // a special case at zero.
    CHECK(lit_column(context, atlas, animation, -1.5, 0.0, camera, 0) == 30);
    CHECK(lit_column(context, atlas, animation, 1.5, 0.0, camera, 0) == 33);
}

TEST_CASE("y rounds the same way as x")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 64, 64, /*fullscreen=*/false);
    const gfx::Atlas atlas = white_pixel(context);
    const gfx::Animation animation = held_frame();

    const int camera = -32;

    // The axes are independent code, so a fix applied to one of them only
    // would pass every case above.
    CHECK(lit_row(context, atlas, animation, 0.0, -0.5, 0, camera) == 31);
    CHECK(lit_row(context, atlas, animation, 0.0, 0.5, 0, camera) == 32);
}

TEST_CASE("a whole-pixel position is unaffected by the rounding")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 64, 64, /*fullscreen=*/false);
    const gfx::Atlas atlas = white_pixel(context);
    const gfx::Animation animation = held_frame();

    // Truncation and floor agree on exact integers, positive or negative, so a
    // whole-pixel position lands in the same column under either rule.
    CHECK(lit_column(context, atlas, animation, 10.0, 0.0, 0, 0) == 10);
    CHECK(lit_column(context, atlas, animation, -8.0, 0.0, -32, 0) == 24);
}

//============================================================================
// Draw ordering, in pixels
//============================================================================

namespace
{

// Two one-pixel frames of different colours in one sheet. Two sprites stacked
// on the same pixel then answer "which of these drew last" by colour, which is
// the only place draw()'s ordering is observable from outside it — draw()
// itself reports a count.
gfx::Atlas red_and_blue(gfx::renderer::Context& context)
{
    SDL_Surface* surface =
        SDL_CreateRGBSurfaceWithFormat(0, 2, 1, 32, SDL_PIXELFORMAT_RGBA32);
    REQUIRE(surface != nullptr);

    const SDL_Rect left = {0, 0, 1, 1};
    const SDL_Rect right = {1, 0, 1, 1};
    SDL_FillRect(surface, &left, SDL_MapRGBA(surface->format, 255, 0, 0, 255));
    SDL_FillRect(surface, &right, SDL_MapRGBA(surface->format, 0, 0, 255, 255));

    gfx::renderer::Texture sheet(context, surface);
    SDL_FreeSurface(surface);

    // Both untrimmed, so the destination is the entity position and nothing
    // else — a trim offset here would be a second reason a pixel could move.
    gfx::AtlasFrame red;
    red.id = "red";
    red.source = gfx::renderer::Rect{0, 0, 1, 1};

    gfx::AtlasFrame blue;
    blue.id = "blue";
    blue.source = gfx::renderer::Rect{1, 0, 1, 1};

    gfx::Atlas::Frames frames;
    frames.push_back(red);
    frames.push_back(blue);

    return gfx::Atlas(std::move(sheet), std::move(frames));
}

// A single held pose on one atlas frame.
gfx::Animation pose(gfx::Atlas::Index frame)
{
    gfx::Animation animation;
    animation.name = "pose";
    animation.frames.push_back(frame);
    animation.seconds_per_frame = 0.0;
    return animation;
}

game::Entity dot(const gfx::Atlas& atlas, const gfx::Animation& animation,
                 double x, double y, int layer)
{
    game::Entity entity = at(x, y, layer);
    entity.sprite = gfx::AnimatedSprite(atlas, animation);
    return entity;
}

// The colour at (column, row) with the alpha dropped, for comparison against
// one of the two constants below.
std::uint32_t colour_at(gfx::renderer::Context& context, int column, int row)
{
    const std::vector<std::uint32_t> pixels = read_framebuffer(context);
    const std::size_t offset = static_cast<std::size_t>(row) *
                                   static_cast<std::size_t>(context.width()) +
                               static_cast<std::size_t>(column);

    return pixels[offset] & 0x00ffffffu;
}

const std::uint32_t red_pixel = 0x00ff0000u;
const std::uint32_t blue_pixel = 0x000000ffu;

} // namespace

TEST_CASE("the higher layer lands on top at the same position")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 64, 64, /*fullscreen=*/false);
    const gfx::Atlas atlas = red_and_blue(context);
    const gfx::Animation show_red = pose(0);
    const gfx::Animation show_blue = pose(1);

    game::Entities entities;

    // Added front-first, so insertion order and draw order disagree and the
    // surviving colour cannot be produced by skipping the sort.
    entities.add(dot(atlas, show_blue, 10.0, 10.0, /*layer=*/1));
    entities.add(dot(atlas, show_red, 10.0, 10.0, /*layer=*/0));

    context.clear(gfx::renderer::Color{0, 0, 0, 255});
    REQUIRE(entities.draw(context, 0, 0) == 2);

    CHECK(colour_at(context, 10, 10) == blue_pixel);

    // The same two layers with the colours exchanged, so what survives is
    // pinned to the layer rather than to which frame of the sheet it came from.
    game::Entities exchanged;
    exchanged.add(dot(atlas, show_red, 10.0, 10.0, /*layer=*/1));
    exchanged.add(dot(atlas, show_blue, 10.0, 10.0, /*layer=*/0));

    context.clear(gfx::renderer::Color{0, 0, 0, 255});
    REQUIRE(exchanged.draw(context, 0, 0) == 2);

    CHECK(colour_at(context, 10, 10) == red_pixel);
}

TEST_CASE("within one layer the lower entity lands on top")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 64, 64, /*fullscreen=*/false);
    const gfx::Atlas atlas = red_and_blue(context);
    const gfx::Animation show_red = pose(0);
    const gfx::Animation show_blue = pose(1);

    game::Entities entities;

    // The y gap is sub-pixel on purpose: both sprites have to reach the same
    // pixel for the pixel to arbitrate between them, and any whole-pixel gap
    // would put them in different rows and settle nothing. 10.0 and 10.4 share
    // a row under floor and under any other rounding rule as well, so this does
    // not quietly depend on which one to_pixel uses.
    entities.add(dot(atlas, show_blue, 10.0, 10.4, /*layer=*/0));
    entities.add(dot(atlas, show_red, 10.0, 10.0, /*layer=*/0));

    context.clear(gfx::renderer::Color{0, 0, 0, 255});
    REQUIRE(entities.draw(context, 0, 0) == 2);

    CHECK(colour_at(context, 10, 10) == blue_pixel);

    // Colours exchanged between the two heights, for the same reason as above.
    game::Entities exchanged;
    exchanged.add(dot(atlas, show_red, 10.0, 10.4, /*layer=*/0));
    exchanged.add(dot(atlas, show_blue, 10.0, 10.0, /*layer=*/0));

    context.clear(gfx::renderer::Color{0, 0, 0, 255});
    REQUIRE(exchanged.draw(context, 0, 0) == 2);

    CHECK(colour_at(context, 10, 10) == red_pixel);
}
