#include <gfx/atlas.hpp>

#include <algorithm>

#include <util/algorithm.hpp>

namespace gfx
{

Atlas::Atlas(renderer::Texture sheet, Frames frames)
    : _sheet(std::move(sheet))
    , _frames(std::move(frames))
{
}

Atlas::Index Atlas::find(const std::string& id) const
{
    return util::index_of(
        _frames, [&id](const AtlasFrame& frame) { return frame.id == id; });
}

void Atlas::draw(renderer::Context& context, Index index, int x, int y,
                 int scale) const
{
    if (scale < 1) {
        return;
    }

    const AtlasFrame& frame = _frames[index];
    const renderer::Rect dst = {x + frame.trim_x * scale,
                                y + frame.trim_y * scale,
                                frame.source.w * scale, frame.source.h * scale};

    context.draw(_sheet, &frame.source, &dst);
}

bool Atlas::contains_frames() const
{
    const renderer::Rect sheet =
        renderer::bounds_of(_sheet.width(), _sheet.height());

    return std::all_of(_frames.begin(), _frames.end(),
                       [&sheet](const AtlasFrame& frame) {
                           return renderer::contains(sheet, frame.source);
                       });
}

} // namespace gfx
