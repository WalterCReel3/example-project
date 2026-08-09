#include <loaders/tmx.hpp>

#include <util/ascii.hpp>
#include <util/algorithm.hpp>
#include <util/format.hpp>
#include <util/number.hpp>
#include <util/xml.hpp>

#include <cstdlib>

namespace loaders
{

namespace
{

// Tiled sets the top three bits of a gid for horizontal, vertical and diagonal
// flips. Nothing here draws a flipped tile, so they are masked off: the
// alternative is a tile index in the billions, which reads as a blank cell and
// looks like missing map data rather than an unsupported feature.
const unsigned int gid_flags = 0xE0000000u;

// The directory part of a path, with its separator, or "" for a bare filename.
// An external tileset is named relative to the map that references it.
std::string directory_of(const std::string& path)
{
    const std::string::size_type slash = path.find_last_of("/\\");
    if (slash == std::string::npos) {
        return std::string();
    }
    return path.substr(0, slash + 1);
}

void read_tileset_body(const util::xml::Node& node, const std::string& path,
                       TmxTileset& tileset)
{
    tileset.name = node.attribute("name", "");
    tileset.tile_width = node.require_attribute_int("tilewidth");
    tileset.tile_height = node.require_attribute_int("tileheight");
    tileset.columns = node.attribute_int("columns", 0);
    tileset.tile_count = node.attribute_int("tilecount", 0);
    tileset.margin = node.attribute_int("margin", 0);
    tileset.spacing = node.attribute_int("spacing", 0);

    if (!util::all_positive(tileset.tile_width, tileset.tile_height)) {
        throw TmxFormatError(util::format("%s: tileset has a %dx%d tile size",
                                          path.c_str(), tileset.tile_width,
                                          tileset.tile_height));
    }

    const util::xml::Node image = node.child("image");
    if (!image) {
        throw TmxFormatError(util::format(
            "%s: tileset \"%s\" has no <image>; a collection-of-images tileset "
            "is not supported",
            path.c_str(), tileset.name.c_str()));
    }

    tileset.image = image.require_attribute("source");
    tileset.image_width = image.attribute_int("width", 0);
    tileset.image_height = image.attribute_int("height", 0);

    if (tileset.columns <= 0) {
        throw TmxFormatError(
            util::format("%s: tileset \"%s\" declares %d columns", path.c_str(),
                         tileset.name.c_str(), tileset.columns));
    }
    if (tileset.tile_count <= 0) {
        throw TmxFormatError(
            util::format("%s: tileset \"%s\" declares %d tiles", path.c_str(),
                         tileset.name.c_str(), tileset.tile_count));
    }
}

TmxTileset read_tileset(const util::xml::Node& node, const std::string& path)
{
    TmxTileset tileset;
    tileset.first_gid = node.attribute_int("firstgid", 1);

    const std::string source = node.attribute("source", "");
    if (source.empty()) {
        // Embedded: the map element carries the whole definition.
        read_tileset_body(node, path, tileset);
        return tileset;
    }

    // External. Tiled names it relative to the map, so that is where it is
    // looked for — not relative to the working directory, which is what makes
    // this work from an installed bundle.
    const std::string tsx_path = directory_of(path) + source;

    util::xml::Document document = util::xml::load(tsx_path);
    const util::xml::Node root = document.child("tileset");
    if (!root) {
        throw TmxFormatError(
            util::format("%s: no <tileset> element", tsx_path.c_str()));
    }

    read_tileset_body(root, tsx_path, tileset);

    // The image is named relative to the .tsx, which may sit in a different
    // directory from the .tmx. Fold that in now so the caller has one path to
    // resolve rather than two to combine.
    tileset.image = directory_of(source) + tileset.image;

    return tileset;
}

// "1,2,0,\n3,4,0" -> tileset indexes, with firstgid subtracted and 0 mapped to
// gfx::empty_tile. Hand-rolled rather than run through the tokenizer: a
// 64x36 layer is 2304 fields and this is the hot path of loading a map.
void parse_csv(const std::string& text, int first_gid, const std::string& path,
               const std::string& layer_name, std::vector<int>& out)
{
    const char* p = text.c_str();
    const char* const end = p + text.size();

    while (p != end) {
        // util::ascii, not a hand-rolled character class: the predicate
        // exists, is tested, and does not depend on the locale the way
        // <cctype> does.
        while (p != end && (util::ascii_is_whitespace(*p) || *p == ',')) {
            ++p;
        }
        if (p == end) {
            break;
        }

        char* stop = nullptr;
        const unsigned long value = std::strtoul(p, &stop, 10);
        if (stop == p) {
            throw TmxFormatError(util::format(
                "%s: layer \"%s\" has a non-numeric entry in its CSV data",
                path.c_str(), layer_name.c_str()));
        }
        p = stop;

        const unsigned int gid = static_cast<unsigned int>(value) & ~gid_flags;
        if (gid == 0) {
            out.push_back(gfx::empty_tile);
        } else {
            out.push_back(static_cast<int>(gid) - first_gid);
        }
    }
}

gfx::TileLayer read_layer(const util::xml::Node& node, const std::string& path,
                          int first_gid)
{
    gfx::TileLayer layer;
    layer.name = node.attribute("name", "");
    layer.width = node.require_attribute_int("width");
    layer.height = node.require_attribute_int("height");
    layer.visible = node.attribute_int("visible", 1) != 0;
    layer.offset_x = static_cast<int>(node.attribute_double("offsetx", 0.0));
    layer.offset_y = static_cast<int>(node.attribute_double("offsety", 0.0));

    if (!util::all_positive(layer.width, layer.height)) {
        throw TmxFormatError(util::format("%s: layer \"%s\" is %dx%d",
                                          path.c_str(), layer.name.c_str(),
                                          layer.width, layer.height));
    }

    const util::xml::Node data = node.child("data");
    if (!data) {
        throw TmxFormatError(util::format("%s: layer \"%s\" has no <data>",
                                          path.c_str(), layer.name.c_str()));
    }

    const std::string encoding = data.attribute("encoding", "");
    if (encoding != "csv") {
        throw TmxFormatError(util::format(
            "%s: layer \"%s\" uses encoding \"%s\"; only csv is supported. In "
            "Tiled, Map > Map Properties > Tile Layer Format > CSV",
            path.c_str(), layer.name.c_str(),
            encoding.empty() ? "xml" : encoding.c_str()));
    }

    layer.tiles.reserve(static_cast<std::size_t>(layer.width) * layer.height);
    parse_csv(data.text(), first_gid, path, layer.name, layer.tiles);

    const std::size_t expected =
        static_cast<std::size_t>(layer.width) * layer.height;
    if (layer.tiles.size() != expected) {
        throw TmxFormatError(util::format(
            "%s: layer \"%s\" is %dx%d but its data has %zu entries, not %zu",
            path.c_str(), layer.name.c_str(), layer.width, layer.height,
            layer.tiles.size(), expected));
    }

    return layer;
}

gfx::ObjectLayer read_object_layer(const util::xml::Node& node)
{
    gfx::ObjectLayer group;
    group.name = node.attribute("name", "");

    for (const util::xml::Node& node_object : node.children("object")) {
        gfx::MapObject object;
        object.id = node_object.attribute_int("id", 0);
        object.name = node_object.attribute("name", "");
        object.type = node_object.attribute("type", "");
        object.x = node_object.attribute_double("x", 0.0);
        object.y = node_object.attribute_double("y", 0.0);
        object.width = node_object.attribute_double("width", 0.0);
        object.height = node_object.attribute_double("height", 0.0);
        group.objects.push_back(object);
    }

    return group;
}

} // namespace

TmxMap load_tmx(const std::string& path)
{
    TmxMap map;

    try {
        const util::xml::Document document = util::xml::load(path);

        const util::xml::Node root = document.child("map");
        if (!root) {
            throw TmxFormatError(
                util::format("%s: no <map> element", path.c_str()));
        }

        const std::string orientation =
            root.attribute("orientation", "orthogonal");
        if (orientation != "orthogonal") {
            throw TmxFormatError(util::format(
                "%s: orientation is \"%s\"; only orthogonal is supported",
                path.c_str(), orientation.c_str()));
        }

        if (root.attribute_int("infinite", 0) != 0) {
            throw TmxFormatError(util::format(
                "%s: the map is infinite, which stores layers as chunks. In "
                "Tiled, Map > Map Properties > Infinite > off",
                path.c_str()));
        }

        map.width = root.require_attribute_int("width");
        map.height = root.require_attribute_int("height");
        map.tile_width = root.require_attribute_int("tilewidth");
        map.tile_height = root.require_attribute_int("tileheight");

        if (!util::all_positive(map.width, map.height, map.tile_width,
                                map.tile_height)) {
            throw TmxFormatError(util::format(
                "%s: map is %dx%d tiles of %dx%d", path.c_str(), map.width,
                map.height, map.tile_width, map.tile_height));
        }

        int tilesets = 0;
        for (const util::xml::Node& node : root.children("tileset")) {
            ++tilesets;
            if (tilesets == 1) {
                map.tileset = read_tileset(node, path);
            }
        }

        if (tilesets == 0) {
            throw TmxFormatError(util::format(
                "%s: the map references no tileset", path.c_str()));
        }
        if (tilesets > 1) {
            throw TmxFormatError(util::format(
                "%s: the map references %d tilesets; only one is supported, "
                "because a gid would have to select between textures per tile",
                path.c_str(), tilesets));
        }

        for (const util::xml::Node& node : root.children("layer")) {
            map.layers.push_back(read_layer(node, path, map.tileset.first_gid));
        }

        for (const util::xml::Node& node : root.children("objectgroup")) {
            map.object_layers.push_back(read_object_layer(node));
        }
    } catch (const util::xml::ParseError& error) {
        throw TmxFormatError(
            util::format("%s: %s", path.c_str(), error.what()));
    }

    if (map.layers.empty()) {
        throw TmxFormatError(
            util::format("%s: the map has no tile layers", path.c_str()));
    }

    return map;
}

} // namespace loaders
