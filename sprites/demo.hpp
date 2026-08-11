#pragma once

#include <memory>
#include <string>
#include <vector>

#include <gfx/animation.hpp>
#include <gfx/atlas.hpp>
#include <gfx/renderer/context.hpp>
#include <gfx/system.hpp>
#include <gfx/tilemap.hpp>
#include <rig/input.hpp>
#include <rig/timing.hpp>

#include "entities.hpp"

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

    // Draw data/sunnyland.tmx behind the sprites. This is the fill-rate
    // instrument the snapshot asks for before a tilemap is designed around
    // per-tile blitting: the map's backdrop layer covers every cell, so the
    // whole target is blitted every frame and the exit line reports what that
    // cost.
    bool tilemap = false;

    // Scroll the camera rather than holding it still. A static camera lets a
    // driver do nothing between frames, which measures the wrong thing.
    bool scroll = true;

    // Integer scale for the tilemap. Raising it covers the same screen with
    // fewer, larger blits, which is what separates a per-call cost from a
    // per-pixel one.
    int tile_scale = 1;
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
    void load_tilemap();
    void update(double delta);
    void draw();

    // Advances every actor and steps the hero across the screen. Split out so
    // both run() and render_to_file() drive the same simulation.
    void step(double delta);
    void play_hero(const gfx::Animation& animation);

    Options _options;

    gfx::System _system;
    gfx::renderer::Context _context;
    rig::Pad _pad;

    // Declared after _context: the texture inside the atlas belongs to that
    // renderer and must not outlive it.
    gfx::Atlas _atlas;
    gfx::AnimationSet _animations;

    // Optional, and by pointer because TileMap owns a TileSet which owns a
    // Texture — there is no default to construct without a renderer.
    std::unique_ptr<gfx::TileMap> _map;
    double _camera = 0.0;
    int _camera_direction = 1;

    // Tiles blitted on the most recent frame, and the running total, so a
    // measured run reports work done rather than only frames per second.
    int _tiles_drawn = 0;
    long long _tiles_total = 0;

    // Every animated thing on screen, including the hero, so the store is what
    // the demo actually runs on rather than something tested beside it.
    Entities _entities;

    // The large one that walks across, and which animation it is playing.
    Entities::Id _hero = Entities::none();
    gfx::AnimationSet::Index _hero_animation = 0;
    int _hero_direction = 1;
};

} // namespace sprites
