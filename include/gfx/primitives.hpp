#ifndef __MATH_PRIMITIVES_HPP
#define __MATH_PRIMITIVES_HPP

#include <utility>

class RectSize : std::pair<int, int>
{
public:
    int width()
    {
        return this->first;
    }
    int height()
    {
        return this->second;
    }
    void set_width(int w)
    {
        this->first = w;
    }
    void set_height(int h)
    {
        this->second = h;
    }
};

template<typename T>
class Coords2 : std::pair<T, T>
{
public:
    int x()
    {
        return this->first;
    }
    int y()
    {
        return this->second;
    }
    void set_x(int x)
    {
        this->first = x;
    }
    void set_y(int y)
    {
        this->second = y;
    }
};

#endif
