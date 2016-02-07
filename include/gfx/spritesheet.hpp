#ifndef __GFX_SPRITESHEET_HPP__
#define __GFX_SPRITESHEET_HPP__

#include <vector>
#include <string>
#include <util/nocopy.hpp>

class SDL_Surface;

namespace gfx
{

class SpritesheetFrame
{
public:
    explicit SpritesheetFrame(std::string& id, int offset_x, int offset_y,
                              int x, int y, int width, int height);
    SpritesheetFrame(const SpritesheetFrame& rh);
    SpritesheetFrame& operator=(const SpritesheetFrame &rh);
    ~SpritesheetFrame();

    void assign(const SpritesheetFrame& rh);

public:
    std::string id;
    // Offset coordinates: These define where to start in the source
    // for the frame.
    int offset_x;
    int offset_y;
    // These dimensions define the actual Frame from the offset
    int x;
    int y;
    int width;
    int height;
};

class Spritesheet
{
public:
    typedef std::vector<SpritesheetFrame> Frames;
    typedef Frames::const_iterator FrameIterator;

    Spritesheet(SDL_Surface* source_image);
    ~Spritesheet();

private:
    DISALLOW_COPY_AND_ASSIGN(Spritesheet);

public:
    void add_frame(const SpritesheetFrame &frame);
    FrameIterator begin();
    FrameIterator end();
    void set_surface(SDL_Surface* source_image);

private:
    // std::string image_path;
    SDL_Surface* _sheet;
    Frames _frames;
};

} // namespace gfx

#endif
