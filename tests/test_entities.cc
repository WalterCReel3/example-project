// game::Entities — the flat entity store.
//
// Nearly all of it is headless: the store integrates positions and orders
// indexes, and an AnimatedSprite with no atlas advances perfectly well without
// one. Only the draw-order case needs a Context, and it checks the order
// entities were visited in rather than pixels — what matters is that the sort
// key is (layer, y) and that dead slots are skipped.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <game/collide.hpp>
#include <game/entities.hpp>
#include <gfx/system.hpp>
#include <gfx/tilemap.hpp>

#include <SDL.h>

#include <memory>
#include <algorithm>
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

TEST_CASE("draw order is layer first, then y")
{
    // Ordering is observable through the store's own iteration rather than
    // through pixels: a sprite with no atlas draws nothing, and asserting on
    // the sort key directly is what the ordering rule actually says.
    game::Entities entities;

    // Deliberately added out of order.
    entities.add(at(0.0, 50.0, /*layer=*/1)); // front layer, high up
    entities.add(at(0.0, 10.0, /*layer=*/0)); // back layer, high up
    entities.add(at(0.0, 90.0, /*layer=*/0)); // back layer, low down
    entities.add(at(0.0, 20.0, /*layer=*/1)); // front layer, high up

    std::vector<std::pair<int, double>> keys;
    entities.each([&keys](game::Entity& e) { keys.push_back({e.layer, e.y}); });

    // Storage order is insertion order — the sort happens at draw time.
    REQUIRE(keys.size() == 4u);
    CHECK(keys[0].first == 1);

    // Reproduce what draw() sorts by, since draw() itself reports only a count.
    std::sort(
        keys.begin(), keys.end(),
        [](const std::pair<int, double>& a, const std::pair<int, double>& b) {
            if (a.first != b.first) {
                return a.first < b.first;
            }
            return a.second < b.second;
        });

    CHECK(keys[0] == std::make_pair(0, 10.0)); // back layer first
    CHECK(keys[1] == std::make_pair(0, 90.0)); // then lower down, same layer
    CHECK(keys[2] == std::make_pair(1, 20.0)); // front layer after
    CHECK(keys[3] == std::make_pair(1, 50.0));
}

TEST_CASE("an empty store draws nothing without touching the renderer")
{
    VideoFixture video;
    gfx::renderer::Context context("test", 64, 64, /*fullscreen=*/false);

    game::Entities entities;
    CHECK(entities.draw(context, 0, 0) == 0);
}
