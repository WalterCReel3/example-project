#include <loaders/animations.hpp>

#include <util/format.hpp>
#include <util/logging.hpp>
#include <util/xml.hpp>

namespace loaders
{

namespace
{

AnimationDef parse_animation(const util::xml::Node& node,
                             const std::string& path)
{
    AnimationDef def;
    def.name = node.require_attribute("name");

    def.fps = node.attribute_double("fps", 12.0);
    if (def.fps <= 0.0) {
        throw AnimationFormatError(
            util::format("%s: animation \"%s\" has fps=\"%g\"; an animation "
                         "cannot advance at that rate",
                         path.c_str(), def.name.c_str(), def.fps));
    }

    // An unrecognised mode falls back to repeat rather than throwing, but says
    // so: a demo should still start, and a silent fallback is how a typo
    // survives to the device.
    const std::string loop = node.attribute("loop", "repeat");
    def.loop = gfx::loop_from_string(loop, gfx::Loop::Repeat);
    if (loop != gfx::to_string(def.loop)) {
        util::log_warning("%s: animation \"%s\" has unknown loop \"%s\"; "
                          "using \"repeat\"",
                          path.c_str(), def.name.c_str(), loop.c_str());
    }

    for (const util::xml::Node& frame : node.children("Frame")) {
        def.frame_ids.push_back(frame.require_attribute("id"));
    }

    const int expected = node.attribute_int("frames", 0);
    if (expected < 0) {
        throw AnimationFormatError(
            util::format("%s: animation \"%s\" claims %d frames", path.c_str(),
                         def.name.c_str(), expected));
    }
    def.expected_frames = static_cast<std::size_t>(expected);

    return def;
}

} // namespace

AnimationDefs load_animations(const std::string& path)
{
    AnimationDefs defs;

    try {
        const util::xml::Document document = util::xml::load(path);

        const util::xml::Node root = document.child("Animations");
        if (!root) {
            throw AnimationFormatError(
                util::format("%s: no <Animations> element", path.c_str()));
        }

        defs.atlas_path = root.attribute("atlas", "");

        for (const util::xml::Node& node : root.children("Animation")) {
            defs.animations.push_back(parse_animation(node, path));
        }
    } catch (const util::xml::ParseError& error) {
        throw AnimationFormatError(
            util::format("%s: %s", path.c_str(), error.what()));
    }

    if (defs.animations.empty()) {
        throw AnimationFormatError(util::format(
            "%s: <Animations> declares no <Animation> elements", path.c_str()));
    }

    return defs;
}

gfx::AnimationSet resolve_animations(const gfx::Atlas& atlas,
                                     const AnimationDefs& defs)
{
    gfx::AnimationSet set;

    for (const AnimationDef& def : defs.animations) {
        gfx::Animation animation;

        if (def.frame_ids.empty()) {
            animation =
                gfx::animation_from_prefix(atlas, def.name, def.fps, def.loop);
        } else {
            animation.name = def.name;
            animation.seconds_per_frame = 1.0 / def.fps;
            animation.loop = def.loop;

            for (const std::string& id : def.frame_ids) {
                const gfx::Atlas::Index index = atlas.find(id);
                if (index == gfx::Atlas::npos) {
                    throw AnimationFormatError(util::format(
                        "animation \"%s\" names frame \"%s\", which the atlas "
                        "does not have",
                        def.name.c_str(), id.c_str()));
                }
                animation.frames.push_back(index);
            }
        }

        if (animation.frames.empty()) {
            throw AnimationFormatError(util::format(
                "animation \"%s\" resolves to no frames; the atlas has nothing "
                "named \"%s.NNN\"",
                def.name.c_str(), def.name.c_str()));
        }

        // The count the document carries is generated, so a disagreement means
        // the atlas was regenerated without its sidecar or the other way round.
        // Not fatal — the frames that are there still play — but the drift is
        // worth saying out loud, because the symptom is an animation quietly
        // missing its tail.
        if (def.expected_frames != 0 &&
            def.expected_frames != animation.frames.size()) {
            util::log_warning(
                "animation \"%s\": document says %zu frames, atlas has %zu",
                def.name.c_str(), def.expected_frames, animation.frames.size());
        }

        set.add(std::move(animation));
    }

    return set;
}

gfx::AnimationSet load_animation_set(const gfx::Atlas& atlas,
                                     const std::string& path)
{
    return resolve_animations(atlas, load_animations(path));
}

} // namespace loaders
