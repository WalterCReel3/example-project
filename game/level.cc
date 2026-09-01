#include <game/level.hpp>

#include <util/algorithm.hpp>
#include <util/format.hpp>

#include <cstddef>
#include <limits>

namespace game
{

namespace
{

// "path.tmx: " when there is a source to name, nothing when there is not.
std::string prefix(const std::string& source)
{
    if (source.empty()) {
        return std::string();
    }
    return source + ": ";
}

// Whether the half-open span [lo, lo + extent) lies inside [0, limit), reading
// a zero or negative extent as the single point `lo`. See the rule in the
// header: the degenerate case cannot be folded into the general one, because
// an empty span is inside every rectangle.
bool span_inside(double lo, double extent, int limit)
{
    if (!(lo >= 0.0)) {
        return false;
    }

    const double high = static_cast<double>(limit);
    if (extent > 0.0) {
        return lo + extent <= high;
    }
    return lo < high;
}

// id first, because `id="7"` is what a .tmx is greppable by and is the thing
// somebody with the editor closed actually needs.
std::string describe(const gfx::MapObject& object)
{
    return util::format("id=%d type=\"%s\" name=\"%s\" at (%g, %g) %gx%g",
                        object.id, object.type.c_str(), object.name.c_str(),
                        object.x, object.y, object.width, object.height);
}

} // namespace

Level::Level(const std::vector<gfx::TileLayer>& layers,
             const std::vector<gfx::ObjectLayer>& object_layers, int width,
             int height, int tile_width, int tile_height,
             const std::string& source)
    : _width(width)
    , _height(height)
    , _tile_width(tile_width)
    , _tile_height(tile_height)
{
    if (!util::all_positive(width, height, tile_width, tile_height)) {
        throw LevelError(util::format(
            "%sa %dx%d map of %dx%d pixel tiles has no extent",
            prefix(source).c_str(), width, height, tile_width, tile_height));
    }

    // pixel_width() and pixel_height() are int products of two unbounded int
    // factors, and the spawn rule below is stated against them, so a map whose
    // pixel extent does not fit in an int has to be refused before anything
    // reads one. Written as a division because the multiplication that would
    // detect it is the overflow.
    const int int_max = std::numeric_limits<int>::max();
    if (width > int_max / tile_width || height > int_max / tile_height) {
        throw LevelError(util::format(
            "%sa %dx%d map of %dx%d pixel tiles is more than %d pixels on a "
            "side, which is wider than this level can measure",
            prefix(source).c_str(), width, height, tile_width, tile_height,
            int_max));
    }

    const std::size_t collision_index =
        util::index_of(layers, [](const gfx::TileLayer& layer) {
            return layer.name == collision_layer_name;
        });
    if (collision_index == util::npos) {
        throw LevelError(util::format(
            "%sno tile layer named \"%s\", so nothing in this level is solid",
            prefix(source).c_str(), collision_layer_name));
    }
    _collision = layers[collision_index];

    if (_collision.width != width || _collision.height != height) {
        throw LevelError(util::format(
            "%stile layer \"%s\" is %dx%d but the map is %dx%d; everything "
            "outside the layer is solid, so the level would end there",
            prefix(source).c_str(), collision_layer_name, _collision.width,
            _collision.height, width, height));
    }

    // game::solid_at bounds-checks a cell against the layer's declared width
    // and height and then reads it through the unchecked
    // gfx::TileLayer::operator(), so a layer whose data is shorter than the
    // rectangle it declares is read past the end on the first query.
    const unsigned long long expected =
        static_cast<unsigned long long>(_collision.width) * _collision.height;
    if (_collision.tiles.size() != expected) {
        throw LevelError(util::format(
            "%stile layer \"%s\" is %dx%d but its data has %zu entries, not "
            "%llu; the cells past the end have nothing to read",
            prefix(source).c_str(), collision_layer_name, _collision.width,
            _collision.height, _collision.tiles.size(), expected));
    }

    if (_collision.offset_x != 0 || _collision.offset_y != 0) {
        throw LevelError(util::format(
            "%stile layer \"%s\" has a draw offset of (%d, %d); collision "
            "reads the grid without one, so the solid cells would not be "
            "where the layer draws",
            prefix(source).c_str(), collision_layer_name, _collision.offset_x,
            _collision.offset_y));
    }

    const std::size_t spawn_index =
        util::index_of(object_layers, [](const gfx::ObjectLayer& layer) {
            return layer.name == spawn_layer_name;
        });
    if (spawn_index == util::npos) {
        throw LevelError(util::format(
            "%sno object layer named \"%s\", so this level places nothing",
            prefix(source).c_str(), spawn_layer_name));
    }
    _spawns = object_layers[spawn_index].objects;

    for (const gfx::MapObject& object : _spawns) {
        if (span_inside(object.x, object.width, pixel_width()) &&
            span_inside(object.y, object.height, pixel_height())) {
            continue;
        }

        throw LevelError(util::format("%sspawn %s is outside the %dx%d pixel "
                                      "level",
                                      prefix(source).c_str(),
                                      describe(object).c_str(), pixel_width(),
                                      pixel_height()));
    }
}

} // namespace game
