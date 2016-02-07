#ifndef __MATH_VECTOR_HPP__
#define __MATH_VECTOR_HPP__

namespace math
{

class Vector3
{
public:
    Vector3()
        : x(0)
        , y(0)
        , z(0) { }
    Vector3(float x1, float y1, float z1)
        : x(x1)
        , y(y1)
        , z(z1) { }
    Vector3(const Vector3 & rh)
        : x(rh.x)
        , y(rh.y)
        , z(rh.z) { }
    Vector3& operator=(const Vector3 & rh)
    {
        x = rh.x;
        y = rh.y;
        z = rh.z;
        return *this;
    }
    Vector3& operator+(const Vector3 & rh)
    {
        x += rh.x;
        y += rh.y;
        z += rh.z;
        return *this;
    }
    Vector3& operator*(float s)
    {
        x *= s;
        y *= s;
        z *= s;
        return *this;
    }

public:
    float x;
    float y;
    float z;
};

} // namespace math

// vim: set sts=2 sw=2 expandtab:
#endif
