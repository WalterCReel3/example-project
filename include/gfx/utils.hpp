#ifndef __GFX_UTILS_HPP__
#define __GFX_UTILS_HPP__

#include <string>
#include <algorithm>
#include <vector>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <math/vector.hpp>
#include <gfx/types.hpp>

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#define R_MASK 0xff000000
#define G_MASK 0x00ff0000
#define B_MASK 0x0000ff00
#define A_MASK 0x000000ff
#else
#define R_MASK 0x000000ff
#define G_MASK 0x0000ff00
#define B_MASK 0x00ff0000
#define A_MASK 0xff000000
#endif

namespace gfx
{

class Orientation : public math::Vector3
{
public:
    Orientation(float f1, float f2, float f3)
        : math::Vector3(f1, f2, f3) {}
    Orientation(const Orientation & rh)
        : math::Vector3(rh) { }

public:
    float yaw()   const
    {
        return x;
    }
    float pitch() const
    {
        return y;
    }
    float roll()  const
    {
        return z;
    }

    void setYaw(float v)
    {
        x = v;
    }
    void setPitch(float v)
    {
        y = v;
    }
    void setRoll(float v)
    {
        z = v;
    }
};

class Pov
{
public:
    Pov()
        : orientation(0.0f, 0.0f, 0.0f),
          position(0.0f, 0.0f, 0.0f) {}
    Pov(const Pov & rh)
        : orientation(rh.orientation),
          position(rh.position) { }
    Pov& operator=(const Pov & rh)
    {
        orientation = rh.orientation;
        position = rh.position;
        return *this;
    }

public:
    Orientation orientation;
    math::Vector3 position;
};

void render_surface(SDL_Surface *surface, SDL_Rect *rect);
void render_text(char *str, TTF_Font *font, SDL_Color color, SDL_Rect *rect);

} // namespace gfx

// vim: set sts=2 sw=2 expandtab:
#endif
