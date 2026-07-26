#pragma once

#include <vector>
#include <string>

class SDL_Surface;

namespace gfx
{

// An aggregate: all members are public, there is no invariant to maintain, and
// copy/move/destroy are all correct by default for a string and six ints.
//
//     SpritesheetFrame frame {name, ox, oy, x, y, w, h};
struct SpritesheetFrame {
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

    explicit Spritesheet(SDL_Surface* source_image);
    ~Spritesheet();

    Spritesheet(const Spritesheet&) = delete;
    Spritesheet& operator=(const Spritesheet&) = delete;

    void add_frame(const SpritesheetFrame& frame);
    FrameIterator begin();
    FrameIterator end();
    void set_surface(SDL_Surface* source_image);

private:
    // std::string image_path;
    SDL_Surface* _sheet;
    Frames _frames;
};

} // namespace gfx
