#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include <gfx/tilemap.hpp>

//============================================================================
//
// Tiled TMX maps
//
//     const loaders::TmxMap map =
//     loaders::load_tmx(rig::asset_path("sunnyland.tmx"));
//
//     SDL_Surface* image =
//     loaders::load_image(rig::asset_path(map.tileset.image)); gfx::TileSet
//     tiles(gfx::renderer::Texture(context, image), ...); gfx::TileMap
//     drawable(std::move(tiles), map.layers, map.width, map.height);
//
// A parser, and only that — no Context, no texture, no image load, so
// tests/test_tmx.cc runs headless. Same split as load_sparrow and load_obj.
//
// Supported, because Tiled writes it by default:
//
//     orthogonal orientation, right-down render order, fixed size
//     an external <tileset source="x.tsx"> or an embedded one
//     <layer> with <data encoding="csv">
//     <objectgroup> with point and rectangle objects
//     per-layer offsetx/offsety and visible
//
// Rejected rather than mis-parsed:
//
//     - a non-orthogonal orientation (isometric, hexagonal). The tile-to-pixel
//       arithmetic is different and drawing it as orthogonal produces a mess
//       that looks like a corrupt map rather than an unsupported one.
//     - infinite="1". Tiled stores those as chunks, and reading the <data> as
//       one grid silently yields a fraction of the map.
//     - an encoding other than csv, including base64 with or without
//       compression. Decoding base64 is easy; zlib is a dependency the project
//       has deliberately not taken, so the honest thing is a message naming the
//       setting to change in Tiled. Recorded in
//       planning/2026-07-25-software-2d-sprites-tiling/.
//     - a layer whose data length does not match its width x height, which
//       otherwise reads as a map that is subtly the wrong shape.
//
// Global ids are converted to tileset indexes here. A TMX numbers tiles from a
// per-tileset `firstgid` and uses 0 for "no tile"; this subtracts the firstgid
// once so everything downstream sees a 0-based index or gfx::empty_tile. The
// high bits Tiled uses for flip flags are masked off — a flipped tile draws
// unflipped rather than as tile 2^30-something, which would just be a blank.
//
//============================================================================
namespace loaders
{

class TmxFormatError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

// A tileset as the map refers to it, with the .tsx already read if it was
// external. `image` is relative to the document that named it, exactly as
// SparrowAtlas::image_path is; resolving it is the caller's job.
struct TmxTileset {
    int first_gid = 1;
    std::string name;
    std::string image;

    int tile_width = 0;
    int tile_height = 0;
    int columns = 0;
    int tile_count = 0;
    int margin = 0;
    int spacing = 0;

    // The image's own size, as the document claims it. Worth carrying because
    // a .tsx that disagrees with its PNG is a real authoring failure and the
    // only place to notice it is where both are known.
    int image_width = 0;
    int image_height = 0;
};

struct TmxMap {
    int width = 0;  // tiles
    int height = 0; // tiles
    int tile_width = 0;
    int tile_height = 0;

    // Exactly one tileset is supported. A map with several is rejected: gid
    // ranges would have to be mapped per layer entry to different textures, and
    // gfx::TileMap holds one TileSet. Saying so beats drawing the first
    // tileset's art for every id.
    TmxTileset tileset;

    std::vector<gfx::TileLayer> layers;
    std::vector<gfx::ObjectLayer> object_layers;
};

// Throws TmxFormatError for a malformed or unsupported document, and the
// matching posix:: exception for a file that cannot be read. An external .tsx
// is resolved relative to the .tmx, which is how Tiled writes the reference.
TmxMap load_tmx(const std::string& path);

} // namespace loaders
