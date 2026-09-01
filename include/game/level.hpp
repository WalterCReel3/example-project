#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include <game/collide.hpp>
#include <gfx/tilemap.hpp>

//============================================================================
//
// A level: what is solid, and what to place
//
//     const loaders::TmxMap tmx = loaders::load_tmx(rig::asset_path("x.tmx"));
//
//     const game::Level level(tmx.layers, tmx.object_layers, tmx.width,
//                             tmx.height, tmx.tile_width, tmx.tile_height,
//                             "x.tmx");
//
//     camera.set_bounds(level.pixel_width(), level.pixel_height());
//     for (const gfx::MapObject& spawn : level.spawns()) {
//         entities.add(actor_for(spawn));   // the demo owns the vocabulary
//     }
//     const game::Resolution moved =
//         game::slide(level.collision(), box, dx, dy);
//
// The rules half of a map and only that: what is solid, where things start, and
// how big the world is. Drawing the map stays with gfx::TileMap, which is built
// from the same parsed data alongside this rather than owned by it.
//
// HEADLESS, DELIBERATELY, and that is what shaped the interface. A gfx::TileMap
// owns a gfx::TileSet, which owns a renderer::Texture, which cannot exist
// without a Context and so without a video driver. A Level holding one would
// drag a window into a test of arithmetic that has nothing to do with one, and
// would drop this module's level rules out of the group in tests/CMakeLists.txt
// that runs under qemu-user-static on the cross targets — currently the only
// route any of game/ has to being exercised on something other than an x86-64
// host. So a Level is built from the parsed layers: it takes exactly what
// loaders::load_tmx returns, minus the tileset.
//
// It COPIES the collision layer rather than pointing at one. That meets
// "resolve the layer once at load" more completely than an index into somebody
// else's vector would — there is no per-frame name lookup and also no map to
// keep alive, so the drawable gfx::TileMap can be moved, rebuilt or dropped
// without invalidating the level. The copy is one std::vector<int> of width x
// height: 9 KB for the 64x36 fixture, against 128 MB on the smallest target.
//
// THE SPAWN TABLE IS gfx::MapObject, AND `type` STAYS A STRING. Decision 3 of
// planning/2026-08-10-game-layer-and-demo/ calls it "a typed table"; decision 8
// settled that the actor vocabulary — player, frog, opossum, eagle, cherry, gem
// — is one demo's and must not enter this module, which is why
// Entity::behaviour is an opaque int. Turning the string into a game::SpawnKind
// here would put the vocabulary back in through the other door, so the table is
// typed in the sense that matters — named fields, not a bag of attributes — and
// the string is mapped to a demo's own enum at the spawn site, next to the cast
// decision 8 already asks for. gfx::MapObject rather than a game::Spawn of the
// same shape: it already carries name, type, id and the rectangle in map-origin
// pixels, which is the space Entity::x/y is in, and a parallel struct would be
// a copy to keep in step for no field it adds.
//
//============================================================================
//
// WHAT IT REFUSES TO BUILD. Each of these is a level that would otherwise run
// and be wrong in a way nobody notices for a while:
//
//   - no tile layer named "collision". Every cell would read as empty and the
//     player falls through a floor that draws correctly. Solidity IS a layer
//     here, not a per-tile property (decision 2), so a missing layer is not a
//     level without walls, it is a level without data.
//   - no object layer named "spawns". A level that places nothing.
//   - a collision layer that is not the map's size. collide treats everything
//     outside the layer as solid, so a short layer is an invisible wall part
//     way across a level whose art carries on past it — and the camera, clamped
//     to the map, shows the unreachable far side.
//   - a collision layer whose tile data is not width x height entries long.
//     collide bounds-checks a cell against the layer's declared rectangle and
//     then reads it unchecked, so a layer that declares more cells than it
//     carries is read past the end of its own vector rather than answering
//     wrongly. loaders::load_tmx cannot produce one — it refuses a CSV whose
//     length disagrees with the declared size — but this constructor is public
//     and takes bare vectors.
//   - a collision layer with a non-zero draw offset. game::TileGrid carries no
//     offset and collide applies none, so the solid cells would sit somewhere
//     other than where the layer draws.
//   - a spawn outside the level, by the rule below.
//   - a non-positive map or tile dimension.
//
// NOT refused, both stated because they read as omissions:
//
//   - an EMPTY spawns layer. This module does not know that a `player` is
//     required and must not learn — that is the vocabulary decision 8 keeps
//     out. A demo that needs a player checks for one.
//   - a SECOND layer of either name. The first match wins, matching
//     gfx::TileMap::find, which is the lookup this replaces.
//
//============================================================================
//
// THE SPAWN BOUNDS CHECK, which is load-bearing rather than belt-and-braces.
// The reasoning is decision 10's and the interacting half of it is written out
// under slide() in include/game/collide.hpp.
//
// game::collide deliberately does not rescue an entity that starts outside the
// map. Outside is solid, but only cells the box does not already occupy can
// block it, so an entity far outside is frozen — loud, and findable. An entity
// just past the edge instead WALKS BACK IN on the first frame reporting no hit
// at all, and from then on looks entirely correct while the level data stays
// wrong. This check is the only thing in the tree that catches that second one.
//
// WHAT IT CHECKS IS THE OBJECT'S OWN RECTANGLE, which is the entity's collision
// box only under the spawn idiom include/game/entities.hpp documents —
// `entity.box = {0.0, 0.0, object.width, object.height}`. A point object is
// checked as the single point (x, y) and is legal anywhere in the level, so a
// spawn site that point-spawns and then gives the entity a box of its own has a
// world_box() this class never saw, and it may lie mostly outside the level
// having been told yes. The guarantee is over what the map says, not over what
// the demo builds from it.
//
// THE RULE. The level is the half-open rectangle [0, pixel_width()) by
// [0, pixel_height()) — the same half-openness game::Aabb uses, so a spawn box
// and a collision box agree about what "inside" means. A spawn is inside when
// every point of it is:
//
//   - AN OBJECT WITH EXTENT — width > 0 and height > 0 — spans [x, x + width)
//     and [y, y + height), so it needs x >= 0, y >= 0, x + width <=
//     pixel_width() and y + height <= pixel_height(). A box whose right edge
//     lands exactly on pixel_width() is INSIDE: it covers the last legal pixel
//     column and not the one after it. One pixel further right is not.
//   - AN OBJECT WITH NO EXTENT — Tiled writes a point object as width = 0,
//     height = 0 — is treated as the single point (x, y), inside when
//     0 <= x < pixel_width() and 0 <= y < pixel_height(). The degenerate case
//     is spelled out rather than left to fall out of the general one, because
//     "every point of an empty box is inside" is vacuously true and would
//     accept a point anywhere at all. A negative extent is read the same way.
//
// The axes are independent, and so are the two extents: an object may have
// extent on x and none on y, and each axis is judged by its own rule. Every
// number in the three paragraphs above is asserted in tests/test_level.cc, so
// this comment fails a build when it stops being true — the arrangement
// collide.hpp's out-of-bounds paragraph is under, and for the same reason.
//
//============================================================================
namespace game
{

// The tile layer that says what is solid, and the object layer that says what
// to place. Fixed rather than passed in: they are decisions 2 and 3 of
// planning/2026-08-10-game-layer-and-demo/, not a per-level setting, and a map
// that spells one differently is a map that is wrong.
constexpr const char* collision_layer_name = "collision";
constexpr const char* spawn_layer_name = "spawns";

// A level that cannot be built.
//
// Distinct from loaders::TmxFormatError, and the split is where the fix goes
// rather than where the throw is: a TmxFormatError means the document is not
// TMX this project can read, and is fixed in the file's syntax or in Tiled's
// export settings. A LevelError means a well-formed map says something the game
// layer cannot use, and is fixed by the person who drew the level.
class LevelError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

class Level
{
public:
    // `layers`, `object_layers`, `width`, `height`, `tile_width` and
    // `tile_height` are loaders::TmxMap's members of those names; `width` and
    // `height` are in tiles, the other two in pixels.
    //
    // `tile_width` and `tile_height` are THE MAP GRID'S cell size — TmxMap's
    // own members — and not the tileset's, which is a different number that
    // load_tmx reads separately and never reconciles. Tiled lets a tileset hold
    // tiles taller than the grid, which is how a 16x16 map draws a 16x32 tree,
    // and on such a map gfx::TileSet::tile_height() is the drawn size while the
    // collision grid's cells are still 16. Handing the tileset's number here
    // would put every solid cell in the wrong place and overstate the level by
    // the ratio.
    //
    // `source` labels error messages — the .tmx path, normally. A Level built
    // from layers that never came from a file has nothing to name and may leave
    // it empty, at the cost of a message that says which object but not which
    // map.
    //
    // Throws LevelError for anything in the refusal list above.
    Level(const std::vector<gfx::TileLayer>& layers,
          const std::vector<gfx::ObjectLayer>& object_layers, int width,
          int height, int tile_width, int tile_height,
          const std::string& source = std::string());

    // THERE IS DELIBERATELY NO CONSTRUCTOR FROM A gfx::TileMap, which is the
    // obvious convenience and cannot be made correct. A TileMap does not carry
    // the map grid's tile size — it keeps only the tileset's, and its own
    // pixel_width()/pixel_height() are built on that — so on the oversized-tile
    // map described above there is no way to recover the number this class
    // needs. A constructor that is silently wrong on a legal .tmx is worse than
    // one that does not exist, and the caller who has a TileMap also has the
    // TmxMap it was built from.

    int width() const { return _width; }   // in tiles
    int height() const { return _height; } // in tiles

    int tile_width() const { return _tile_width; }
    int tile_height() const { return _tile_height; }

    int pixel_width() const { return _width * _tile_width; }
    int pixel_height() const { return _height * _tile_height; }

    // The collision layer in the form game::collide takes.
    //
    // The returned grid refers to this Level's own copy of the layer, so it
    // must not outlive the Level — which is what TileGrid is already documented
    // as: a short-lived argument, not something to store.
    TileGrid collision() const
    {
        return TileGrid{_collision, _tile_width, _tile_height};
    }

    // Every object of the `spawns` layer, in the order the document wrote them.
    // Empty is legal; see above.
    const std::vector<gfx::MapObject>& spawns() const { return _spawns; }

private:
    int _width;
    int _height;
    int _tile_width;
    int _tile_height;

    gfx::TileLayer _collision;
    std::vector<gfx::MapObject> _spawns;
};

} // namespace game
