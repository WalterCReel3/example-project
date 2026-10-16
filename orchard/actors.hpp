#pragma once

#include <string>

#include <game/collide.hpp>
#include <game/entities.hpp>

#include "player.hpp"

//============================================================================
//
// orchard's cast: what a spawn type means, and how the animals move
//
//     orchard::Behaviour behaviour;
//     if (orchard::behaviour_for(spawn.type, behaviour)) { ... }
//
//     box = orchard::step_animal(animal, level.collision(), box, prowl, delta);
//
// NO RENDERER AND NO ATLAS, for the reason player.hpp gives: the spawn table's
// vocabulary and the animals' motion are both arithmetic over supplied state,
// so tests/test_orchard.cc compiles this in and asks what the shipped map
// spawns without opening a window. Which sprite each behaviour draws with is
// orchard/demo.cc's business and is not expressible here.
//
//============================================================================
namespace orchard
{

// This demo's vocabulary for game::Entity::behaviour, which is an opaque int to
// game:: — decision 8. Cast once, at the spawn site.
//
// One value per spawn type rather than one per rule, so `cherry` and `gem` are
// separate although they behave identically today. They are separate in the
// map and a score that tells them apart is the next slice's to write; folding
// them together here would have to be undone there, and the count this file
// reports would stop matching the count the map holds.
enum class Behaviour {
    Player,
    Opossum,
    Frog,
    Eagle,
    Cherry,
    Gem,
    Goal,

    Count
};

// The behaviour a `gfx::MapObject::type` names (decision 3: the type arrives as
// a string). False for a type this demo has no rule for, leaving `behaviour`
// untouched — an unknown spawn is a level to fix or a slice not yet written,
// and the caller is what decides which and says so.
bool behaviour_for(const std::string& type, Behaviour& behaviour);

// For logs. Never null, including for Count.
const char* behaviour_name(Behaviour behaviour);

// The animals' tuning, in one struct so a test drives the shipped numbers
// rather than a copy of them — same reason Motion is one.
//
// The two walkers are moved by orchard::Player, which after the extent moved
// out of it is a box, a velocity and a tile grid and has nothing player-shaped
// left in it. Giving the opossum its own copy of gravity, sliding and a jump
// would be that file pasted with the Intent hard-coded; supplying the Intent is
// the whole of what a patrol is. The eagle does not use it at all — it is
// airborne and never resolves against the grid, so a Motion would only be
// numbers it ignores.
struct Prowl {
    // The same tuning struct the player is driven by, with different numbers.
    // Set in the constructor rather than here: only three of the members differ
    // from Motion's own defaults, and spelling all of them out would make the
    // three that matter invisible.
    Motion opossum;
    Motion frog;

    // px/s, horizontal. The eagle keeps its spawn altitude.
    double eagle_speed = 55.0;

    // How far either side of its spawn an actor patrols before turning, in
    // pixels. A leash rather than authored waypoints: the map carries a point
    // per animal and nothing else, and a walker that turns only at walls would
    // leave the screen it was placed on.
    double walk_span = 96.0;
    double fly_span = 160.0;

    // Seconds the frog sits between hops, and how long it holds its own Jump.
    // The hold is what makes the hop an arc rather than the shortest possible
    // trim — see Motion::jump_cut_speed.
    double frog_rest = 1.0;
    double frog_hold = 0.25;

    Prowl();
};

// One animal, beside the entity that draws it.
//
// The entity holds the position, the box and the sprite. This holds what
// game::Entities deliberately does not model (decision 5): which rule drives
// this one, which way it is facing, where it started, and the frog's hop
// timer. `entity` is the caller's link between the two; nothing in actors.cc
// reads it.
struct Animal {
    game::Entities::Id entity = game::Entities::none();
    Behaviour behaviour = Behaviour::Opossum;

    // The walking, falling and jumping. Unused by the eagle, which is airborne
    // and resolves against nothing.
    Player body;

    // -1 or 1.
    int direction = 1;

    // The x this one spawned at, which is the centre of its patrol.
    double home_x = 0.0;

    // Frog only: seconds left of sitting still, and seconds left of holding
    // its own Jump down.
    double rest = 0.0;
    double hold = 0.0;

    // Whether the last step left this one off the ground. Frog only, and the
    // sprite is the only thing that reads it — a caller picks frog-jump over
    // frog-idle from this rather than re-deriving it.
    bool airborne = false;
};

// A newly spawned animal: pointed left, leashed to the x it spawned at, and —
// if it is the frog — already counting down to its first hop.
//
// Left, so the animals walk into a player that only ever travels right rather
// than away from one.
//
// Here rather than at the spawn site because the spawn site is not the only
// caller: tests/test_orchard.cc starts the same three animals from the same
// map, and a patrol tested from a different starting state to the one shipped
// is a patrol tested somewhere else. `entity` is left empty for the caller.
Animal animal_at(Behaviour behaviour, double home_x, const Prowl& prowl);

// Advances one animal and answers where its collision box ended up, in world
// pixels. Motion only: the entity, its sprite and the camera are the caller's,
// which is what keeps this file free of gfx.
//
// A Behaviour that does not move — a pickup, the goal — is answered with the
// box it was handed. Placing those once at spawn is correct and stepping them
// is simply nothing, rather than an error.
game::Aabb step_animal(Animal& animal, const game::TileGrid& solid,
                       const game::Aabb& box, const Prowl& prowl, double delta);

// Whether there is floor to walk onto just past the leading edge of `box`.
//
// Exposed because it is the one piece of a patrol that is not already in
// Player::step(), and it is worth pinning directly: a walker that turns at
// walls but not at ledges walks off the platform it was placed on, and the
// symptom — an animal on the ground floor two screens away — reads as a bad
// spawn rather than a missing probe.
bool floor_ahead(const game::TileGrid& solid, const game::Aabb& box,
                 int direction);

} // namespace orchard
