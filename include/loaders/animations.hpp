#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include <gfx/animation.hpp>

//============================================================================
//
// Animation definitions
//
//     const gfx::AnimationSet set =
//         loaders::load_animation_set(atlas, rig::asset_path("foxy.anim.xml"));
//     sprite.play(set[set.find("run")]);
//
// The sidecar a Sparrow atlas needs and does not have. Sparrow describes where
// the frames are and nothing about how they play, so frame rate, loop mode and
// any non-obvious frame order live here:
//
//     <Animations atlas="foxy.xml">
//       <Animation name="run"  fps="12" loop="repeat" frames="6"/>
//       <Animation name="idle" fps="6"  loop="repeat" frames="4"/>
//       <Animation name="hop"  fps="8"  loop="once">
//         <Frame id="jump.000"/>
//         <Frame id="jump.001"/>
//         <Frame id="jump.000"/>
//       </Animation>
//     </Animations>
//
// XML rather than JSON because util::xml already exists and is tested, the
// atlas beside it is XML, and so is the TMX still to come. There is no
// util::json facade yet, and CLAUDE.md requires one before nlohmann appears
// outside a test.
//
// An <Animation> with no <Frame> children takes every atlas frame named
// "<name>.NNN", which is the common case and what tools/pack_atlas.py writes.
// Explicit <Frame> children override that, and are how frame reuse and holds
// get expressed -- a repeated id holds that frame for another tick.
//
// The generated `frames` attribute is a count, not a list: the loader checks
// what it derived against it and complains if they disagree, which is how an
// atlas regenerated without its sidecar gets noticed.
//
//============================================================================
namespace loaders
{

class AnimationFormatError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

// One entry, before its frame ids are resolved against an atlas. Plain data, so
// the parser is testable without a Context -- the same split load_sparrow
// makes.
struct AnimationDef {
    std::string name;
    double fps = 12.0;
    gfx::Loop loop = gfx::Loop::Repeat;

    // Empty means "derive from the name", which is the common case.
    std::vector<std::string> frame_ids;

    // The count the document claims, or 0 if it did not say. Cross-checked
    // against what resolving actually produces.
    std::size_t expected_frames = 0;
};

struct AnimationDefs {
    // The atlas the document names, as authored. Resolving it is the caller's
    // job, exactly as with SparrowAtlas::image_path.
    std::string atlas_path;
    std::vector<AnimationDef> animations;
};

// Parses the document. Throws AnimationFormatError for a malformed one and the
// matching posix:: exception for a file that cannot be read.
AnimationDefs load_animations(const std::string& path);

// Resolves `defs` against `atlas` into playable definitions.
//
// Throws AnimationFormatError when an explicit <Frame id> names a frame the
// atlas does not have, or when an animation resolves to no frames at all --
// both mean the sidecar and the atlas have drifted apart, and the symptom
// otherwise is a sprite that silently draws nothing.
gfx::AnimationSet resolve_animations(const gfx::Atlas& atlas,
                                     const AnimationDefs& defs);

// load_animations followed by resolve_animations, which is what a caller that
// already has its atlas wants.
gfx::AnimationSet load_animation_set(const gfx::Atlas& atlas,
                                     const std::string& path);

} // namespace loaders
