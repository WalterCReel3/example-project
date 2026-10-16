#include "actors.hpp"

namespace orchard
{

namespace
{

// The spawn table's vocabulary, in one place. Kept as a table rather than a
// chain of comparisons so that adding a type is one line and cannot be added
// to the lookup without also being added to the name.
struct Named {
    const char* type;
    Behaviour behaviour;
};

const Named names[] = {
    {"player", Behaviour::Player}, {"opossum", Behaviour::Opossum},
    {"frog", Behaviour::Frog},     {"eagle", Behaviour::Eagle},
    {"cherry", Behaviour::Cherry}, {"gem", Behaviour::Gem},
    {"goal", Behaviour::Goal},
};

// A probe two pixels deep just under the feet. Deep enough to find a floor the
// walker is resting flush on — the box's bottom edge is exactly the floor's top
// edge, and the half-open rule means the box itself covers none of that cell.
const double probe = 2.0;

int turned(int direction)
{
    return -direction;
}

// Whether this one has wandered past its leash and is still heading away.
// Tested against the direction as well as the distance, so an animal nudged
// past the limit turns once rather than every frame.
bool beyond_leash(const Animal& animal, double x, double span)
{
    return (animal.direction > 0 && x > animal.home_x + span) ||
           (animal.direction < 0 && x < animal.home_x - span);
}

Intent walking(int direction)
{
    Intent intent;
    intent.right = direction > 0;
    intent.left = direction < 0;
    return intent;
}

game::Aabb step_walker(Animal& animal, const game::TileGrid& solid,
                       const game::Aabb& box, const Motion& motion, double span,
                       double delta)
{
    Player& body = animal.body;
    const game::Aabb moved =
        body.step(solid, box, walking(animal.direction), motion, delta);

    // After the step and not before it: a walker that has just been stopped by
    // a wall reports the bump on the step that was refused, so turning here
    // means the next step is the one that moves.
    if (body.bumped() || beyond_leash(animal, moved.x, span) ||
        (body.on_ground() && !floor_ahead(solid, moved, animal.direction))) {
        animal.direction = turned(animal.direction);
    }

    return moved;
}

game::Aabb step_frog(Animal& animal, const game::TileGrid& solid,
                     const game::Aabb& box, const Prowl& prowl, double delta)
{
    Player& body = animal.body;

    // As of the last step, which is what the whole sequence below is phrased
    // against: on_ground() is recomputed by step() and reading it after would
    // be asking about a frame that has not been decided yet.
    const bool grounded = body.on_ground();

    Intent intent;

    if (grounded && animal.hold <= 0.0) {
        animal.rest -= delta;
        if (animal.rest <= 0.0) {
            animal.hold = prowl.frog_hold;
            intent.jump_pressed = true;
        }
    }

    if (animal.hold > 0.0) {
        intent.jump_held = true;
        animal.hold -= delta;
    }

    // A frog carries its direction only while it is in the air or leaving the
    // ground. Sitting still between hops is the pose the idle animation is
    // drawn for, and a frog that slid along the floor between hops would be
    // playing it against the wrong motion.
    //
    // Read from the flag this function keeps rather than from the body's
    // on_ground(), which answers false before the first step has decided
    // anything — an unstepped frog would otherwise take its opening frame for
    // the top of an arc and drift a pixel sideways out of its spawn.
    if (animal.airborne || intent.jump_pressed) {
        const Intent carried = walking(animal.direction);
        intent.left = carried.left;
        intent.right = carried.right;
    }

    const game::Aabb moved = body.step(solid, box, intent, prowl.frog, delta);

    // No ledge probe. A hopper clearing a gap is airborne over exactly the
    // cells a probe would report as missing, and turning there would make it
    // hop back and forth across one edge forever. The leash is what keeps it
    // near its spawn.
    if (body.bumped() || beyond_leash(animal, moved.x, prowl.walk_span)) {
        animal.direction = turned(animal.direction);
    }

    if (body.on_ground() && !grounded) {
        animal.rest = prowl.frog_rest;
        animal.hold = 0.0;
    }

    animal.airborne = !body.on_ground();
    return moved;
}

// The eagle. No grid, no gravity: it holds its spawn altitude and turns at the
// ends of its span. Terrain does not stop it, which is decision-free here —
// nothing it could hit does anything on contact this slice.
game::Aabb step_eagle(Animal& animal, const game::Aabb& box, const Prowl& prowl,
                      double delta)
{
    game::Aabb moved = box;
    moved.x += animal.direction * prowl.eagle_speed * delta;

    if (beyond_leash(animal, moved.x, prowl.fly_span)) {
        animal.direction = turned(animal.direction);
    }

    return moved;
}

} // namespace

Prowl::Prowl()
{
    // Half the player's walk, so the demo's own mover is plainly the fastest
    // thing on screen and an animal is something to step over rather than
    // outrun.
    opossum.walk_speed = 40.0;

    // The frog's horizontal speed applies only while it is airborne, so this
    // is the speed of a hop and not of a walk.
    frog.walk_speed = 70.0;

    // THE HOP HAS TO CLEAR 32 px, which is the tallest step in the ground
    // profile — the same number Motion's own comment sizes the player's jump
    // against. A frog that hops down off a ledge it cannot hop back up is
    // trapped there for the rest of the run, and the symptom is an animal
    // pacing a pit two screens back rather than anything that looks like a
    // tuning mistake. At 320 px/s against 1200 px/s^2 the rise is 42 px, or
    // 39 px as actually stepped at 60 Hz.
    frog.jump_speed = 320.0;

    // COYOTE TIME AND THE JUMP BUFFER KEEP MOTION'S DEFAULTS, which for a
    // scripted hopper is a choice and not a requirement. Both are an allowance
    // for a human's late press and a frog presses on the exact frame it means
    // to, so zeroing them would change nothing it does: a press while standing
    // fires whatever either is tuned to. They are left alone because a second
    // set of numbers here would be two tunings to keep in step for no effect.
}

bool behaviour_for(const std::string& type, Behaviour& behaviour)
{
    for (const Named& named : names) {
        if (type == named.type) {
            behaviour = named.behaviour;
            return true;
        }
    }
    return false;
}

const char* behaviour_name(Behaviour behaviour)
{
    for (const Named& named : names) {
        if (behaviour == named.behaviour) {
            return named.type;
        }
    }
    return "unknown";
}

Animal animal_at(Behaviour behaviour, double home_x, const Prowl& prowl)
{
    Animal animal;
    animal.behaviour = behaviour;
    animal.home_x = home_x;
    animal.direction = -1;
    animal.rest = prowl.frog_rest;
    return animal;
}

bool floor_ahead(const game::TileGrid& solid, const game::Aabb& box,
                 int direction)
{
    const double x = direction > 0 ? box.x + box.width : box.x - probe;
    return game::overlaps_solid(
        solid, game::Aabb{x, box.y + box.height, probe, probe});
}

game::Aabb step_animal(Animal& animal, const game::TileGrid& solid,
                       const game::Aabb& box, const Prowl& prowl, double delta)
{
    switch (animal.behaviour) {
    case Behaviour::Opossum:
        return step_walker(animal, solid, box, prowl.opossum, prowl.walk_span,
                           delta);
    case Behaviour::Frog:
        return step_frog(animal, solid, box, prowl, delta);
    case Behaviour::Eagle:
        return step_eagle(animal, box, prowl, delta);
    default:
        // A pickup, the goal, or the player, which this does not drive.
        return box;
    }
}

} // namespace orchard
