#include <game/entities.hpp>

#include <algorithm>
#include <utility>

namespace game
{

Entities::Id Entities::add(Entity entity)
{
    ++_live;

    if (!_free.empty()) {
        const std::uint32_t index = _free.back();
        _free.pop_back();

        Slot& slot = _slots[index];
        slot.entity = std::move(entity);
        slot.generation = _next_generation++;
        slot.alive = true;
        return Id{index, slot.generation};
    }

    Slot slot;
    slot.entity = std::move(entity);
    slot.generation = _next_generation++;
    slot.alive = true;
    _slots.push_back(std::move(slot));

    return Id{static_cast<std::uint32_t>(_slots.size() - 1),
              _slots.back().generation};
}

void Entities::remove(Id id)
{
    if (!alive(id)) {
        return;
    }

    Slot& slot = _slots[id.index];
    slot.alive = false;

    // Cleared on removal rather than on reuse, so every handle to this slot
    // goes stale the moment the entity does, not only once something else takes
    // the slot over. 0 never matches a handed-out generation.
    slot.generation = 0;

    // The sprite holds observer pointers into an Atlas and an Animation. Not
    // clearing them would leave a dead slot pointing at live objects, which is
    // harmless until something iterates slots instead of live entities.
    slot.entity = Entity();

    _free.push_back(id.index);
    --_live;
}

bool Entities::alive(Id id) const
{
    if (id.index >= _slots.size()) {
        return false;
    }

    const Slot& slot = _slots[id.index];
    return slot.alive && slot.generation == id.generation;
}

Entity* Entities::find(Id id)
{
    return alive(id) ? &_slots[id.index].entity : nullptr;
}

const Entity* Entities::find(Id id) const
{
    return alive(id) ? &_slots[id.index].entity : nullptr;
}

void Entities::clear()
{
    // The storage goes, but _next_generation does not rewind — so the slots a
    // later add() creates carry values no outstanding handle has ever held, and
    // a handle taken before the clear cannot match one of them. Resetting the
    // counter here is exactly the bug the generation exists to prevent.
    _slots.clear();
    _free.clear();
    _live = 0;
}

void Entities::update(double delta)
{
    if (delta <= 0.0) {
        return;
    }

    for (Slot& slot : _slots) {
        if (!slot.alive) {
            continue;
        }

        Entity& entity = slot.entity;
        entity.sprite.advance(delta);
        entity.x += entity.velocity_x * delta;
        entity.y += entity.velocity_y * delta;
    }
}

int Entities::draw(gfx::renderer::Context& context, int camera_x,
                   int camera_y) const
{
    _order.clear();
    _order.reserve(_live);

    for (std::uint32_t i = 0; i < _slots.size(); ++i) {
        if (_slots[i].alive) {
            _order.push_back(i);
        }
    }

    // Layer first, then y, so something lower on the screen draws in front of
    // something above it at the same layer. std::sort rather than stable_sort:
    // the tie beyond (layer, y) is two entities at the same depth *and* the
    // same height, where the order is genuinely arbitrary, and paying for
    // stability to fix an ordering nobody can observe is not worth it.
    const std::vector<Slot>& slots = _slots;
    std::sort(_order.begin(), _order.end(),
              [&slots](std::uint32_t lh, std::uint32_t rh) {
                  const Entity& a = slots[lh].entity;
                  const Entity& b = slots[rh].entity;
                  if (a.layer != b.layer) {
                      return a.layer < b.layer;
                  }
                  return a.y < b.y;
              });

    int drawn = 0;
    for (std::uint32_t index : _order) {
        const Entity& entity = slots[index].entity;

        entity.sprite.draw(context, static_cast<int>(entity.x) - camera_x,
                           static_cast<int>(entity.y) - camera_y, entity.scale);
        ++drawn;
    }

    return drawn;
}

} // namespace game
