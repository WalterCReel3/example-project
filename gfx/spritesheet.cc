#include <gfx/spritesheet.hpp>
#include <SDL.h>
#include <SDL_image.h>

namespace gfx
{

Spritesheet::Spritesheet(SDL_Surface* source_image)
    : _sheet(source_image)
    , _frames()
{
}

void Spritesheet::add_frame(const SpritesheetFrame& frame)
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
