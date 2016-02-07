#ifndef __LOADERS_IMAGE_HPP__
#define __LOADERS_IMAGE_HPP__
#include <string>

class SDL_Surface;

namespace loaders
{

SDL_Surface* load_image(const std::string& filename);

} // namespace loaders

#endif
