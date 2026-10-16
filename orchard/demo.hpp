#pragma once

#include <memory>
#include <string>
#include <vector>

#include <game/camera.hpp>
#include <game/entities.hpp>
#include <game/level.hpp>
#include <gfx/animation.hpp>
#include <gfx/atlas.hpp>
#include <gfx/renderer/context.hpp>
#include <gfx/system.hpp>
#include <gfx/tilemap.hpp>
#include <rig/input.hpp>

#include "actors.hpp"
#include "cues.hpp"
#include "player.hpp"
#include "rules.hpp"

//============================================================================
//
// orchard — the game layer, assembled
//
// Two visible tile layers over a cleared sky, a player walking and jumping
// against the invisible `collision` layer, and a camera following it and
// clamping to the level. The map's rules come from game::Level; drawing it is a
// gfx::TileMap built from the same parsed document.
//
// THE SKY IS Context::clear() AND NOT A TILE LAYER. A cleared colour is one
// fill; a covering backdrop layer is a full-screen blit that everything above
// then paints over again, which on two Cortex-A7 cores software-rasterising
// every pixel is the difference between hitting the frame budget and missing
// it. `orchard.tmx` carries no backdrop for that reason, and `--seconds N`
// reports the per-frame tile cost so the saving is measured rather than
// asserted.
//
// The camera hands out whole pixels. A fractional offset puts every tile blit
// on SDL's stretch path, several times the cost of a 1:1 blit.
//
// WHAT THE RULES ARE AND ARE NOT DOING HERE. orchard::Rules decides what
// contact means and owns no entity; this file owns the cast, tests the boxes
// and turns the answers into removals, bursts and a HUD line. So the two halves
// of "the player touched a cherry" are split by what needs a renderer to check:
// whether the boxes overlap is geometry, whether that is worth a point is a
// rule, and only the latter is testable without a window.
//
//============================================================================
namespace orchard
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
    // regardless; this is how the same path is exercised on a dev box.
    bool software = false;

    // Silence every cue. A timed run and a screenshot run should measure
    // drawing, not also be an audio test.
    bool mute = false;
};

class Demo
{
public:
    explicit Demo(const Options& options);
    ~Demo();

    // Runs until quit, or until Options::seconds elapses.
    void run();

    // Renders `frames` frames on a fixed delta and writes the last one. Nobody
    // can watch a handheld's panel over SSH, so this is how "does it draw?" is
    // answered on a device.
    bool render_to_file(const std::string& path, int frames);

private:
    void load_level();
    void load_actors();

    // Every spawn in the map except the player, turned into an entity. Called
    // by load_actors() once the atlases are up, because a spawn needs a sprite
    // to be placed against — align_box_to_sprite() reads the frame.
    void spawn_cast();

    // One spawn, reporting which behaviour it became. Answers false for a type
    // this demo has no rule for, having said so; the caller counts those
    // rather than stopping.
    bool spawn_one(const gfx::MapObject& spawn, Behaviour& behaviour);

    // The animation a behaviour draws with at rest, or null if it has none.
    // A pointer rather than an AnimationSet::Index because the goal's is not
    // in the loaded set — see load_actors().
    const gfx::Animation* animation_for(Behaviour behaviour) const;

    // Advances every animal and moves its entity to match.
    void step_animals(double delta);

    // An animal's pose, from what it is doing. Sprite only, like
    // play_for_state() and for the same reason. Only the frog has two.
    void play_for_animal(const Animal& animal, game::Entity& actor) const;

    // Advances the simulation by one frame. Split out so run() and
    // render_to_file() drive the same thing from different clocks.
    void step(const Intent& intent, double delta);
    void draw();

    // Tests the player's box against the cast and hands each answer to Rules,
    // then acts on what it decided. The only place in this demo that knows both
    // a rule and an entity.
    void apply_rules();

    // A one-shot burst centred on `at`: sparkle for a collection, puff for a
    // bump. An entity with a sprite, no box and no rules state, removed by
    // reap_bursts() once its Loop::Once animation has played out.
    void burst(const gfx::Animation& animation, const game::Aabb& at);
    void reap_bursts();

    // Plays a cue and counts it. The only route to the mixer, which is what
    // keeps "fires once per event" a property of the call sites rather than of
    // five separate play() calls.
    void cue(Cue which);

    // What the run came to, which cues it played, and what that cost the mixer.
    // Printed by both drivers, because a screenshot run exercises the rules and
    // an interactive one may not.
    void report_run() const;

    // The score, and the state when the state is worth saying. A pure function
    // of the simulation, which is what keeps --screenshot reproducible with the
    // HUD on: coppers' fixture needs --no-hud precisely because its HUD draws
    // measured microseconds, and decision 7 was written from that lesson.
    std::string hud_text() const;
    void draw_hud();

    // The player's sprite, chosen from what it is doing. Sprite only — nothing
    // here feeds back into motion.
    void play_for_state();

    // The intent a screenshot run is driven by: walk right, and jump at
    // anything that blocks it. A function of the simulation's own state and
    // nothing else — no clock, no pad, no random — so the same binary writes
    // the same pixels for the same frame count. Holding Right alone would pin
    // the player against the first 32 px step in the ground and photograph it
    // standing there.
    Intent scripted_intent();

    Options _options;

    gfx::System _system;
    gfx::renderer::Context _context;
    rig::Pad _pad;

    // Declared after _context: every texture below belongs to that renderer and
    // must not outlive it.
    gfx::Atlas _foxy;
    gfx::AnimationSet _foxy_animations;

    gfx::Atlas _critters;
    gfx::AnimationSet _critter_animations;

    // Built here rather than loaded, so it is a member and not a local: an
    // AnimatedSprite holds a pointer to its definition. See load_actors().
    gfx::Animation _goal_marker;

    // By pointer because a TileMap owns a TileSet which owns a Texture, and
    // there is nothing to default-construct without a renderer.
    std::unique_ptr<gfx::TileMap> _map;

    // By pointer for a different reason: a Level has no default state, and
    // building one may throw. Until load_level() has run there is no level.
    std::unique_ptr<game::Level> _level;

    game::Camera _camera;
    Player _player;
    Motion _motion;

    // The HUD's font, or null if it could not be loaded — a missing font costs
    // the HUD and not the demo, which is the call coppers makes for the same
    // file. Speedy.fon rather than a glyph sheet: gfx::renderer::Context
    // already draws text through SDL_ttf and coppers' GlyphSheet is a
    // scroller's machinery living in coppers/, so reaching for it would be a
    // demo depending on another demo.
    TTF_Font* _font = nullptr;

    game::Entities _entities;
    game::Entities::Id _avatar = game::Entities::none();

    Stakes _stakes;
    Rules _rules;

    // The pickups and the goal by handle, so a collection does not search the
    // store. A pickup leaves this list the frame it is taken, which is also
    // what stops it being collected twice — Rules does no edge detection for
    // exactly this reason.
    struct Pickup {
        game::Entities::Id entity;
        Behaviour behaviour;
    };
    std::vector<Pickup> _pickups;
    game::Entities::Id _goal = game::Entities::none();

    // The one-shot bursts in flight. Entities with no box and no rules state.
    std::vector<game::Entities::Id> _bursts;

    // The mixer, the five chunks and the module. Declared before the counters
    // it is reported beside, and independent of everything above it: it holds
    // no texture and no renderer.
    Cues _cues;

    // Cues played, by Cue. Reported at the end of either driver, so "a cue
    // fires once per event" is a number a headless run prints rather than
    // something only an ear can check.
    int _cue_counts[static_cast<std::size_t>(Cue::Count)] = {0};

    // Turns the player's on_ground/velocity states into `jump` and `land`.
    // See cues.hpp: the other three cues come off Rules, which is already
    // edge-triggered, and these two are the pair that would otherwise fire
    // every frame.
    CueEdges _cue_edges;

    // The three animals' rules state, beside the entities that draw them. The
    // other spawns — the pickups and the goal — are entities and nothing else:
    // they are placed once and the store animates them from there.
    std::vector<Animal> _animals;
    Prowl _prowl;

    gfx::AnimationSet::Index _idle = gfx::AnimationSet::npos;
    gfx::AnimationSet::Index _run = gfx::AnimationSet::npos;
    gfx::AnimationSet::Index _jump = gfx::AnimationSet::npos;
    gfx::AnimationSet::Index _dizzy = gfx::AnimationSet::npos;
    gfx::AnimationSet::Index _victory = gfx::AnimationSet::npos;
    gfx::AnimationSet::Index _playing = gfx::AnimationSet::npos;

    // Into _critter_animations. Resolved once at load, because a name lookup
    // is a string compare and these are read per spawn and per frame.
    gfx::AnimationSet::Index _cherry = gfx::AnimationSet::npos;
    gfx::AnimationSet::Index _gem = gfx::AnimationSet::npos;
    gfx::AnimationSet::Index _opossum_walk = gfx::AnimationSet::npos;
    gfx::AnimationSet::Index _frog_idle = gfx::AnimationSet::npos;
    gfx::AnimationSet::Index _frog_jump = gfx::AnimationSet::npos;
    gfx::AnimationSet::Index _eagle_swoop = gfx::AnimationSet::npos;
    gfx::AnimationSet::Index _sparkle = gfx::AnimationSet::npos;
    gfx::AnimationSet::Index _puff = gfx::AnimationSet::npos;

    // Frames of Jump the screenshot script still has to hold down. A jump's
    // height is trimmed by releasing the button during the ascent, so a
    // one-frame press would be a deliberate short hop.
    int _scripted_jump = 0;

    // Tiles blitted on the most recent frame and the running total, so a timed
    // run reports work done and not only frames per second.
    int _tiles_drawn = 0;
    long long _tiles_total = 0;

    // The extremes behind the mean. A tile count that never moves is a count
    // that does not depend on where the camera is, and reporting only the
    // average hides which of the two a run was.
    int _tiles_min = -1;
    int _tiles_max = -1;

    // Wall-clock seconds spent inside TileMap::draw over the whole run. The
    // per-tile cost divides by this and not by the frame: a frame here also
    // clears, draws entities and rasterises the HUD afresh, and charging that
    // to the tilemap inflates us/tile by whatever else the demo happens to do.
    double _tilemap_seconds = 0.0;

    // The same for the rest of the frame. A capped run reports the cap whenever
    // the device keeps up, so the frame time alone says nothing about cost;
    // these are the work, whatever the cap was. Event polling and the cap's own
    // sleep are in none of them.
    //
    // draw() flushes the renderer at each boundary, so a batching driver's
    // deferred work is charged to the phase that queued it.
    double _step_seconds = 0.0;
    double _clear_seconds = 0.0;
    double _entities_seconds = 0.0;
    double _hud_seconds = 0.0;
    double _present_seconds = 0.0;
};

} // namespace orchard
