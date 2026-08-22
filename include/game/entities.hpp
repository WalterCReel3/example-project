#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <game/collide.hpp>
#include <gfx/animation.hpp>
#include <gfx/renderer/context.hpp>

//============================================================================
//
// A flat entity store
//
//     game::Entities entities;
//     const game::Entities::Id id = entities.add(actor);
//     entities.update(delta);
//     entities.draw(context, camera_x, camera_y);
//
// Position, a sprite, an update tick and a draw order. Not a component system
// and not a scene graph: this drives several animated things independently and
// hands out stable handles to them, and the rest of the game layer — collision,
// camera, input actions — sits beside it rather than inside it.
//
// An Entity also carries a collision box and a behaviour tag, but the store
// only stores them: update() integrates position and advances sprites and does
// not test the boxes, and nothing here dispatches on a tag. They live on the
// entity because that is where a caller doing either of those jobs needs to
// find them, not because this store performs them.
//
// Storage is an array of structs rather than parallel arrays. At the scale this
// runs at — tens of entities, not tens of thousands — the cache argument for a
// structure of arrays does not pay for the ergonomics it costs, and the honest
// reason to reach for one would be a measurement that does not exist yet.
//
// The store is not templated over its payload. Two consumers do not say where
// the seam between "storage" and "what is stored" belongs, and a demo that does
// not read a field pays a few unused bytes across tens of entities for the
// privilege of not guessing. See planning/2026-08-10-game-layer-and-demo/,
// decision 5.
//
//============================================================================
namespace game
{

// No behaviour attached. Negative so that a consumer enum starting at its
// natural 0 cannot collide with it.
//
// The tag is an opaque int and this module never learns what any value means:
// a demo declares its own `enum class Behaviour { Player, Frog, ... }` and
// casts once, at the spawn site. Spelling those actors out here would put one
// demo's vocabulary inside the module every other demo is meant to share.
//
// The type is signed *because* of the sentinel — the two are one decision. A
// C++ enumeration numbers from 0 unless told otherwise, so reserving 0 would
// swallow whichever actor the consumer happened to list first, and the symptom
// is that one actor stops responding while spawning, drawing and moving
// correctly. That reads as an input bug, not a tag bug.
constexpr int no_behaviour = -1;

struct Entity {
    // World pixels, not screen: draw() subtracts the camera. Doubles because
    // position integrates a velocity against a frame delta, and rounding to
    // integers every frame makes slow movement stutter.
    double x = 0.0;
    double y = 0.0;

    // Pixels per second.
    double velocity_x = 0.0;
    double velocity_y = 0.0;

    gfx::AnimatedSprite sprite;

    int scale = 1;

    // Draw order, low to high. Ties break on y, so something further down the
    // screen draws in front — the usual 2D depth cue, and the reason this is
    // not simply insertion order.
    int layer = 0;

    // The collision box, RELATIVE TO (x, y) — not absolute world space. Use
    // world_box() to get the absolute one; do not read `box` against a tile
    // grid directly.
    //
    // Relative because (x, y) is the only position this entity has. update()
    // integrates velocity into it, so an absolute box would be a second copy
    // of the position that every mover has to remember to advance in step, and
    // when one path forgets, both members still hold perfectly valid boxes
    // that simply disagree. Storing the offset makes that state unreachable.
    //
    // Spawning stays readable despite the offset: a gfx::MapObject carries
    // x/y/width/height in map-origin pixels, which is the space (x, y) is
    // already in, so a spawn writes `entity.x = object.x; entity.y = object.y;
    // entity.box = {0.0, 0.0, object.width, object.height}`. The offset is
    // non-zero only when the box is deliberately inset from the sprite.
    //
    // Default is an empty box, which game::overlaps and game::overlaps_solid
    // both read as "there is nothing here" — so an entity nobody gave a box to
    // collides with nothing rather than being a point collider at its feet.
    Aabb box;

    // Which rule drives this entity, or no_behaviour for none. Opaque to this
    // module: see the note on no_behaviour above.
    int behaviour = no_behaviour;

    // The collision box in world space. The one place (x, y) and `box` are
    // combined, so a caller cannot get the addition backwards.
    Aabb world_box() const
    {
        return Aabb{x + box.x, y + box.y, box.width, box.height};
    }
};

class Entities
{
public:
    // A handle, not an index. Slots are reused when an entity is removed, and a
    // bare index would silently address whatever took the slot over — the kind
    // of bug that presents as one entity inheriting another's animation.
    struct Id {
        std::uint32_t index = 0;
        std::uint32_t generation = 0;

        bool operator==(const Id& rh) const
        {
            return index == rh.index && generation == rh.generation;
        }
        bool operator!=(const Id& rh) const { return !(*this == rh); }
    };

    // An Id that never refers to anything, including in a fresh store.
    static Id none() { return Id{0, 0}; }

    Id add(Entity entity);

    // Removing twice is not an error; the second call finds a stale handle.
    void remove(Id id);

    bool alive(Id id) const;

    // Null for a stale or removed handle, so a caller that keeps ids across
    // frames has something to test rather than a reference to a reused slot.
    Entity* find(Id id);
    const Entity* find(Id id) const;

    // Live entities. Not the slot count — removed slots stay allocated for
    // reuse and are not counted here.
    std::size_t size() const { return _live; }
    bool empty() const { return _live == 0; }

    void clear();

    // Advances every live sprite and integrates position from velocity.
    void update(double delta);

    // Draws every live entity, back to front, with the camera subtracted.
    // Returns the number drawn.
    //
    // Sorts an index buffer each call rather than keeping the store ordered:
    // ordering depends on y, which changes every frame for anything moving, so
    // a sorted store would be re-sorted just as often and would invalidate
    // handles while doing it. The buffer is reused between frames.
    int draw(gfx::renderer::Context& context, int camera_x, int camera_y) const;

    // Iteration over live entities, for callers that need to touch state this
    // interface does not model. Skips dead slots.
    template<typename Visitor>
    void each(Visitor visit)
    {
        for (Slot& slot : _slots) {
            if (slot.alive) {
                visit(slot.entity);
            }
        }
    }

private:
    struct Slot {
        Entity entity;
        std::uint32_t generation = 0;
        bool alive = false;
    };

    std::vector<Slot> _slots;
    std::vector<std::uint32_t> _free;
    std::size_t _live = 0;

    // Store-wide and monotonic, never reset. A per-slot counter is not enough:
    // clear() releases the slots, so the next add() would start a fresh slot at
    // generation 1 and an old handle to index 0 would match it. Handing out a
    // value that has never been used before makes every prior handle stale no
    // matter what happened to the storage. Starts at 1 so none() — {0, 0} —
    // is never a live handle.
    std::uint32_t _next_generation = 1;

    // Draw order, rebuilt per draw. Mutable because producing it is an
    // implementation detail of a const operation.
    mutable std::vector<std::uint32_t> _order;
};

} // namespace game
