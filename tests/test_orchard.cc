// orchard's motion and its cast, against the real data/orchard.tmx.
//
// The level's branch platforms climb in 48 px steps and its ground profile
// steps up 32 px in three places. Those numbers were written before a jump
// existed, so "the map loads" and "the map can be played" are separate claims
// and only the first of them was previously checked. This file checks the
// second: it drives orchard::Player across the real map from the `player` spawn
// to the `goal` spawn and fails if it cannot get there.
//
// The second half asks the other question the map can be wrong about: whether
// every spawn in it means something to the demo, and whether the three animals
// stay on the level once they are let go. Both are claims about the shipped
// .tmx as much as about the code, which is why they belong beside the crossing
// rather than in a fixture.
//
// The third half is the rules, which are where a demo goes wrong while still
// drawing a perfectly good frame. A score that double-counts, a bump that fires
// sixty times a second, a goal that never fires: none of those is visible in a
// screenshot and all of them are arithmetic. The crossing is then asked a
// second time with the rules switched on — contact costs a pickup and sits the
// player down, so "the geometry is passable" and "it is passable once contact
// means something" are two claims and only the first was ever checked here.
//
// Headless. orchard/player.cc, orchard/actors.cc and orchard/rules.cc are free
// of gfx and rig, and loaders::load_tmx needs no video subsystem, so all of it
// runs without a window. They are compiled in rather than linked because
// orchard is an executable and its sources are not a library.

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <game/collide.hpp>
#include <game/level.hpp>
#include <loaders/tmx.hpp>
#include <rig/assets.hpp>

#include "../orchard/actors.hpp"
#include "../orchard/cues.hpp"
#include "../orchard/player.hpp"
#include "../orchard/rules.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

namespace
{

// The frame the demo runs its screenshot at, and what the whole file steps at.
const double delta = 1.0 / 60.0;

const loaders::TmxMap& parsed()
{
    static const loaders::TmxMap map =
        loaders::load_tmx(rig::asset_path("orchard.tmx"));
    return map;
}

const game::Level& level()
{
    static const game::Level built(parsed().layers, parsed().object_layers,
                                   parsed().width, parsed().height,
                                   parsed().tile_width, parsed().tile_height,
                                   "orchard.tmx");
    return built;
}

// The first spawn of a type, which is the rule orchard itself uses.
const gfx::MapObject& spawn_of(const std::string& type)
{
    for (const gfx::MapObject& object : level().spawns()) {
        if (object.type == type) {
            return object;
        }
    }
    REQUIRE_MESSAGE(false, "orchard.tmx has no " << type << " spawn");
    return level().spawns().front();
}

game::Aabb rect_of(const gfx::MapObject& object)
{
    return game::Aabb{object.x, object.y, object.width, object.height};
}

// Holds Jump down for a span of frames once started, because a jump's height is
// trimmed by releasing the button during the ascent and a one-frame press is a
// deliberately short hop.
class JumpButton
{
public:
    void press() { _frames = 20; }

    void apply(orchard::Intent& intent)
    {
        intent.jump_pressed = _frames == 20;
        intent.jump_held = _frames > 0;
        if (_frames > 0) {
            --_frames;
        }
    }

private:
    int _frames = 0;
};

// Walks right and jumps at anything that blocks it. Deliberately the dumbest
// policy that could work: if this crosses the level then a player who can see
// the screen certainly can, and if it does not, the report says where it stuck
// rather than blaming the driver.
struct Crossing {
    bool reached = false;
    int frames = 0;
    double furthest_x = 0.0;
    double furthest_y = 0.0;

    // What orchard::CueEdges made of the same run. Counted here rather than in
    // a case of their own because the interesting number is how a cue behaves
    // over a real crossing — 758 frames of walking, jumping and landing on the
    // shipped map — and not how it behaves over a contrived one.
    int jump_cues = 0;
    int land_cues = 0;
};

Crossing walk_to_goal(int frame_limit)
{
    const gfx::MapObject& start = spawn_of("player");
    const game::Aabb goal = rect_of(spawn_of("goal"));

    orchard::Player player;
    game::Aabb box = rect_of(start);

    const orchard::Motion motion;
    JumpButton jump;
    orchard::CueEdges edges;
    Crossing crossing;
    crossing.furthest_x = box.x;
    crossing.furthest_y = box.y;

    for (int frame = 0; frame < frame_limit; ++frame) {
        orchard::Intent intent;
        intent.right = true;

        // The only rule: something is in the way and there is ground to push
        // off. Everything else the level asks for falls out of gravity.
        if (player.on_ground() && player.bumped()) {
            jump.press();
        }
        jump.apply(intent);

        box = player.step(level().collision(), box, intent, motion, delta);

        // Exactly where orchard/demo.cc reads them, and off the same two
        // accessors, so this counts what the demo would have played.
        const orchard::CueEdges::Fired fired =
            edges.observe(player.on_ground(), player.velocity_y());
        crossing.jump_cues += fired.jump ? 1 : 0;
        crossing.land_cues += fired.land ? 1 : 0;

        if (box.x > crossing.furthest_x) {
            crossing.furthest_x = box.x;
            crossing.furthest_y = box.y;
        }

        if (game::overlaps(box, goal)) {
            crossing.reached = true;
            crossing.frames = frame + 1;
            break;
        }
    }

    return crossing;
}

// Drops the player onto a platform and lets it settle, so a jump measured from
// there starts from rest on solid ground rather than mid-fall. Answers the box
// it came to rest in, which is what the caller goes on to step.
game::Aabb standing_at(orchard::Player& player, double x, double feet)
{
    game::Aabb box{x, feet - 28.0, 16.0, 28.0};

    const orchard::Motion motion;
    for (int frame = 0; frame < 10; ++frame) {
        box = player.step(level().collision(), box, orchard::Intent(), motion,
                          delta);
    }
    return box;
}

// Steps the rules until a dizzy spell has passed, so a case about what a bump
// costs does not also have to be a case about how long one lasts.
void recover(orchard::Rules& rules)
{
    for (int frame = 0; frame < 10 * 60 && !rules.accepts_intent(); ++frame) {
        rules.advance(delta);
    }
}

// Frames a timer tuned to `seconds` takes to expire when it is counted down one
// fixed delta at a time.
//
// A RANGE AND NOT A NUMBER, deliberately. Summing 1/60 for 2.5 s lands on
// 2.4999999999999996, so the last frame is one short and the timer runs a
// sixtieth of a second long. That is not a defect to chase: the same binary
// reproduces it exactly, which is all decision 7 asks for, and a case that
// pinned the exact count would be pinning the rounding rather than the tuning.
int frames_for(double seconds)
{
    return static_cast<int>(seconds / delta);
}

} // namespace

TEST_CASE("the map this demo ships is the one the demo reads")
{
    // Guards the arithmetic in every case below, all of which is written in
    // 16 px cells against a 96x40 map.
    CHECK(level().tile_width() == 16);
    CHECK(level().tile_height() == 16);
    CHECK(level().width() == 96);
    CHECK(level().height() == 40);
    CHECK(level().pixel_width() == 1536);
    CHECK(level().pixel_height() == 640);
}

TEST_CASE("the player spawn rests on the ground rather than in it or above it")
{
    const gfx::MapObject& start = spawn_of("player");
    const game::Aabb box = rect_of(start);

    CHECK_FALSE(game::overlaps_solid(level().collision(), box));

    // One pixel lower is solid, which is what "resting on" means.
    game::Aabb sunk = box;
    sunk.y += 1.0;
    CHECK(game::overlaps_solid(level().collision(), sunk));
}

TEST_CASE("a full jump clears the 48 px the branch platforms step in")
{
    // Flat ground at the spawn, so the rise is measured against a floor and not
    // against a slope.
    const gfx::MapObject& start = spawn_of("player");
    orchard::Player player;
    game::Aabb box = standing_at(player, start.x, start.y + start.height);

    const double floor_y = box.y;
    const orchard::Motion motion;

    JumpButton jump;
    jump.press();

    double peak = floor_y;
    for (int frame = 0; frame < 120; ++frame) {
        orchard::Intent intent;
        jump.apply(intent);
        box = player.step(level().collision(), box, intent, motion, delta);
        if (box.y < peak) {
            peak = box.y;
        }
        if (frame > 2 && player.on_ground()) {
            break;
        }
    }

    const double rise = floor_y - peak;
    INFO("peak rise " << rise << " px");

    // The level's own unit of climb, and the taller of the two things the map
    // asks for. The margin matters as much as the number: a jump that cleared
    // 48 px exactly would need a frame-perfect launch to land on anything.
    CHECK(rise > 48.0);
    CHECK(rise > 60.0);

    // And it comes back down.
    CHECK(player.on_ground());
    CHECK(box.y == doctest::Approx(floor_y));
}

TEST_CASE("a short press is a lower hop than a held one")
{
    const gfx::MapObject& start = spawn_of("player");
    const orchard::Motion motion;

    double rise[2] = {0.0, 0.0};
    for (int held = 0; held < 2; ++held) {
        orchard::Player player;
        game::Aabb box = standing_at(player, start.x, start.y + start.height);
        const double floor_y = box.y;
        double peak = floor_y;

        for (int frame = 0; frame < 120; ++frame) {
            orchard::Intent intent;
            intent.jump_pressed = frame == 0;
            intent.jump_held = held == 1 ? frame < 20 : frame == 0;
            box = player.step(level().collision(), box, intent, motion, delta);
            peak = box.y < peak ? box.y : peak;
            if (frame > 2 && player.on_ground()) {
                break;
            }
        }
        rise[held] = floor_y - peak;
    }

    INFO("tap " << rise[0] << " px, held " << rise[1] << " px");
    CHECK(rise[0] < rise[1]);

    // A tap still has to be worth pressing. The ground profile steps up a
    // single 16 px cell in two places, so a minimum hop that cannot mount one
    // reads as the button not having worked.
    CHECK(rise[0] > 16.0);
}

TEST_CASE("a press on solid ground jumps whatever the credits are tuned to")
{
    // `coyote_time` and `jump_buffer` are forgiveness, so zeroing one is the
    // obvious way to ask for none of it. That must not be the same thing as
    // asking for no jump: the frame a player is standing on the ground and
    // presses Jump is not a case either credit has any say over.
    const gfx::MapObject& start = spawn_of("player");
    const orchard::Motion shipped;

    struct Tuning {
        const char* name;
        double coyote;
        double buffer;
    };

    const Tuning tunings[] = {
        {"shipped", shipped.coyote_time, shipped.jump_buffer},
        {"no coyote", 0.0, shipped.jump_buffer},
        {"no buffer", shipped.coyote_time, 0.0},
        {"neither", 0.0, 0.0},
    };

    for (const Tuning& tuning : tunings) {
        orchard::Motion motion;
        motion.coyote_time = tuning.coyote;
        motion.jump_buffer = tuning.buffer;

        orchard::Player player;
        game::Aabb box = standing_at(player, start.x, start.y + start.height);
        REQUIRE(player.on_ground());
        const double floor_y = box.y;

        orchard::Intent intent;
        intent.jump_pressed = true;
        intent.jump_held = true;
        box = player.step(level().collision(), box, intent, motion, delta);

        INFO(tuning.name);
        CHECK(player.velocity_y() < 0.0);
        CHECK(box.y < floor_y);
    }
}

TEST_CASE("coyote_time at zero costs the grace after a ledge, not the jump")
{
    // The right-hand end of the middle climb's first platform: columns 46-48 at
    // row 28. Walking off it is the case coyote_time exists for, so it is also
    // the case that tells a removed grace apart from a broken jump.
    const double ledge_feet = 28.0 * 16.0;
    const double ledge_x = 48.0 * 16.0;
    const orchard::Motion shipped;

    for (int forgiving = 0; forgiving < 2; ++forgiving) {
        orchard::Motion motion;
        motion.coyote_time = forgiving == 1 ? shipped.coyote_time : 0.0;

        orchard::Player player;
        game::Aabb box = standing_at(player, ledge_x, ledge_feet);
        REQUIRE(player.on_ground());

        // Walk until the platform runs out. The step that leaves it is the last
        // one with ground under the box, so the press below is one frame late —
        // exactly the mistake the credit forgives.
        int walked = 0;
        while (player.on_ground() && walked < 60) {
            orchard::Intent intent;
            intent.right = true;
            box = player.step(level().collision(), box, intent, motion, delta);
            ++walked;
        }
        REQUIRE(walked < 60);

        orchard::Intent intent;
        intent.right = true;
        intent.jump_pressed = true;
        intent.jump_held = true;
        box = player.step(level().collision(), box, intent, motion, delta);

        INFO("coyote_time " << motion.coyote_time << ", walked " << walked
                            << " frames");
        CHECK((player.velocity_y() < 0.0) == (forgiving == 1));
    }
}

TEST_CASE("jump_buffer at zero costs the early press, not the jump")
{
    const gfx::MapObject& start = spawn_of("player");
    const orchard::Motion shipped;

    // Dropped from clear air over the flat ground at the spawn, so which frame
    // it lands on is a function of gravity alone and can be measured once.
    game::Aabb dropped = rect_of(start);
    dropped.y -= 48.0;

    int landing = -1;
    {
        orchard::Player player;
        game::Aabb box = dropped;
        for (int frame = 0; frame < 120 && landing < 0; ++frame) {
            box = player.step(level().collision(), box, orchard::Intent(),
                              shipped, delta);
            if (player.on_ground()) {
                landing = frame;
            }
        }
    }
    REQUIRE(landing > 2);

    // Two frames early is 0.033 s: inside the shipped 0.10 s buffer and outside
    // a zeroed one. The jump fires on the frame after the landing frame, not on
    // it — `_on_ground` is what the landing step computes, so the first step
    // that can see it is the next.
    for (int forgiving = 0; forgiving < 2; ++forgiving) {
        orchard::Motion motion;
        motion.jump_buffer = forgiving == 1 ? shipped.jump_buffer : 0.0;

        orchard::Player player;
        game::Aabb box = dropped;
        for (int frame = 0; frame <= landing + 1; ++frame) {
            orchard::Intent intent;
            intent.jump_pressed = frame == landing - 2;
            intent.jump_held = frame >= landing - 2;
            box = player.step(level().collision(), box, intent, motion, delta);
        }

        INFO("jump_buffer " << motion.jump_buffer << ", landed on frame "
                            << landing);
        CHECK((player.velocity_y() < 0.0) == (forgiving == 1));
    }
}

TEST_CASE("a branch-to-branch hop crosses 32 px of gap while 48 px of rise")
{
    // The middle climb's first two platforms: columns 46-48 at row 28, then
    // columns 51-53 at row 25. Three tiles wide, two empty columns between
    // them, three rows up. This is the level's repeated unit and the hop the
    // BRANCHES table is really asserting is possible.
    const double launch_feet = 28.0 * 16.0;  // row 28's top edge
    const double landing_feet = 25.0 * 16.0; // row 25's top edge

    // Standing at the right-hand edge of the near platform, which is where a
    // player jumping for the far one stands.
    orchard::Player player;
    game::Aabb box = standing_at(player, 48.0 * 16.0 - 16.0, launch_feet);
    REQUIRE(player.on_ground());
    REQUIRE(box.y + 28.0 == doctest::Approx(launch_feet));

    const orchard::Motion motion;
    JumpButton jump;
    jump.press();

    bool landed = false;
    for (int frame = 0; frame < 120; ++frame) {
        orchard::Intent intent;
        intent.right = true;
        jump.apply(intent);
        box = player.step(level().collision(), box, intent, motion, delta);

        if (frame > 2 && player.on_ground()) {
            landed = true;
            break;
        }
    }

    REQUIRE(landed);
    INFO("landed at x " << box.x << " feet " << box.y + 28.0);

    // On the far platform, not back on the near one and not on the ground
    // 80 px below it.
    CHECK(box.y + 28.0 == doctest::Approx(landing_feet));
    CHECK(box.x > 51.0 * 16.0 - 16.0);
    CHECK(box.x < 54.0 * 16.0);
}

TEST_CASE("the level can be crossed from the player spawn to the goal")
{
    // 90 seconds of frames. The walk is about 12 px per frame over 1424 px of
    // level, so a run that has not arrived by here is stuck rather than slow.
    const Crossing crossing = walk_to_goal(90 * 60);

    INFO("furthest x " << crossing.furthest_x << ", y " << crossing.furthest_y
                       << " of a 1536x640 level");
    REQUIRE(crossing.reached);

    // Recorded rather than bounded tightly: the number is here so a change that
    // makes the crossing twice as long is visible in the diff.
    INFO("crossed in " << crossing.frames << " frames");
    CHECK(crossing.frames < 90 * 60);
}

TEST_CASE("standing on the ground is not landing on it, however long it lasts")
{
    // The whole reason orchard::CueEdges exists. Player::on_ground() is a
    // per-frame STATE: a player stood still reads true on every one of these
    // 300 frames. A `land` cue taken from it directly would restart the same
    // chunk five seconds running — inaudible as a defect on a dev box with 16
    // voices and a forgiving buffer, and a starved mixer on a handheld with 8
    // and 2048 samples. This case is the one that would have caught it.
    orchard::Player player;
    game::Aabb box = standing_at(player, 4.0 * 16.0, 33.0 * 16.0);
    REQUIRE(player.on_ground());

    const orchard::Motion motion;
    orchard::CueEdges edges;
    int jumps = 0;
    int lands = 0;

    for (int frame = 0; frame < 300; ++frame) {
        box = player.step(level().collision(), box, orchard::Intent(), motion,
                          delta);

        const orchard::CueEdges::Fired fired =
            edges.observe(player.on_ground(), player.velocity_y());
        jumps += fired.jump ? 1 : 0;
        lands += fired.land ? 1 : 0;
    }

    REQUIRE(player.on_ground());
    INFO("after 300 frames standing: " << jumps << " jump, " << lands
                                       << " land");

    // Not "few". None: the player never left the ground, so nothing happened.
    CHECK(lands == 0);
    CHECK(jumps == 0);
}

TEST_CASE("one jump is one jump cue and one land cue")
{
    orchard::Player player;
    game::Aabb box = standing_at(player, 4.0 * 16.0, 33.0 * 16.0);
    REQUIRE(player.on_ground());

    const orchard::Motion motion;
    orchard::CueEdges edges;
    JumpButton jump;
    jump.press();

    int jumps = 0;
    int lands = 0;

    for (int frame = 0; frame < 180; ++frame) {
        orchard::Intent intent;
        jump.apply(intent);
        box = player.step(level().collision(), box, intent, motion, delta);

        const orchard::CueEdges::Fired fired =
            edges.observe(player.on_ground(), player.velocity_y());
        jumps += fired.jump ? 1 : 0;
        lands += fired.land ? 1 : 0;
    }

    // 180 frames is three seconds, long enough that the arc is over and the
    // player has been resting on the floor for most of it. The cue does not
    // repeat while it rests.
    REQUIRE(player.on_ground());
    CHECK(jumps == 1);
    CHECK(lands == 1);
}

TEST_CASE("a coyote-time jump fires the jump cue, though the ground had gone")
{
    // WHY THE EDGE IS TAKEN FROM THE VELOCITY AND NOT FROM on_ground(). The
    // obvious detector — "was on the ground last frame and is not now" — reads
    // the wrong thing: coyote_time exists precisely so that a jump can fire
    // several frames AFTER the ground went away, and by then that transition
    // has already been spent on the walk-off. The velocity turning upward is
    // the launch itself, and nothing else in Player::step() can produce it.
    //
    // Walks off the right-hand edge of the branch at columns 46-48, row 28 —
    // the same ledge the branch-to-branch case above launches from.
    orchard::Player player;
    game::Aabb box = standing_at(player, 48.0 * 16.0 - 16.0, 28.0 * 16.0);
    REQUIRE(player.on_ground());

    const orchard::Motion motion;
    const double ledge_y = box.y;
    orchard::CueEdges edges;

    int jumps = 0;
    bool left_the_ground = false;
    bool pressed = false;
    int frames_airborne = 0;
    int airborne_at_press = 0;

    for (int frame = 0; frame < 120; ++frame) {
        orchard::Intent intent;
        intent.right = true;

        // One frame of grace spent walking before the press, so the press is
        // genuinely paid for out of the credit rather than out of the ground.
        if (left_the_ground && !pressed && frames_airborne == 1) {
            intent.jump_pressed = true;
            intent.jump_held = true;
            pressed = true;
            airborne_at_press = frames_airborne;
        }

        box = player.step(level().collision(), box, intent, motion, delta);

        const orchard::CueEdges::Fired fired =
            edges.observe(player.on_ground(), player.velocity_y());
        jumps += fired.jump ? 1 : 0;

        if (!player.on_ground()) {
            left_the_ground = true;
            ++frames_airborne;
        }
        if (pressed) {
            break;
        }
    }

    INFO("pressed after " << airborne_at_press << " airborne frames, credit "
                          << motion.coyote_time << " s");
    REQUIRE(pressed);

    // The press bought a jump, and the jump bought a cue.
    CHECK(player.velocity_y() < 0.0);
    CHECK(box.y < ledge_y);
    CHECK(jumps == 1);
}

TEST_CASE("crossing the level fires a cue per jump and per landing, not per "
          "frame of either")
{
    const Crossing crossing = walk_to_goal(90 * 60);
    REQUIRE(crossing.reached);

    INFO("over " << crossing.frames << " frames: " << crossing.jump_cues
                 << " jump cues, " << crossing.land_cues << " land cues");

    // The autopilot only jumps at what blocks it, so a real crossing makes a
    // handful of each. The claim is the ratio, not the count: two orders of
    // magnitude below the frame count is what "one-shot" means, and a detector
    // that had reverted to reading the state would land within a few of 758.
    CHECK(crossing.jump_cues > 0);
    CHECK(crossing.land_cues > 0);
    CHECK(crossing.jump_cues < crossing.frames / 20);
    CHECK(crossing.land_cues < crossing.frames / 20);

    // Every jump comes down somewhere, and the walk also steps off ledges it
    // never jumped from — so landings are at least as many as launches. Fewer
    // would mean a landing had been swallowed.
    CHECK(crossing.land_cues >= crossing.jump_cues);
}

TEST_CASE("every spawn in the shipped map is something the demo places")
{
    int counted[static_cast<int>(orchard::Behaviour::Count)] = {0};
    int unknown = 0;

    for (const gfx::MapObject& object : level().spawns()) {
        orchard::Behaviour behaviour = orchard::Behaviour::Player;
        if (orchard::behaviour_for(object.type, behaviour)) {
            ++counted[static_cast<int>(behaviour)];
        } else {
            INFO("no rule for spawn type \"" << object.type << "\"");
            CHECK(false);
            ++unknown;
        }
    }

    CHECK(unknown == 0);

    // The map's own census, spelled out rather than summed. A spawn added to
    // orchard.tmx is meant to fail here until the code that places it exists:
    // "23 spawns loaded" is what the level log already says, and it says
    // nothing about whether any of them became an entity.
    CHECK(counted[static_cast<int>(orchard::Behaviour::Player)] == 1);
    CHECK(counted[static_cast<int>(orchard::Behaviour::Opossum)] == 1);
    CHECK(counted[static_cast<int>(orchard::Behaviour::Frog)] == 1);
    CHECK(counted[static_cast<int>(orchard::Behaviour::Eagle)] == 1);
    CHECK(counted[static_cast<int>(orchard::Behaviour::Cherry)] == 15);
    CHECK(counted[static_cast<int>(orchard::Behaviour::Gem)] == 3);
    CHECK(counted[static_cast<int>(orchard::Behaviour::Goal)] == 1);
    CHECK(level().spawns().size() == 23);
}

TEST_CASE("every spawn starts clear of the collision layer")
{
    // The rectangle each entity's world_box() starts at: the demo places a
    // spawn by putting its box exactly here, whatever the sprite around it
    // does. An actor spawned inside a wall is the one starting state collision
    // deliberately does not rescue — see the note on slide().
    for (const gfx::MapObject& object : level().spawns()) {
        INFO(object.type << " at (" << object.x << ", " << object.y << ")");
        CHECK_FALSE(game::overlaps_solid(level().collision(), rect_of(object)));
    }
}

TEST_CASE("the two walking animals spawn resting on ground, not in mid-air")
{
    // Same question the player spawn case asks, for the two that walk. The
    // eagle is excluded on purpose: it is airborne and never touches the grid.
    const char* const walkers[] = {"opossum", "frog"};

    for (const char* type : walkers) {
        game::Aabb sunk = rect_of(spawn_of(type));
        sunk.y += 1.0;

        INFO(type << " spawn");
        CHECK(game::overlaps_solid(level().collision(), sunk));
    }
}

TEST_CASE("the animals patrol without leaving the level or entering the walls")
{
    const orchard::Prowl prowl;

    struct Patrol {
        const char* type;
        orchard::Behaviour behaviour;
        double span;
        bool grounded; // whether the grid is meant to stop it at all
    };

    const Patrol patrols[] = {
        {"opossum", orchard::Behaviour::Opossum, prowl.walk_span, true},
        {"frog", orchard::Behaviour::Frog, prowl.walk_span, true},
        {"eagle", orchard::Behaviour::Eagle, prowl.fly_span, false},
    };

    for (const Patrol& patrol : patrols) {
        const gfx::MapObject& spawn = spawn_of(patrol.type);
        orchard::Animal animal =
            orchard::animal_at(patrol.behaviour, spawn.x, prowl);

        game::Aabb box = rect_of(spawn);
        double lowest_x = box.x;
        double highest_x = box.x;
        double lowest_y = box.y;
        int turns = 0;
        int airborne = 0;
        int buried = 0;

        // Thirty seconds, which is several times the longest leash at the
        // slowest speed here — long enough that a patrol that walks off its
        // platform has arrived somewhere it should not be.
        for (int frame = 0; frame < 30 * 60; ++frame) {
            const int before = animal.direction;
            box = orchard::step_animal(animal, level().collision(), box, prowl,
                                       delta);

            turns += animal.direction != before ? 1 : 0;
            airborne += animal.airborne ? 1 : 0;
            lowest_x = std::min(lowest_x, box.x);
            highest_x = std::max(highest_x, box.x);
            lowest_y = std::max(lowest_y, box.y);

            if (patrol.grounded &&
                game::overlaps_solid(level().collision(), box)) {
                ++buried;
            }
        }

        INFO(std::string(patrol.type)
             << ": x " << lowest_x << ".." << highest_x << ", " << turns
             << " turns, lowest y " << lowest_y);

        // A patrol that never turns is one walking into a wall forever or off
        // the end of the map, and both look like a working animal in a single
        // screenshot.
        CHECK(turns > 0);

        // The leash, plus a frame of overshoot at the speed each moves. The
        // turn happens after the step that crossed the limit, by design: it is
        // the next step that comes back.
        CHECK(lowest_x > spawn.x - patrol.span - 16.0);
        CHECK(highest_x < spawn.x + patrol.span + 16.0);

        // On the map rather than under it. A walker that fell through the
        // world would satisfy every check above.
        CHECK(lowest_y + box.height < level().pixel_height());
        CHECK(buried == 0);
    }
}

TEST_CASE("the frog hops rather than sliding, and lands again")
{
    const orchard::Prowl prowl;
    const gfx::MapObject& spawn = spawn_of("frog");

    orchard::Animal frog =
        orchard::animal_at(orchard::Behaviour::Frog, spawn.x, prowl);
    game::Aabb box = rect_of(spawn);

    int airborne = 0;
    int moved_while_grounded = 0;
    double peak = box.y;

    for (int frame = 0; frame < 10 * 60; ++frame) {
        const double was_x = box.x;
        const bool was_airborne = frog.airborne;

        box =
            orchard::step_animal(frog, level().collision(), box, prowl, delta);

        peak = std::min(peak, box.y);
        if (frog.airborne) {
            ++airborne;
        } else if (!was_airborne && box.x != was_x) {
            // Grounded at both ends of the frame. The frame it touches down on
            // is the tail of an arc and moves horizontally by design, so it is
            // excluded rather than counted as a slide.
            ++moved_while_grounded;
        }
    }

    INFO("airborne " << airborne << " of 600 frames, peak rise "
                     << rect_of(spawn).y - peak << " px, slid "
                     << moved_while_grounded << " frames");

    // It leaves the ground, and it is not airborne the whole time either —
    // a frog that never lands is falling, not hopping.
    CHECK(airborne > 0);
    CHECK(airborne < 10 * 60);

    // And it stands still between hops. This is what the idle pose is drawn
    // against: a frog that slides along the floor is playing the wrong
    // animation however good the arc looks.
    CHECK(moved_while_grounded == 0);
}

//============================================================================
// The rules
//
// Everything below is arithmetic over supplied booleans: orchard::Rules tests
// no boxes and owns no entity, so a case here says what a frame of contact
// MEANS without standing a renderer up to produce one.
//============================================================================

TEST_CASE(
    "a pickup is worth its points once, and a non-pickup is worth nothing")
{
    orchard::Rules rules;
    const orchard::Stakes stakes;

    REQUIRE(rules.points() == 0);
    REQUIRE(rules.held(orchard::Behaviour::Cherry) == 0);

    CHECK(rules.collect(orchard::Behaviour::Cherry));
    CHECK(rules.points() ==
          stakes.points[orchard::slot_of(orchard::Behaviour::Cherry)]);
    CHECK(rules.held(orchard::Behaviour::Cherry) == 1);
    CHECK(rules.held(orchard::Behaviour::Gem) == 0);

    CHECK(rules.collect(orchard::Behaviour::Gem));
    CHECK(rules.held(orchard::Behaviour::Gem) == 1);
    CHECK(rules.points() ==
          stakes.points[orchard::slot_of(orchard::Behaviour::Cherry)] +
              stakes.points[orchard::slot_of(orchard::Behaviour::Gem)]);

    // The rarer one is worth more, which is the whole reason the two are
    // separate Behaviours rather than one `pickup`.
    CHECK(stakes.points[orchard::slot_of(orchard::Behaviour::Gem)] >
          stakes.points[orchard::slot_of(orchard::Behaviour::Cherry)]);

    // Handed something that is not a pickup, nothing moves and the caller is
    // told so — the answer is what a caller removes an entity on.
    const int before = rules.points();
    CHECK_FALSE(rules.collect(orchard::Behaviour::Opossum));
    CHECK_FALSE(rules.collect(orchard::Behaviour::Goal));
    CHECK(rules.points() == before);
}

TEST_CASE("a bump lands once however long the overlap lasts")
{
    // The case the whole call is shaped around, and the one most likely to be
    // quietly wrong: a player standing inside an animal is one bump, not one
    // per frame. Nothing in a screenshot distinguishes the two.
    orchard::Rules rules;
    rules.collect(orchard::Behaviour::Cherry);
    rules.collect(orchard::Behaviour::Cherry);
    rules.collect(orchard::Behaviour::Cherry);

    int landed = 0;
    for (int frame = 0; frame < 120; ++frame) {
        rules.advance(delta);
        landed += rules.touch_animal(0, true) ? 1 : 0;
    }

    CHECK(landed == 1);
    CHECK(rules.bumps() == 1);
    CHECK(rules.held(orchard::Behaviour::Cherry) == 2);

    // And it rearms on separation, not before.
    rules.touch_animal(0, false);
    rules.advance(delta);
    CHECK(rules.touch_animal(0, true));
    CHECK(rules.bumps() == 2);
}

TEST_CASE("each animal latches separately")
{
    // One shared latch would let the eagle's overlap clear the one the frog
    // set, and the symptom is an animal that stops costing anything once
    // another has stood on the player. Two animals are enough to show it.
    orchard::Rules rules;

    // Both arrive on the same frame. Only the first can land — the second
    // meets a player already sitting down — but the second's latch is set all
    // the same, which is what the recovery below is about.
    CHECK(rules.touch_animal(0, true));
    CHECK_FALSE(rules.touch_animal(1, true));
    CHECK(rules.bumps() == 1);

    recover(rules);
    REQUIRE(rules.accepts_intent());

    // Animal 1 never let go, so it has no edge to give. Animal 0 did, so it
    // has. One latch for both would answer these the other way round.
    CHECK_FALSE(rules.touch_animal(1, true));
    rules.touch_animal(0, false);
    CHECK(rules.touch_animal(0, true));
    CHECK(rules.bumps() == 2);
}

TEST_CASE("a bump costs the newest pickup, and costs nothing when empty")
{
    orchard::Rules rules;
    const orchard::Stakes stakes;
    const int cherry =
        stakes.points[orchard::slot_of(orchard::Behaviour::Cherry)];
    const int gem = stakes.points[orchard::slot_of(orchard::Behaviour::Gem)];

    rules.collect(orchard::Behaviour::Cherry);
    rules.collect(orchard::Behaviour::Gem);
    REQUIRE(rules.points() == cherry + gem);

    // Newest first: the gem goes before the cherry underneath it.
    CHECK(rules.touch_animal(0, true));
    CHECK(rules.held(orchard::Behaviour::Gem) == 0);
    CHECK(rules.held(orchard::Behaviour::Cherry) == 1);
    CHECK(rules.points() == cherry);

    // Separate, stand up, and be caught again. Both are needed: a held overlap
    // has no edge left, and a player still sitting down cannot be bumped.
    rules.touch_animal(0, false);
    recover(rules);
    CHECK(rules.touch_animal(0, true));
    CHECK(rules.held(orchard::Behaviour::Cherry) == 0);
    CHECK(rules.points() == 0);

    // Nothing left to lose. A bump still lands and is still counted — there is
    // no fail state here, so an empty player is not an invulnerable one.
    rules.touch_animal(0, false);
    recover(rules);
    CHECK(rules.touch_animal(0, true));
    CHECK(rules.points() == 0);
    CHECK(rules.bumps() == 3);
}

TEST_CASE(
    "a dizzy spell holds for its tuned duration and then hands control back")
{
    const orchard::Stakes stakes;
    orchard::Rules rules(stakes);

    REQUIRE(rules.phase() == orchard::Phase::Playing);
    REQUIRE(rules.accepts_intent());

    REQUIRE(rules.touch_animal(0, true));
    CHECK(rules.phase() == orchard::Phase::Dizzy);
    CHECK_FALSE(rules.accepts_intent());
    CHECK(rules.dizzy_left() == doctest::Approx(stakes.dizzy_time));

    // The art is 6 frames on a repeat loop precisely so this number, and not
    // the sidecar, decides how long it lasts.
    int frames = 0;
    while (!rules.accepts_intent() && frames < 10 * 60) {
        rules.advance(delta);
        ++frames;
    }

    INFO("recovered after " << frames << " frames");
    CHECK(rules.accepts_intent());
    CHECK(frames >= frames_for(stakes.dizzy_time));
    CHECK(frames <= frames_for(stakes.dizzy_time) + 1);
}

TEST_CASE("a dizzy spell ending inside an animal does not bump again")
{
    // The loop this rule exists to prevent: recover, be bumped on the same
    // frame, lose the next pickup, and never walk out. Held overlap, so there
    // is no edge to give until the player separates.
    orchard::Rules rules;
    for (int i = 0; i < 5; ++i) {
        rules.collect(orchard::Behaviour::Cherry);
    }

    int landed = 0;
    for (int frame = 0; frame < 10 * 60; ++frame) {
        rules.advance(delta);
        landed += rules.touch_animal(0, true) ? 1 : 0;
    }

    CHECK(landed == 1);
    CHECK(rules.held(orchard::Behaviour::Cherry) == 4);
    CHECK(rules.phase() == orchard::Phase::Playing);
}

TEST_CASE(
    "the goal ends the run once, and the run outlives the one frame of art")
{
    const orchard::Stakes stakes;
    orchard::Rules rules(stakes);

    rules.collect(orchard::Behaviour::Gem);
    const int scored = rules.points();

    CHECK_FALSE(rules.reach_goal(false));
    CHECK(rules.phase() == orchard::Phase::Playing);

    CHECK(rules.reach_goal(true));
    CHECK(rules.phase() == orchard::Phase::Finished);
    CHECK_FALSE(rules.accepts_intent());

    // Standing in it does not end it a second time.
    CHECK_FALSE(rules.reach_goal(true));

    // `victory` is one frame on a repeat loop, so the art cannot time this and
    // the state has to. Nothing has elapsed yet, so the run is not over.
    CHECK_FALSE(rules.run_over());

    int frames = 0;
    while (!rules.run_over() && frames < 60 * 60) {
        rules.advance(delta);
        ++frames;
    }
    INFO("victory held " << frames << " frames");
    CHECK(rules.run_over());
    CHECK(frames >= frames_for(stakes.victory_time));
    CHECK(frames <= frames_for(stakes.victory_time) + 1);

    // And nothing is collected or lost after the whistle.
    CHECK_FALSE(rules.collect(orchard::Behaviour::Cherry));
    CHECK_FALSE(rules.touch_animal(0, true));
    CHECK(rules.points() == scored);
}

//============================================================================
// The crossing, a second time, with the rules switched on
//============================================================================

namespace
{

// One thing on the map the player's box is tested against.
struct Thing {
    orchard::Behaviour behaviour = orchard::Behaviour::Cherry;
    game::Aabb box;
};

struct RuledCrossing {
    bool reached = false;
    int frames = 0;
    int bumps = 0;
    int collected = 0;
    int points = 0;
    double furthest_x = 0.0;
};

// The whole demo except the drawing: the player, the three animals, every
// pickup and the goal, stepped together against the shipped map.
//
// The autopilot is the same dumb one the plain crossing uses — walk right, jump
// at whatever blocks you — and it still makes no attempt to avoid an animal.
// That is the point rather than a shortcut: contact costs a pickup and a second
// and a quarter of sitting down, so a driver that walks into every patrol on
// the level is the worst case, and if it still arrives then a player who can
// see the screen certainly does.
RuledCrossing walk_the_rules(int frame_limit)
{
    const orchard::Prowl prowl;

    std::vector<Thing> pickups;
    std::vector<orchard::Animal> animals;
    std::vector<game::Aabb> animal_boxes;
    game::Aabb goal;
    game::Aabb box;

    for (const gfx::MapObject& spawn : level().spawns()) {
        orchard::Behaviour behaviour = orchard::Behaviour::Player;
        if (!orchard::behaviour_for(spawn.type, behaviour)) {
            continue;
        }

        switch (behaviour) {
        case orchard::Behaviour::Player:
            box = rect_of(spawn);
            break;
        case orchard::Behaviour::Goal:
            goal = rect_of(spawn);
            break;
        case orchard::Behaviour::Opossum:
        case orchard::Behaviour::Frog:
        case orchard::Behaviour::Eagle:
            animals.push_back(orchard::animal_at(behaviour, spawn.x, prowl));
            animal_boxes.push_back(rect_of(spawn));
            break;
        default:
            pickups.push_back(Thing{behaviour, rect_of(spawn)});
            break;
        }
    }

    orchard::Player player;
    orchard::Rules rules;
    const orchard::Motion motion;
    JumpButton jump;

    RuledCrossing run;
    run.furthest_x = box.x;

    for (int frame = 0; frame < frame_limit; ++frame) {
        rules.advance(delta);

        if (player.on_ground() && player.bumped()) {
            jump.press();
        }

        orchard::Intent intent;
        jump.apply(intent);
        intent.right = true;

        // Sitting down is not steering. The rules withhold the intent rather
        // than reaching into Player, so gravity still applies to a dizzy player
        // and it still falls off whatever it was bumped on.
        if (!rules.accepts_intent()) {
            intent = orchard::Intent();
        }

        box = player.step(level().collision(), box, intent, motion, delta);
        run.furthest_x = std::max(run.furthest_x, box.x);

        for (std::size_t i = 0; i < animals.size(); ++i) {
            animal_boxes[i] = orchard::step_animal(
                animals[i], level().collision(), animal_boxes[i], prowl, delta);

            if (rules.touch_animal(i, game::overlaps(box, animal_boxes[i]))) {
                ++run.bumps;
            }
        }

        for (std::size_t i = 0; i < pickups.size();) {
            if (game::overlaps(box, pickups[i].box) &&
                rules.collect(pickups[i].behaviour)) {
                pickups.erase(pickups.begin() + static_cast<std::ptrdiff_t>(i));
                ++run.collected;
            } else {
                ++i;
            }
        }

        if (rules.reach_goal(game::overlaps(box, goal))) {
            run.reached = true;
            run.frames = frame + 1;
            break;
        }
    }

    run.points = rules.points();
    return run;
}

} // namespace

TEST_CASE("the level is still crossable once contact costs something")
{
    const RuledCrossing run = walk_the_rules(180 * 60);

    // The frame count is recorded for the reason the plain crossing records
    // its own: it is the number that moves when something has been wired into
    // the simulation that should not be, and an INFO makes it visible in a
    // diff instead of leaving it to be rediscovered.
    INFO("crossed in " << run.frames << " frames");
    INFO("furthest x " << run.furthest_x << " of 1536, " << run.bumps
                       << " bumps, " << run.collected << " pickups, "
                       << run.points << " points");

    // The claim this file exists to make, restated under the rules. If a bump
    // could trap the run — the recovery frame re-bumping inside the same animal
    // is the way it would — this is where it shows, and it shows as a hang
    // rather than as a wrong pixel.
    REQUIRE(run.reached);
    CHECK(run.frames < 180 * 60);

    // The autopilot routes around nothing, so it meets the patrols head on.
    // A run that took no bumps at all would mean contact is not being detected,
    // which is exactly the silent failure this case is here to catch.
    CHECK(run.bumps > 0);

    // And it forages on the way, because the pickups on the ground line are in
    // its path. Zero here means overlap is being tested against the wrong box.
    CHECK(run.collected > 0);
    CHECK(run.points > 0);
}

//============================================================================
// Facing
//============================================================================

TEST_CASE("the player's art looks right and the three animals' looks left")
{
    // The two sheets as drawn: `foxy` with its snout to the right, and the
    // opossum, frog and eagle of `critters` with their heads to the left.
    CHECK(orchard::native_facing(orchard::Behaviour::Player) == 1);
    CHECK(orchard::native_facing(orchard::Behaviour::Opossum) == -1);
    CHECK(orchard::native_facing(orchard::Behaviour::Frog) == -1);
    CHECK(orchard::native_facing(orchard::Behaviour::Eagle) == -1);
}

TEST_CASE("an actor is drawn reversed exactly when it heads against its art")
{
    using orchard::Behaviour;
    using orchard::drawn_reversed;

    CHECK_FALSE(drawn_reversed(Behaviour::Player, 1));
    CHECK(drawn_reversed(Behaviour::Player, -1));

    // The opposite rule, which is the reason this is data per behaviour: one
    // rule for all four walks three of them backwards.
    for (const Behaviour animal :
         {Behaviour::Opossum, Behaviour::Frog, Behaviour::Eagle}) {
        CAPTURE(orchard::behaviour_name(animal));
        CHECK_FALSE(drawn_reversed(animal, -1));
        CHECK(drawn_reversed(animal, 1));
    }
}

TEST_CASE("art with no facing is never drawn reversed")
{
    using orchard::Behaviour;

    for (const Behaviour still : {Behaviour::Cherry, Behaviour::Gem,
                                  Behaviour::Goal, Behaviour::Count}) {
        CAPTURE(orchard::behaviour_name(still));
        CHECK(orchard::native_facing(still) == 0);
        CHECK_FALSE(orchard::drawn_reversed(still, -1));
        CHECK_FALSE(orchard::drawn_reversed(still, 1));
    }

    // No direction is not a direction to turn towards.
    CHECK_FALSE(orchard::drawn_reversed(Behaviour::Player, 0));
    CHECK_FALSE(orchard::drawn_reversed(Behaviour::Opossum, 0));
}

TEST_CASE("a newly spawned animal and a newly made player both start "
          "unreversed")
{
    // The entity's mirror flag defaults to unmirrored and is first written by
    // the step, so the starting headings have to be ones that need no
    // reversal or the first drawn frame of a run that draws before stepping
    // would face the wrong way.
    const orchard::Prowl prowl;
    for (const orchard::Behaviour animal :
         {orchard::Behaviour::Opossum, orchard::Behaviour::Frog,
          orchard::Behaviour::Eagle}) {
        const orchard::Animal spawned = orchard::animal_at(animal, 0.0, prowl);
        CHECK_FALSE(orchard::drawn_reversed(animal, spawned.direction));
    }

    const orchard::Player player;
    CHECK_FALSE(
        orchard::drawn_reversed(orchard::Behaviour::Player, player.facing()));
}
