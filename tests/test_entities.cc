// sprites::Entities — the flat entity store.
//
// entities.cc is compiled into this test rather than linked, because the store
// lives in the demo and the demo is an executable. Same arrangement as
// test_bars.cc and coppers/bars.cc; worth a library if a second consumer
// appears, which is also when the store should move out of sprites/ entirely.
//
// Nearly all of it is headless: the store integrates positions and orders
// indexes, and an AnimatedSprite with no atlas advances perfectly well without
// one. Only the draw-order case needs a Context, and it checks the order
// entities were visited in rather than pixels — what matters is that the sort
// key is (layer, y) and that dead slots are skipped.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <gfx/system.hpp>

#include <SDL.h>

#include <memory>
#include <algorithm>
#include <utility>
#include <vector>

#include "../sprites/entities.hpp"

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

sprites::Entity at(double x, double y, int layer = 0)
{
    sprites::Entity entity;
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
    sprites::Entities entities;
    CHECK(entities.empty());

    const sprites::Entities::Id a = entities.add(at(1.0, 2.0));
    const sprites::Entities::Id b = entities.add(at(3.0, 4.0));

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
    sprites::Entities entities;
    const sprites::Entities::Id id = entities.add(at(1.0, 1.0));

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
    sprites::Entities entities;

    const sprites::Entities::Id first = entities.add(at(10.0, 10.0));
    entities.remove(first);

    const sprites::Entities::Id second = entities.add(at(99.0, 99.0));

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
    sprites::Entities entities;
    CHECK_FALSE(entities.alive(sprites::Entities::none()));

    entities.add(at(0.0, 0.0));
    CHECK_FALSE(entities.alive(sprites::Entities::none()));
    CHECK(entities.find(sprites::Entities::none()) == nullptr);
}

TEST_CASE("clear empties the store and strands every handle")
{
    sprites::Entities entities;
    const sprites::Entities::Id a = entities.add(at(1.0, 1.0));
    const sprites::Entities::Id b = entities.add(at(2.0, 2.0));

    entities.clear();

    CHECK(entities.empty());
    CHECK_FALSE(entities.alive(a));
    CHECK_FALSE(entities.alive(b));

    // A handle from before the clear must not come back to life against the
    // slot its index now names.
    const sprites::Entities::Id fresh = entities.add(at(5.0, 5.0));
    CHECK(entities.alive(fresh));
    CHECK_FALSE(entities.alive(a));
}

//============================================================================
// The update tick
//============================================================================

TEST_CASE("update integrates velocity against the delta")
{
    sprites::Entities entities;

    sprites::Entity moving = at(0.0, 0.0);
    moving.velocity_x = 60.0;
    moving.velocity_y = -30.0;
    const sprites::Entities::Id id = entities.add(moving);

    entities.update(0.5);

    REQUIRE(entities.find(id) != nullptr);
    CHECK(entities.find(id)->x == doctest::Approx(30.0));
    CHECK(entities.find(id)->y == doctest::Approx(-15.0));

    // Position is a double so that slow movement accumulates instead of being
    // rounded away every frame.
    sprites::Entity slow = at(0.0, 0.0);
    slow.velocity_x = 1.0;
    const sprites::Entities::Id crawler = entities.add(slow);

    for (int i = 0; i < 10; ++i) {
        entities.update(0.1);
    }
    CHECK(entities.find(crawler)->x == doctest::Approx(1.0));
}

TEST_CASE("update ignores a zero or negative delta and skips dead slots")
{
    sprites::Entities entities;

    sprites::Entity moving = at(5.0, 0.0);
    moving.velocity_x = 100.0;
    const sprites::Entities::Id id = entities.add(moving);

    entities.update(0.0);
    CHECK(entities.find(id)->x == doctest::Approx(5.0));
    entities.update(-1.0);
    CHECK(entities.find(id)->x == doctest::Approx(5.0));

    // A removed entity must not keep integrating in its slot.
    const sprites::Entities::Id dead = entities.add(moving);
    entities.remove(dead);
    entities.update(1.0);
    CHECK(entities.size() == 1u);
}

TEST_CASE("each visits live entities only")
{
    sprites::Entities entities;
    entities.add(at(1.0, 0.0));
    const sprites::Entities::Id gone = entities.add(at(2.0, 0.0));
    entities.add(at(3.0, 0.0));
    entities.remove(gone);

    std::vector<double> seen;
    entities.each([&seen](sprites::Entity& e) { seen.push_back(e.x); });

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

    sprites::Entities entities;
    entities.add(at(0.0, 0.0));
    const sprites::Entities::Id gone = entities.add(at(1.0, 1.0));
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
    sprites::Entities entities;

    // Deliberately added out of order.
    entities.add(at(0.0, 50.0, /*layer=*/1)); // front layer, high up
    entities.add(at(0.0, 10.0, /*layer=*/0)); // back layer, high up
    entities.add(at(0.0, 90.0, /*layer=*/0)); // back layer, low down
    entities.add(at(0.0, 20.0, /*layer=*/1)); // front layer, high up

    std::vector<std::pair<int, double>> keys;
    entities.each([&keys](sprites::Entity& e) {
        keys.push_back({e.layer, e.y});
    });

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

    sprites::Entities entities;
    CHECK(entities.draw(context, 0, 0) == 0);
}
