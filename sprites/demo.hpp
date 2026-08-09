#pragma once

#include <string>
#include <vector>

#include <gfx/animation.hpp>
#include <gfx/atlas.hpp>
#include <gfx/renderer/context.hpp>
#include <gfx/system.hpp>
#include <rig/input.hpp>
#include <rig/timing.hpp>

//============================================================================
//
// sprites — the gfx::Atlas / gfx::AnimatedSprite demo
//
// Separate from coppers deliberately. coppers is the raster-effect demo and
// measures the layer path; this is the 2D game path — a texture atlas, named
// frames, and several things animating independently off one sheet.
//
// It needs no GPU and no gfx::gles2, so it builds on every target including the
// Miyoo Mini, which is where the fill-rate question this snapshot carries
// actually gets answered.
//
// See planning/2026-07-25-software-2d-sprites-tiling/.
//
//============================================================================
namespace sprites
{

struct Options {
    bool fullscreen = true;
    int width = 640;
    int height = 480;
    int target_fps = 60;
    double seconds = 0.0; // 0 runs until quit

    std::string screenshot;
    int screenshot_frames = 30;

    // Force the software driver even where a GPU exists. The Miyoo Mini gets it
    // regardless; this is how the same path gets exercised on a dev box.
    bool software = false;
};

// One animated thing on screen.
struct Actor {
    gfx::AnimatedSprite sprite;
    int x = 0;
    int y = 0;
    int scale = 1;
};

class Demo
{
public:
    explicit Demo(const Options& options);

    // Runs until quit, or until Options::seconds elapses.
    void run();

    // Renders `frames` frames and writes the last one. The counterpart of
    // coppers' own: nobody can watch a handheld's panel over SSH, so this is
    // how "does it actually draw?" gets answered on a device.
    bool render_to_file(const std::string& path, int frames);

private:
    void load();
    void update(double delta);
    void draw();

    // Advances every actor and steps the hero across the screen. Split out so
    // both run() and render_to_file() drive the same simulation.
    void step(double delta);

    Options _options;

    gfx::System _system;
    gfx::renderer::Context _context;
    rig::Pad _pad;

    // Declared after _context: the texture inside the atlas belongs to that
    // renderer and must not outlive it.
    gfx::Atlas _atlas;
    gfx::AnimationSet _animations;

    std::vector<Actor> _actors;

    // The large one that walks across, and which animation it is playing.
    Actor _hero;
    gfx::AnimationSet::Index _hero_animation = 0;
    double _hero_position = 0.0;
    int _hero_direction = 1;
};

} // namespace sprites
