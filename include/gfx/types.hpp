#ifndef __GFX_TYPES_HPP__
#define __GFX_TYPES_HPP__

#include <vector>
#include <math/vector.hpp>

namespace gfx
{

typedef std::vector<math::Vector3> Vertices;
typedef std::vector<math::Vector3> Colors;
typedef std::vector<unsigned int> Indexes;

} // namespace gfx

// vim: set sts=2 sw=2 expandtab:
#endif
