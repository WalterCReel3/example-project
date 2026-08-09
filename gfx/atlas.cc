#include <gfx/atlas.hpp>

#include <algorithm>

namespace gfx
{

Atlas::Atlas(renderer::Texture sheet, Frames frames)
    : _sheet(std::move(sheet))
    , _frames(std::move(frames))
{
}

Atlas::Index Atlas::find(const std::string& id) const
{
    const Frames::const_iterator found =
        std::find_if(_frames.begin(), _frames.end(),
                     [&id](const AtlasFrame& frame) { return frame.id == id; });

    if (found == _frames.end()) {
        return npos;
    }
    return static_cast<Index>(std::distance(_frames.begin(), found));
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
                                frame.source.w * scale,
                                frame.source.h * scale};

    context.draw(_sheet, &frame.source, &dst);
}

bool Atlas::contains_frames() const
{
    const int sheet_width = _sheet.width();
    const int sheet_height = _sheet.height();

    return std::all_of(
        _frames.begin(), _frames.end(),
        [sheet_width, sheet_height](const AtlasFrame& frame) {
            return frame.source.x >= 0 && frame.source.y >= 0 &&
                   frame.source.x + frame.source.w <= sheet_width &&
                   frame.source.y + frame.source.h <= sheet_height;
        });
}

} // namespace gfx
