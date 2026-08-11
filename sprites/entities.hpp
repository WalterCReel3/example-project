#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <gfx/animation.hpp>
#include <gfx/renderer/context.hpp>

//============================================================================
//
// A flat entity store
//
//     sprites::Entities entities;
//     const sprites::Entities::Id id = entities.add(actor);
//     entities.update(delta);
//     entities.draw(context, camera_x, camera_y);
//
// Position, a sprite, an update tick and a draw order. Not a component system,
// not a scene graph, and deliberately not either: collision, camera and physics
// are a game layer with their own snapshot, and this exists to drive several
// animated things independently, which is what
// planning/2026-07-25-software-2d-sprites-tiling/ asked for.
//
// **Why this lives in sprites/ and not in a game/ module.** That snapshot left
// the question open and leaned this way, on the grounds that a boundary drawn
// before there are two users tends to be drawn wrong. There is still one user.
// Promoting it is a file move plus a namespace, and it should happen when a
// second consumer turns up and says what the interface actually needs to be —
// not before, on a guess about what that will be.
//
// Storage is an array of structs rather than parallel arrays. At the scale this
// runs at — tens of entities, not tens of thousands — the cache argument for a
// structure of arrays does not pay for the ergonomics it costs, and the honest
// reason to reach for one would be a measurement that does not exist yet.
//
//============================================================================
namespace sprites
{

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

} // namespace sprites
