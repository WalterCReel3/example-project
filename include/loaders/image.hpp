#pragma once
#include <string>

class SDL_Surface;

namespace loaders
{

SDL_Surface* load_image(const std::string& filename);

} // namespace loaders
