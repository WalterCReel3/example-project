#pragma once

#include <string>
#include <vector>

#include <gfx/renderer/context.hpp>
#include <gfx/renderer/texture.hpp>

//============================================================================
//
// Tilesets and tile maps
//
//     gfx::TileSet tiles(std::move(texture), 16, 16, 25, 575);
//     gfx::TileMap map(std::move(tiles), std::move(layers), 64, 36);
//     map.draw(context, camera_x, camera_y);
//
// A *tileset* is the uniform grid an atlas is not: every cell is the same size,
// cells are addressed by number rather than by name, and the source rectangle
// is arithmetic. So it stores nothing per tile — a gfx::Atlas over 575 tiles
// would hold 575 std::strings to answer a question nobody asks of a tilemap.
// This is the reason "sheet"/"tileset" was kept free when gfx::Atlas was named.
//
// Tile ids here are **0-based indexes into the tileset**, not TMX global ids.
// A TMX file numbers tiles from a per-tileset `firstgid` and reserves 0 for
// "no tile"; loaders::load_tmx subtracts that once, so nothing downstream has
// to remember which convention it is holding. empty_tile marks a blank cell.
//
// Rendering is a straight per-tile blit of the visible rectangle. That is the
// thing planning/2026-07-25-software-2d-sprites-tiling/ says to *measure*
// rather than assume, and it is written this way deliberately: it is the
// baseline a cached or dirty-rectangle strategy would have to beat, and there
// is no point building the cache before the number exists.
//
//============================================================================
namespace gfx
{

// A cell with no tile in it.
const int empty_tile = -1;

// One tile layer: a rectangle of tile ids in row-major order.
struct TileLayer {
    std::string name;
    int width = 0;
    int height = 0;

    // width * height entries, each a tileset index or empty_tile.
    std::vector<int> tiles;

    bool visible = true;

    // Tiled writes these per layer and a map that uses them looks wrong
    // without them. Pixels, added to the draw position.
    int offset_x = 0;
    int offset_y = 0;

    // Row-major, unchecked. at() is the checked form.
    int operator()(int x, int y) const { return tiles[y * width + x]; }

    // empty_tile for anything outside the layer, so a caller iterating a
    // viewport does not have to clamp before asking.
    int at(int x, int y) const
    {
        if (x < 0 || y < 0 || x >= width || y >= height) {
            return empty_tile;
        }
        return tiles[static_cast<std::size_t>(y) * width + x];
    }
};

// A Tiled object. Kept because Tiled writes object layers whether or not
// anything reads them, and a spawn point is the first thing a game wants out of
// a map.
struct MapObject {
    std::string name;
    std::string type;
    int id = 0;

    // Pixels. Tiled measures a point or rectangle object from the map origin.
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
};

struct ObjectLayer {
    std::string name;
    std::vector<MapObject> objects;
};

// A uniform grid over one texture.
class TileSet
{
public:
    // `columns` and `count` describe the grid; margin and spacing are Tiled's,
    // and are the gap before the first tile and between adjacent ones.
    TileSet(renderer::Texture sheet, int tile_width, int tile_height,
            int columns, int count, int margin = 0, int spacing = 0);

    // Rule of zero, like Atlas: Texture is move-only, so this is movable and
    // not copyable without declaring anything.

    int tile_width() const { return _tile_width; }
    int tile_height() const { return _tile_height; }
    int columns() const { return _columns; }
    int count() const { return _count; }

    const renderer::Texture& texture() const { return _sheet; }

    bool contains(int tile) const { return tile >= 0 && tile < _count; }

    // Where tile `tile` lives in the sheet. Arithmetic, so there is no table
    // and no lookup; undefined for a tile the set does not contain, which
    // contains() is for.
    renderer::Rect source(int tile) const;

    // Whether every tile the grid claims actually lies inside the texture. The
    // tileset equivalent of Atlas::contains_frames, and the same failure it
    // catches: a .tsx whose tilecount outruns its image draws garbage rather
    // than failing.
    bool fits_texture() const;

private:
    renderer::Texture _sheet;
    int _tile_width;
    int _tile_height;
    int _columns;
    int _count;
    int _margin;
    int _spacing;
};

class TileMap
{
public:
    TileMap(TileSet tiles, std::vector<TileLayer> layers, int width,
            int height);

    int width() const { return _width; }   // in tiles
    int height() const { return _height; } // in tiles

    int pixel_width() const { return _width * _tiles.tile_width(); }
    int pixel_height() const { return _height * _tiles.tile_height(); }

    const TileSet& tileset() const { return _tiles; }

    const std::vector<TileLayer>& layers() const { return _layers; }
    std::vector<TileLayer>& layers() { return _layers; }

    // Layer index by name, or npos.
    typedef std::vector<TileLayer>::size_type Index;
    static constexpr Index npos = static_cast<Index>(-1);
    Index find(const std::string& name) const;

    // Object layers ride along: the map owns them, nothing draws them.
    const std::vector<ObjectLayer>& object_layers() const
    {
        return _object_layers;
    }
    void set_object_layers(std::vector<ObjectLayer> layers);

    // Draws every visible layer, in order, with the map scrolled so that map
    // pixel (camera_x, camera_y) sits at the top-left of the render target.
    //
    // Only the tiles that intersect the target are visited — the map is
    // deliberately larger than any target panel, and drawing all of it would
    // measure the wrong thing. Returns the number of tiles actually blitted,
    // which is what a fill-rate run wants to report.
    int draw(renderer::Context& context, int camera_x, int camera_y,
             int scale = 1) const;

    // One layer, same terms. Useful for drawing sprites between layers.
    int draw_layer(renderer::Context& context, const TileLayer& layer,
                   int camera_x, int camera_y, int scale = 1) const;

private:
    TileSet _tiles;
    std::vector<TileLayer> _layers;
    std::vector<ObjectLayer> _object_layers;
    int _width;
    int _height;
};

} // namespace gfx
