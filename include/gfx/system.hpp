#pragma once

#include <util/nocopy.hpp>

//============================================================================
//
// SDL subsystem lifetime
//
// RAII over SDL_Init/SDL_Quit, TTF_Init/TTF_Quit and IMG_Init/IMG_Quit.
// Construct one before any context or font and let it outlive them:
//
//     gfx::System system;
//     gfx::gles2::Context context("demo", 1280, 720);
//
// Renderer-neutral, deliberately. Which renderer draws has no bearing on
// whether SDL's video subsystem is up, and this used to be duplicated per
// backend: the 2016 gfx::System did this plus glewInit, and
// gfx::renderer::System did it plus owning a vector of raw Context*. Neither
// survives.
//
// It does not own contexts. The 2016 version heap-allocated them into a vector
// and deleted them in its destructor, which is both a raw owning pointer and a
// lifetime split across two classes — see D8. A context is owned by whatever
// created it.
//
//============================================================================
namespace gfx
{

class System
{
public:
    // Throws std::runtime_error if the video subsystem or SDL_ttf cannot start.
    // A missing image codec is a warning rather than a throw: a build may
    // legitimately ship only one asset format.
    System();
    ~System();

private:
    DISALLOW_COPY_AND_ASSIGN(System);
};

} // namespace gfx
