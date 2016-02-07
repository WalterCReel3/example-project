#ifndef __LOADERS_OBJ_HPP__
#define __LOADERS_OBJ_HPP__
#include <cstdio>
#include <stdlib.h>
#include <gfx/obj.hpp>
#include <util/string.hpp>

namespace loaders
{

void load_obj_model(const std::string& filename, gfx::ObjModel& model);

} // namespace loaders

// vim: sts=2 sw=2 expandtab:

#endif
