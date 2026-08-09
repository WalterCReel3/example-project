#include <loaders/sparrow.hpp>

#include <util/format.hpp>
#include <util/xml.hpp>

#include <utility>

namespace loaders
{

namespace
{

// The optional trim group. Present as a set: TexturePacker emits all four
// together, and frameX/frameY alone would describe an offset into a box whose
// size is unknown.
bool has_trim(const util::xml::Node& node)
{
    return node.has_attribute("frameX") || node.has_attribute("frameY") ||
           node.has_attribute("frameWidth") ||
           node.has_attribute("frameHeight");
}

gfx::AtlasFrame parse_subtexture(const util::xml::Node& node,
                                 const std::string& path)
{
    gfx::AtlasFrame frame;
    frame.id = node.require_attribute("name");

    frame.source.x = node.require_attribute_int("x");
    frame.source.y = node.require_attribute_int("y");
    frame.source.w = node.require_attribute_int("width");
    frame.source.h = node.require_attribute_int("height");

    if (gfx::renderer::is_empty(frame.source)) {
        throw SparrowFormatError(util::format(
            "%s: SubTexture \"%s\" has a %dx%d region; a frame with no area "
            "draws nothing",
            path.c_str(), frame.id.c_str(), frame.source.w, frame.source.h));
    }

    if (frame.source.x < 0 || frame.source.y < 0) {
        throw SparrowFormatError(util::format(
            "%s: SubTexture \"%s\" starts at (%d,%d), outside the "
            "sheet",
            path.c_str(), frame.id.c_str(), frame.source.x, frame.source.y));
    }

    // TexturePacker packs a frame sideways to save space and marks it here.
    // Undoing it needs SDL_RenderCopyEx and an angle, which Atlas::draw does
    // not do; drawing it upright instead would put a sideways sprite on screen
    // with nothing in the load to explain it.
    if (node.attribute_bool("rotated", false)) {
        throw SparrowFormatError(util::format(
            "%s: SubTexture \"%s\" is rotated, which is not supported. "
            "Re-export the atlas with rotation disabled",
            path.c_str(), frame.id.c_str()));
    }

    if (has_trim(node)) {
        frame.frame_width = node.require_attribute_int("frameWidth");
        frame.frame_height = node.require_attribute_int("frameHeight");

        // Negated on the way in: Sparrow measures from the trimmed region back
        // to the original's origin, and gfx::AtlasFrame stores an offset to
        // add. Absent means the crop took nothing off that edge.
        frame.trim_x = -node.attribute_int("frameX", 0);
        frame.trim_y = -node.attribute_int("frameY", 0);

        // The untrimmed frame cannot be smaller than what is left after
        // trimming it. This is the check that catches a hand-edited atlas or an
        // exporter writing the pair the wrong way round.
        if (frame.frame_width < frame.source.w ||
            frame.frame_height < frame.source.h) {
            throw SparrowFormatError(util::format(
                "%s: SubTexture \"%s\" is trimmed to %dx%d from a smaller "
                "%dx%d frame",
                path.c_str(), frame.id.c_str(), frame.source.w, frame.source.h,
                frame.frame_width, frame.frame_height));
        }
    }

    return frame;
}

} // namespace

SparrowAtlas load_sparrow(const std::string& path)
{
    SparrowAtlas atlas;

    // util::xml raises ParseError for a malformed document and for an absent or
    // unparseable required attribute. Both are format failures in this file's
    // terms, so they are re-thrown as one type with the path attached — the
    // ParseError message names the element and attribute but not the file it
    // came from.
    try {
        const util::xml::Document document = util::xml::load(path);

        const util::xml::Node root = document.child("TextureAtlas");
        if (!root) {
            throw SparrowFormatError(
                util::format("%s: no <TextureAtlas> element; this is not a "
                             "Sparrow atlas",
                             path.c_str()));
        }

        atlas.image_path = root.require_attribute("imagePath");

        for (const util::xml::Node& node : root.children("SubTexture")) {
            atlas.frames.push_back(parse_subtexture(node, path));
        }
    } catch (const util::xml::ParseError& error) {
        throw SparrowFormatError(
            util::format("%s: %s", path.c_str(), error.what()));
    }

    // An atlas naming a sheet and then declaring nothing in it is an authoring
    // mistake rather than an empty collection — every consumer of it would
    // resolve every name to npos, at which point the cause is much further
    // away than it is here.
    if (atlas.frames.empty()) {
        throw SparrowFormatError(
            util::format("%s: <TextureAtlas> declares no SubTexture elements",
                         path.c_str()));
    }

    return atlas;
}

} // namespace loaders
