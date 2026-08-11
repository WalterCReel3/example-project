#pragma once

#include <stdexcept>
#include <string>

#include <gfx/atlas.hpp>

//============================================================================
//
// Sparrow / Starling texture atlases
//
//     const loaders::SparrowAtlas parsed =
//         loaders::load_sparrow(rig::asset_path("jetpackdude.xml"));
//
//     SDL_Surface* image =
//         loaders::load_image(rig::asset_path(parsed.image_path));
//     gfx::Atlas atlas(gfx::renderer::Texture(context, image),
//                      std::move(parsed.frames));
//     SDL_FreeSurface(image);
//
// A parser, and only that. It produces the frame table and the sheet filename
// the document declares — no Context, no texture, no image load — which is what
// lets tests/test_sparrow.cc run headlessly and under qemu, the same reason
// loaders/obj.cc was changed to produce a plain gfx::Mesh.
//
// The format, as TexturePacker and Adobe Animate emit it:
//
//     <TextureAtlas imagePath="sheet.png">
//       <SubTexture name="run.000" x="0" y="0" width="24" height="34"/>
//       <SubTexture name="run.001" x="24" y="0" width="20" height="30"
//                   frameX="-2" frameY="-4" frameWidth="24" frameHeight="34"/>
//     </TextureAtlas>
//
// `x/y/width/height` is the region in the sheet. The optional `frame*` group
// describes a trimmed frame: the region is what is left after transparent
// margins were cropped, `frameWidth/frameHeight` is the size before cropping,
// and `frameX/frameY` locate the region within it — negative, because they
// point from the cropped image back to the original's origin. gfx::AtlasFrame
// stores that negated, as an offset to add.
//
// Not read: `pivotX`/`pivotY`, which Starling uses for a rotation centre and
// which nothing here has a use for yet. Not supported: `rotated="true"`, which
// TexturePacker emits when it packs a frame sideways — SDL_RenderCopy can undo
// it with an angle, but silently drawing it upright would be wrong, so it is
// rejected rather than ignored.
//
// See planning/2026-07-25-software-2d-sprites-tiling/.
//
//============================================================================
namespace loaders
{

// Errors are types — see include/posix/errors.hpp. Every format failure in this
// file arrives as this one, including those util::xml raises underneath, so a
// caller catches one type rather than two.
class SparrowFormatError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

// What the document declares. `image_path` is returned exactly as authored and
// is relative to the document, so resolving it is the caller's job — see the
// note on asset addressing in the planning snapshot.
struct SparrowAtlas {
    std::string image_path;
    gfx::Atlas::Frames frames;
};

// Throws SparrowFormatError for a malformed document, and the matching posix::
// exception for a file that cannot be read.
//
// Rejected rather than mis-parsed, on the same reasoning as load_obj: a frame
// with a zero or negative size is an invisible sprite, and a frame at a
// negative origin is a blit that clamps. Both look like rendering bugs and
// neither is one. So are a document whose root is not <TextureAtlas>, a missing
// `imagePath`, a `frameX` with no `frameWidth` to offset within, and an atlas
// that declares no frames at all — the last because every name a consumer looks
// up would resolve to npos, at which point the cause is a long way from here.
SparrowAtlas load_sparrow(const std::string& path);

} // namespace loaders
