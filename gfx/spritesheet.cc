#include <gfx/spritesheet.hpp>
#include <SDL.h>
#include <SDL_image.h>

namespace gfx
{

SpritesheetFrame::SpritesheetFrame(std::string& id, int offset_x,
                                   int offset_y,
                                   int x, int y, int width, int height)
    : id(id)
    , offset_x(offset_x)
    , offset_y(offset_y)
    , x(x)
    , y(y)
    , width(width)
    , height(height)
{
}

SpritesheetFrame::SpritesheetFrame(const SpritesheetFrame& rh)
{
    assign(rh);
}

SpritesheetFrame& SpritesheetFrame::operator=(const SpritesheetFrame &rh)
{
    assign(rh);
    return *this;
}

void SpritesheetFrame::assign(const SpritesheetFrame &rh)
{
    id = rh.id;
    offset_x = rh.offset_x;
    offset_y = rh.offset_y;
    x = rh.x;
    y = rh.y;
    width = rh.width;
    height = rh.height;
}

SpritesheetFrame::~SpritesheetFrame()
{
}

Spritesheet::Spritesheet(SDL_Surface* source_image)
    : _sheet(source_image)
    , _frames()
{
}

void Spritesheet::add_frame(const SpritesheetFrame &frame)
{
    _frames.push_back(frame);
}

Spritesheet::FrameIterator Spritesheet::begin()
{
    return _frames.begin();
}

Spritesheet::FrameIterator Spritesheet::end()
{
    return _frames.end();
}

void Spritesheet::set_surface(SDL_Surface* source_image)
{
    _sheet = source_image;
}

Spritesheet::~Spritesheet()
{
}

} // namespace gfx
