#include <gfx/tilemap.hpp>

#include <algorithm>

#include <util/integer.hpp>

namespace gfx
{

TileSet::TileSet(renderer::Texture sheet, int tile_width, int tile_height,
                 int columns, int count, int margin, int spacing)
    : _sheet(std::move(sheet))
    , _tile_width(tile_width)
    , _tile_height(tile_height)
    , _columns(columns)
    , _count(count)
    , _margin(margin)
    , _spacing(spacing)
{
}

renderer::Rect TileSet::source(int tile) const
{
    const int column = tile % _columns;
    const int row = tile / _columns;

    return {_margin + column * (_tile_width + _spacing),
            _margin + row * (_tile_height + _spacing), _tile_width,
            _tile_height};
}

bool TileSet::fits_texture() const
{
    if (_count <= 0 || _columns <= 0) {
        return false;
    }

    // The last tile is the only one that can run off the sheet, since the grid
    // is monotonic in both axes.
    const renderer::Rect last = source(_count - 1);
    return last.x >= 0 && last.y >= 0 && last.x + last.w <= _sheet.width() &&
           last.y + last.h <= _sheet.height();
}

TileMap::TileMap(TileSet tiles, std::vector<TileLayer> layers, int width,
                 int height)
    : _tiles(std::move(tiles))
    , _layers(std::move(layers))
    , _width(width)
    , _height(height)
{
}

TileMap::Index TileMap::find(const std::string& name) const
{
    const std::vector<TileLayer>::const_iterator found =
        std::find_if(_layers.begin(), _layers.end(),
                     [&name](const TileLayer& l) { return l.name == name; });

    if (found == _layers.end()) {
        return npos;
    }
    return static_cast<Index>(std::distance(_layers.begin(), found));
}

void TileMap::set_object_layers(std::vector<ObjectLayer> layers)
{
    _object_layers = std::move(layers);
}

int TileMap::draw_layer(renderer::Context& context, const TileLayer& layer,
                        int camera_x, int camera_y, int scale) const
{
    if (!layer.visible || scale < 1) {
        return 0;
    }

    const int tile_w = _tiles.tile_width() * scale;
    const int tile_h = _tiles.tile_height() * scale;
    if (tile_w <= 0 || tile_h <= 0) {
        return 0;
    }

    // Where the layer's origin lands on screen, once the camera and the
    // layer's own offset are applied.
    const int origin_x = layer.offset_x * scale - camera_x;
    const int origin_y = layer.offset_y * scale - camera_y;

    // Only the cells that intersect the target. The map is deliberately larger
    // than any panel here, so walking all of it would measure the map rather
    // than the screen — and on the Miyoo Mini that difference is the whole
    // question.
    //
    // util::floor_div, not `/`: a camera past the map's left edge makes these
    // negative, and truncation toward zero reports cell 0 for every position in
    // the cell before it, which drops the partially visible first column. The
    // last index is the cell holding the target's final pixel, hence the -1 —
    // without it a cell starting exactly at the right edge is visited and then
    // clipped away.
    const int first_x = std::max(0, util::floor_div(-origin_x, tile_w));
    const int first_y = std::max(0, util::floor_div(-origin_y, tile_h));

    const int last_x =
        std::min(layer.width - 1,
                 util::floor_div(context.width() - 1 - origin_x, tile_w));
    const int last_y =
        std::min(layer.height - 1,
                 util::floor_div(context.height() - 1 - origin_y, tile_h));

    int drawn = 0;
    for (int y = first_y; y <= last_y; ++y) {
        for (int x = first_x; x <= last_x; ++x) {
            const int tile = layer(x, y);
            if (tile == empty_tile || !_tiles.contains(tile)) {
                continue;
            }

            const renderer::Rect src = _tiles.source(tile);
            const renderer::Rect dst = {origin_x + x * tile_w,
                                        origin_y + y * tile_h, tile_w, tile_h};

            context.draw(_tiles.texture(), &src, &dst);
            ++drawn;
        }
    }

    return drawn;
}

int TileMap::draw(renderer::Context& context, int camera_x, int camera_y,
                  int scale) const
{
    int drawn = 0;
    for (const TileLayer& layer : _layers) {
        drawn += draw_layer(context, layer, camera_x, camera_y, scale);
    }
    return drawn;
}

} // namespace gfx
