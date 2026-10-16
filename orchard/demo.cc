#include "demo.hpp"

#include <SDL.h>
#include <SDL_ttf.h>

#include <loaders/animations.hpp>
#include <loaders/image.hpp>
#include <loaders/sparrow.hpp>
#include <loaders/tmx.hpp>
#include <rig/assets.hpp>
#include <rig/timing.hpp>
#include <util/format.hpp>
#include <util/logging.hpp>

#include "verbs.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>

namespace orchard
{

namespace
{

// Mean milliseconds per frame over a whole run.
//
// FrameClock::elapsed() accumulates the clamped deltas, and there is one delta
// per frame *boundary*: the first advance() establishes the origin and
// contributes zero, so N frames carry N-1 measured intervals. Dividing by
// frames() instead would report a cost that is too low by 1/N, which is
// invisible over a long run and wrong over a short one.
double mean_frame_ms(const rig::FrameClock& clock)
{
    if (clock.frames() < 2) {
        return 0.0;
    }
    return 1000.0 * clock.elapsed() / static_cast<double>(clock.frames() - 1);
}

// Decision 4's sky: one fill, and no backdrop layer to blit over.
const gfx::renderer::Color sky = {104, 168, 208, 255};

// The object type this pass places. Everything else in the spawn table is read
// and counted, so the log says what was skipped rather than leaving it silent.
const char* const player_type = "player";

// The HUD. White on the sky, top left, out of the deadzone the player lives in.
const gfx::renderer::Color hud_ink = {255, 255, 255, 255};
const int hud_margin = 6;

// SOME ART HAS NO BASELINE, AND ITS BOX SITS IN THE MIDDLE OF THE FRAME.
//
// Entity::align_box_to_sprite() sits a box on the lowest drawn row of the art,
// and that is right for anything that stands: the feet of the art are the feet
// of the collider. It answers "where are the feet". This answers a different
// question — "there are no feet, so where is the middle" — and the two are not
// one question with a mode. Fixing the first does not subsume the second, which
// was checked rather than assumed, below.
//
// Measured over all four `eagle-swoop` frames of the shipped atlas: the art's
// centre of mass sits at row 21.8 of the 41-row logical frame. A 24-row box
// bottom-flush on the FRAME spans 17..41 and is centred on row 29 — 7.2 rows
// low, so the bird's head is outside its own hitbox and the empty sky under its
// talons is inside it. Bottom-flush on the ART is barely better: the art ends
// at rows 39/38/39/39, so the box is centred on 26.75 and is still 4.5 rows
// low. Centred spans 8.5..32.5 and is 1.3 rows out. Once contact costs a pickup
// that difference is the difference between being hit by a bird and being hit
// by nothing.
//
// The goal marker asks the same question for a different reason. It draws
// `sparkle`, a radially symmetric burst whose centre of mass is row 16.0 to
// 16.5 of a 32-row frame on every cel while its lowest drawn row travels from
// 25 to 32 as it expands. There is no contact point anywhere in that, and
// anchoring the burst to its own growing edge would push it 7 px into the
// ground it marks.
//
// Local to orchard, and deliberately not a second convention on game::Entity:
// decision 16 rejected a stored offset and an anchor mode by name, and two
// pieces of centred art in one demo is a routing table in that demo, not a mode
// on the shared call.
void centre_box_on_sprite(game::Entity& actor)
{
    // For the horizontal half of the convention, which centred art wants
    // unchanged, and for its no-op when there is no frame to measure.
    actor.align_box_to_sprite();

    const double frame_height = actor.sprite.frame_height();
    if (frame_height > 0.0) {
        actor.box.y = (frame_height - actor.box.height) / 2.0;
    }
}

gfx::renderer::Driver driver_for(const Options& options)
{
    return options.software ? gfx::renderer::Driver::Software
                            : gfx::renderer::Driver::PreferAccelerated;
}

// Uploads a sheet named by a document, resolving it beside the other assets
// rather than relative to the document — the flat data/ layout makes those the
// same place, and rig::asset_path is what survives a launcher changing
// directory.
gfx::renderer::Texture upload_sheet(gfx::renderer::Context& context,
                                    const std::string& image_path)
{
    // Freed however the upload goes, including by the throw below: Texture
    // uploads the pixels and does not adopt the surface.
    const std::unique_ptr<SDL_Surface, void (*)(SDL_Surface*)> image(
        loaders::load_image(rig::asset_path(image_path)), SDL_FreeSurface);

    if (!image) {
        throw std::runtime_error(
            util::format("could not load sheet %s", image_path.c_str()));
    }

    return gfx::renderer::Texture(context, image.get());
}

// Whichever anchor convention this behaviour is placed by. One call rather than
// a conditional at each of the three sites that place an actor.
//
// The split is "does this thing stand on something". The opossum, the frog and
// the player do; the eagle and the goal's sparkle do not, and there is nothing
// in their art for a baseline to mean.
void align_box(game::Entity& actor, Behaviour behaviour)
{
    if (behaviour == Behaviour::Eagle || behaviour == Behaviour::Goal) {
        centre_box_on_sprite(actor);
    } else {
        actor.align_box_to_sprite();
    }
}

} // namespace

Demo::Demo(const Options& options)
    : _options(options)
    , _system()
    , _context("orchard", options.width, options.height, options.fullscreen,
               driver_for(options))
    , _pad()
    // Placeholders so the members exist before the loaders replace them. Atlas
    // has no default constructor by design — it owns a Texture, and a Texture
    // without a renderer is not a thing.
    , _foxy(gfx::renderer::Texture(_context, 1, 1), gfx::Atlas::Frames())
    , _critters(gfx::renderer::Texture(_context, 1, 1), gfx::Atlas::Frames())
    , _camera(_context.width(), _context.height())
    // Opens the device and starts the module, or does nothing at all when
    // muted. Before load_level() rather than after, so the music covers the
    // load rather than starting once it is over.
    , _cues(options.mute)
{
    util::log_info("orchard: %dx%d, driver %s%s", _context.width(),
                   _context.height(), _context.driver_name().c_str(),
                   _context.accelerated() ? " (accelerated)" : "");
    util::log_info("orchard: input %s", _pad.description().c_str());

    for (int i = 0; i < static_cast<int>(Verb::Count); ++i) {
        const Verb verb = static_cast<Verb>(i);
        util::log_info("orchard: %s is %s", verb_name(verb),
                       rig::button_name(button_for(verb)));
    }

    // The HUD is the only text here, so a missing font costs the score line
    // rather than the demo — the same call coppers makes for the same file.
    const std::string font_path = rig::asset_path("Speedy.fon");
    _font = TTF_OpenFontIndex(font_path.c_str(), 10, 0);
    if (_font == nullptr) {
        util::log_warning("orchard: no HUD: could not load %s: %s",
                          font_path.c_str(), TTF_GetError());
    }

    load_level();
    load_actors();
}

Demo::~Demo()
{
    if (_font != nullptr) {
        TTF_CloseFont(_font);
    }
}

void Demo::load_level()
{
    const loaders::TmxMap map =
        loaders::load_tmx(rig::asset_path("orchard.tmx"));

    // The rules half and the drawing half are built from the same parsed
    // document, side by side. A Level deliberately cannot be built from a
    // TileMap: a TileMap keeps the tileset's tile size, and on a map whose
    // tileset holds oversized tiles that is not the grid size the collision
    // layer is measured in.
    _level.reset(new game::Level(map.layers, map.object_layers, map.width,
                                 map.height, map.tile_width, map.tile_height,
                                 "orchard.tmx"));

    gfx::TileSet tiles(upload_sheet(_context, map.tileset.image),
                       map.tileset.tile_width, map.tileset.tile_height,
                       map.tileset.columns, map.tileset.tile_count,
                       map.tileset.margin, map.tileset.spacing);

    if (!tiles.fits_texture()) {
        util::log_warning("orchard: tileset claims %d tiles that do not fit "
                          "its %dx%d image",
                          map.tileset.tile_count, tiles.texture().width(),
                          tiles.texture().height());
    }

    _map.reset(
        new gfx::TileMap(std::move(tiles), map.layers, map.width, map.height));

    _camera.set_bounds(_level->pixel_width(), _level->pixel_height());

    // A deadzone a quarter of the view across. Small enough that the player
    // never approaches an edge, wide enough that walking a few pixels back and
    // forth does not drag the whole level with it.
    _camera.set_deadzone(_context.width() / 4.0, _context.height() / 4.0);

    util::log_info("orchard: level %dx%d tiles of %dx%d (%dx%d px), %zu layers,"
                   " %zu spawns",
                   _level->width(), _level->height(), _level->tile_width(),
                   _level->tile_height(), _level->pixel_width(),
                   _level->pixel_height(), map.layers.size(),
                   _level->spawns().size());
}

void Demo::load_actors()
{
    const loaders::SparrowAtlas foxy =
        loaders::load_sparrow(rig::asset_path("foxy.xml"));
    _foxy = gfx::Atlas(upload_sheet(_context, foxy.image_path), foxy.frames);

    if (!_foxy.contains_frames()) {
        // Not fatal: it still draws, just wrongly. Saying so is the difference
        // between a five-minute diagnosis and an afternoon of squinting.
        util::log_warning("orchard: foxy frames run outside %dx%d sheet",
                          _foxy.texture().width(), _foxy.texture().height());
    }

    _foxy_animations =
        loaders::load_animation_set(_foxy, rig::asset_path("foxy.anim.xml"));

    _idle = _foxy_animations.find("idle");
    _run = _foxy_animations.find("run");
    _jump = _foxy_animations.find("jump");

    // `dizzy` is 6 frames on a repeat loop and `victory` is one, which is why
    // Stakes times both of them: neither pose can end anything by itself.
    _dizzy = _foxy_animations.find("dizzy");
    _victory = _foxy_animations.find("victory");

    if (_idle == gfx::AnimationSet::npos || _run == gfx::AnimationSet::npos ||
        _jump == gfx::AnimationSet::npos || _dizzy == gfx::AnimationSet::npos ||
        _victory == gfx::AnimationSet::npos) {
        throw std::runtime_error("foxy.anim.xml is missing idle, run, jump, "
                                 "dizzy or victory");
    }

    const loaders::SparrowAtlas critters =
        loaders::load_sparrow(rig::asset_path("critters.xml"));
    _critters = gfx::Atlas(upload_sheet(_context, critters.image_path),
                           critters.frames);
    _critter_animations = loaders::load_animation_set(
        _critters, rig::asset_path("critters.anim.xml"));

    if (!_critters.contains_frames()) {
        util::log_warning("orchard: critter frames run outside %dx%d sheet",
                          _critters.texture().width(),
                          _critters.texture().height());
    }

    _cherry = _critter_animations.find("cherry");
    _gem = _critter_animations.find("gem");
    _opossum_walk = _critter_animations.find("opossum-walk");
    _frog_idle = _critter_animations.find("frog-idle");
    _frog_jump = _critter_animations.find("frog-jump");
    _eagle_swoop = _critter_animations.find("eagle-swoop");

    // The two one-shots, and which serves what is settled by the art rather
    // than chosen here. `sparkle` is the pack's `item-feedback` renamed on
    // ingest — the twinkle a pickup leaves behind — and `puff` is its
    // `enemy-death`, a generic dust cloud renamed because nothing in this game
    // dies. So sparkle marks a collection and puff marks a bump, which is also
    // the reading the art table in the snapshot already records.
    _sparkle = _critter_animations.find("sparkle");
    _puff = _critter_animations.find("puff");

    if (_cherry == gfx::AnimationSet::npos || _gem == gfx::AnimationSet::npos ||
        _opossum_walk == gfx::AnimationSet::npos ||
        _frog_idle == gfx::AnimationSet::npos ||
        _frog_jump == gfx::AnimationSet::npos ||
        _eagle_swoop == gfx::AnimationSet::npos ||
        _sparkle == gfx::AnimationSet::npos ||
        _puff == gfx::AnimationSet::npos) {
        throw std::runtime_error("critters.anim.xml is missing an animation "
                                 "orchard spawns");
    }

    // The goal's marker, built here rather than read from critters.anim.xml.
    // The shipped `sparkle` plays once, which is what it is drawn for — it is
    // the burst a collection will show when the rules slice writes one. A
    // standing end-of-level marker wants the same frames looping, and building
    // a second definition over them costs nothing and leaves the asset saying
    // what it means.
    _goal_marker = gfx::animation_from_prefix(_critters, "sparkle", 8.0,
                                              gfx::Loop::Repeat);
    if (_goal_marker.empty()) {
        throw std::runtime_error("critters.xml has no \"sparkle\" frames");
    }

    util::log_info("orchard: foxy %zu frames / %zu animations, critters %zu / "
                   "%zu",
                   _foxy.size(), _foxy_animations.size(), _critters.size(),
                   _critter_animations.size());

    // The spawn table, mapped to this game's vocabulary here and nowhere else.
    const gfx::MapObject* start = nullptr;

    for (const gfx::MapObject& spawn : _level->spawns()) {
        if (spawn.type == player_type) {
            // The first wins. A level with two players is a level to fix, and
            // saying so beats silently taking one of them.
            if (start == nullptr) {
                start = &spawn;
            } else {
                util::log_warning("orchard: more than one %s spawn; using the "
                                  "first",
                                  player_type);
            }
        }
    }

    if (start == nullptr) {
        throw std::runtime_error("orchard.tmx has no \"player\" spawn");
    }

    game::Entity avatar;
    avatar.sprite = gfx::AnimatedSprite(_foxy, _foxy_animations[_idle]);

    // The spawn's own rectangle, and the demo's only copy of it: step() reads
    // it back out of world_box() every frame rather than the Player keeping a
    // second one. The offset within the sprite frame is align_box_to_sprite()'s
    // to fill in, per step.
    avatar.x = start->x;
    avatar.y = start->y;
    avatar.box = game::Aabb{0.0, 0.0, start->width, start->height};
    avatar.behaviour = static_cast<int>(Behaviour::Player);

    _playing = _idle;
    _avatar = _entities.add(avatar);

    _camera.snap_to(start->x + start->width / 2.0,
                    start->y + start->height / 2.0);

    util::log_info("orchard: player at (%.0f, %.0f), box %.0fx%.0f", start->x,
                   start->y, start->width, start->height);

    spawn_cast();
}

const gfx::Animation* Demo::animation_for(Behaviour behaviour) const
{
    switch (behaviour) {
    case Behaviour::Opossum:
        return &_critter_animations[_opossum_walk];
    case Behaviour::Frog:
        return &_critter_animations[_frog_idle];
    case Behaviour::Eagle:
        return &_critter_animations[_eagle_swoop];
    case Behaviour::Cherry:
        return &_critter_animations[_cherry];
    case Behaviour::Gem:
        return &_critter_animations[_gem];
    case Behaviour::Goal:
        return &_goal_marker;
    default:
        // The player, which draws from the other atlas and is placed by name
        // above rather than through this table.
        return nullptr;
    }
}

bool Demo::spawn_one(const gfx::MapObject& spawn, Behaviour& behaviour)
{
    if (!behaviour_for(spawn.type, behaviour)) {
        util::log_warning("orchard: no rule for \"%s\" spawn at (%.0f, %.0f)",
                          spawn.type.c_str(), spawn.x, spawn.y);
        return false;
    }

    const gfx::Animation* animation = animation_for(behaviour);
    if (animation == nullptr) {
        return false;
    }

    game::Entity actor;
    actor.sprite = gfx::AnimatedSprite(_critters, *animation);
    actor.behaviour = static_cast<int>(behaviour);

    // The spawn's own rectangle is the collision box, as it is for the player:
    // it is the rectangle game::Level bounds-checked. align_box_to_sprite()
    // then offsets it within the frame currently showing and place_box() moves
    // the entity so that box lands on the spawn — the same two calls for every
    // one of the six, with no per-actor arithmetic between them.
    actor.box = game::Aabb{0.0, 0.0, spawn.width, spawn.height};
    align_box(actor, behaviour);
    actor.place_box(spawn.x, spawn.y);

    const game::Entities::Id id = _entities.add(actor);

    // Each of the three keeps a different thing beside its entity, and which
    // one is the whole of what the spawn table means to this demo.
    switch (behaviour) {
    case Behaviour::Opossum:
    case Behaviour::Frog:
    case Behaviour::Eagle: {
        Animal animal = animal_at(behaviour, spawn.x, _prowl);
        animal.entity = id;
        _animals.push_back(animal);
        break;
    }

    case Behaviour::Goal:
        // The first wins, as the player spawn does and for the same reason.
        if (_goal == game::Entities::none()) {
            _goal = id;
        }
        break;

    default:
        // A pickup: a handle and its kind, so collecting one is a removal
        // rather than a search. Anything with no points is filtered by
        // Rules::collect() rather than by a second list of types here.
        _pickups.push_back(Pickup{id, behaviour});
        break;
    }

    return true;
}

void Demo::spawn_cast()
{
    int placed[static_cast<int>(Behaviour::Count)] = {0};
    int unknown = 0;

    for (const gfx::MapObject& spawn : _level->spawns()) {
        if (spawn.type == player_type) {
            continue;
        }

        Behaviour behaviour = Behaviour::Player;
        if (spawn_one(spawn, behaviour)) {
            ++placed[static_cast<int>(behaviour)];
        } else {
            ++unknown;
        }
    }

    for (int i = 0; i < static_cast<int>(Behaviour::Count); ++i) {
        if (placed[i] > 0) {
            util::log_info("orchard: %d %s", placed[i],
                           behaviour_name(static_cast<Behaviour>(i)));
        }
    }

    if (unknown > 0) {
        util::log_warning("orchard: %d spawn(s) placed nothing", unknown);
    }

    util::log_info("orchard: %zu entities, %zu of them animals",
                   _entities.size(), _animals.size());
}

void Demo::play_for_state()
{
    // The rules' two poses outrank the motion's three: a player sitting down
    // dizzy is still falling off whatever it was bumped on, and one standing in
    // the goal is still standing on the ground, so reading the motion first
    // would draw a run cycle through both.
    const gfx::AnimationSet::Index wanted =
        _rules.phase() == Phase::Dizzy      ? _dizzy
        : _rules.phase() == Phase::Finished ? _victory
        : !_player.on_ground()              ? _jump
        : _player.velocity_x() != 0.0       ? _run
                                            : _idle;

    if (wanted == _playing) {
        return;
    }

    if (game::Entity* avatar = _entities.find(_avatar)) {
        avatar->sprite.play(_foxy_animations[wanted]);
        _playing = wanted;
    }
}

void Demo::play_for_animal(const Animal& animal, game::Entity& actor) const
{
    if (animal.behaviour != Behaviour::Frog) {
        return;
    }

    const gfx::Animation& wanted =
        _critter_animations[animal.airborne ? _frog_jump : _frog_idle];

    // Compared by address rather than by name: play() restarts even when
    // handed the animation already showing, so asking for the current one
    // every frame would pin the frog to its first cel.
    if (actor.sprite.animation() != &wanted) {
        actor.sprite.play(wanted);
    }
}

void Demo::step_animals(double delta)
{
    for (Animal& animal : _animals) {
        game::Entity* actor = _entities.find(animal.entity);
        if (actor == nullptr) {
            continue;
        }

        const game::Aabb resolved = step_animal(
            animal, _level->collision(), actor->world_box(), _prowl, delta);

        // Pose before placement, as the player does: align_box_to_sprite()
        // reads the frame showing now, so a switch made after it would be
        // measured against the pose that has just been left.
        play_for_animal(animal, *actor);
        align_box(*actor, animal.behaviour);
        actor->place_box(resolved.x, resolved.y);
    }
}

void Demo::report_run() const
{
    util::log_info("orchard: %s, %d points, %d bumps, %d cherries, %d gems",
                   phase_name(_rules.phase()), _rules.points(), _rules.bumps(),
                   _rules.held(Behaviour::Cherry), _rules.held(Behaviour::Gem));

    for (int i = 0; i < static_cast<int>(Cue::Count); ++i) {
        if (_cue_counts[i] > 0) {
            util::log_info("orchard: cue %s x%d", cue_name(static_cast<Cue>(i)),
                           _cue_counts[i]);
        }
    }

    if (_cues.muted()) {
        util::log_info("orchard: muted, so none of those were played");
    } else {
        util::log_info("orchard: voices %d configured, peak %d concurrent, "
                       "%d dropped",
                       _cues.voices(), _cues.peak_voices(), _cues.dropped());
    }
}

void Demo::cue(Cue which)
{
    ++_cue_counts[static_cast<std::size_t>(which)];

    // One call, one chunk. Every caller below is already an edge — three come
    // off Rules, which answers once per event, and two off CueEdges — so this
    // needs no de-duplication of its own, and adding some here would hide a
    // call site that had stopped being an edge.
    _cues.play(which);
}

void Demo::burst(const gfx::Animation& animation, const game::Aabb& at)
{
    game::Entity fx;
    fx.sprite = gfx::AnimatedSprite(_critters, animation);

    // Over the cast rather than behind it. No box at all, so it collides with
    // nothing — game::overlaps reads an empty box as "there is nothing here",
    // which is what makes a decoration harmless by default.
    fx.layer = 1;
    fx.x = at.x + at.width / 2.0 - fx.sprite.frame_width() / 2.0;
    fx.y = at.y + at.height / 2.0 - fx.sprite.frame_height() / 2.0;

    _bursts.push_back(_entities.add(fx));
}

void Demo::reap_bursts()
{
    // finished() is true only once a Loop::Once animation's LAST frame has been
    // held for its own duration, so reaping on it shows the whole burst rather
    // than cutting the last cel short.
    const std::vector<game::Entities::Id>::iterator done =
        std::stable_partition(
            _bursts.begin(), _bursts.end(), [this](game::Entities::Id id) {
                const game::Entity* fx = _entities.find(id);
                return fx != nullptr && !fx->sprite.finished();
            });

    for (std::vector<game::Entities::Id>::iterator it = done;
         it != _bursts.end(); ++it) {
        _entities.remove(*it);
    }
    _bursts.erase(done, _bursts.end());
}

void Demo::apply_rules()
{
    const game::Entity* avatar = _entities.find(_avatar);
    if (avatar == nullptr) {
        return;
    }

    // A COPY, AND `avatar` IS NOT TOUCHED AGAIN BELOW. burst() calls
    // Entities::add(), which may grow the slot vector and invalidate every
    // Entity* taken before it. Each pointer here is therefore fetched fresh and
    // read before any burst can be spawned, and the one that has to outlive a
    // burst is this rectangle rather than the entity holding it.
    const game::Aabb player_box = avatar->world_box();

    // The animals first. A bump costs the pickup the player is holding, so
    // running collection first would let one frame give a cherry and take it
    // straight back — the same frame reading two ways depending on the order,
    // which is the kind of thing that only ever shows up as a score that
    // occasionally does not add up.
    for (std::size_t slot = 0; slot < _animals.size(); ++slot) {
        const game::Entity* animal = _entities.find(_animals[slot].entity);
        const bool overlapping =
            animal != nullptr &&
            game::overlaps(player_box, animal->world_box());

        if (_rules.touch_animal(slot, overlapping)) {
            burst(_critter_animations[_puff], player_box);
            cue(Cue::Bump);
        }
    }

    for (std::size_t i = 0; i < _pickups.size();) {
        const game::Entity* pickup = _entities.find(_pickups[i].entity);
        if (pickup != nullptr &&
            game::overlaps(player_box, pickup->world_box()) &&
            _rules.collect(_pickups[i].behaviour)) {
            burst(_critter_animations[_sparkle], pickup->world_box());
            cue(Cue::Pickup);

            _entities.remove(_pickups[i].entity);
            _pickups.erase(_pickups.begin() + static_cast<std::ptrdiff_t>(i));
        } else {
            ++i;
        }
    }

    const game::Entity* goal = _entities.find(_goal);
    if (_rules.reach_goal(goal != nullptr &&
                          game::overlaps(player_box, goal->world_box()))) {
        cue(Cue::LevelComplete);
        util::log_info("orchard: goal reached with %d points, %d bumps",
                       _rules.points(), _rules.bumps());
    }
}

Intent Demo::scripted_intent()
{
    if (_player.on_ground() && _player.bumped()) {
        _scripted_jump = 20;
    }

    Intent intent;
    intent.right = true;
    intent.jump_pressed = _scripted_jump == 20;
    intent.jump_held = _scripted_jump > 0;

    if (_scripted_jump > 0) {
        --_scripted_jump;
    }
    return intent;
}

void Demo::step(const Intent& intent, double delta)
{
    game::Entity* avatar = _entities.find(_avatar);
    if (avatar == nullptr) {
        return;
    }

    reap_bursts();
    _rules.advance(delta);

    // Sitting down is not steering, and neither is having finished. The rules
    // withhold the intent rather than reaching into Player, so gravity, the
    // floor and the walls all still apply — a player bumped in mid-air falls
    // the rest of the way down.
    const Intent asked = _rules.accepts_intent() ? intent : Intent();

    const game::Aabb resolved = _player.step(
        _level->collision(), avatar->world_box(), asked, _motion, delta);

    // The two cues with no rule behind them, read off the step that just
    // happened. Observing rather than asking is what keeps them one-shot:
    // on_ground() is true on every frame the player stands still, and cueing
    // `land` off it directly would restart the same chunk sixty times a second.
    // See cues.hpp.
    const CueEdges::Fired fired =
        _cue_edges.observe(_player.on_ground(), _player.velocity_y());
    if (fired.jump) {
        cue(Cue::Jump);
    }
    if (fired.land) {
        cue(Cue::Land);
    }

    play_for_state();

    // foxy's logical frame is wider and taller than the collision box, so the
    // box sits centred on the frame with their bottom edges flush, and the
    // entity is then placed so that box lands where the Player resolved it.
    // Without the first the art hangs off one side and floats above the floor
    // it is standing on.
    avatar->align_box_to_sprite();
    avatar->place_box(resolved.x, resolved.y);

    step_animals(delta);

    // After both movers and before update(): the boxes tested are the ones this
    // frame resolved, so a bump is decided against where the two actually are
    // rather than against where they were when the frame started.
    apply_rules();

    // The store advances the sprite. Position is driven above rather than
    // integrated here, because collision has already resolved it.
    _entities.update(delta);

    _camera.follow(resolved.x + resolved.width / 2.0,
                   resolved.y + resolved.height / 2.0);
}

void Demo::draw()
{
    // Each phase is flushed before its closing timestamp, so the time recorded
    // is the drawing and not the queueing of it.
    const double clear_start = rig::FrameClock::now();
    _context.clear(sky);
    _context.flush();

    // The collision layer ships visible="0" and TileMap::draw skips it, so this
    // draws `ground` and `canopy` and costs nothing for the third.
    const double tilemap_start = rig::FrameClock::now();
    _clear_seconds += tilemap_start - clear_start;
    _tiles_drawn = _map->draw(_context, _camera.x(), _camera.y());
    _context.flush();
    const double entities_start = rig::FrameClock::now();
    _tilemap_seconds += entities_start - tilemap_start;

    _tiles_total += _tiles_drawn;
    _tiles_min =
        _tiles_min < 0 ? _tiles_drawn : std::min(_tiles_min, _tiles_drawn);
    _tiles_max = std::max(_tiles_max, _tiles_drawn);

    _entities.draw(_context, _camera.x(), _camera.y());
    _context.flush();
    const double hud_start = rig::FrameClock::now();
    _entities_seconds += hud_start - entities_start;
    draw_hud();
    _context.flush();
    _hud_seconds += rig::FrameClock::now() - hud_start;
}

std::string Demo::hud_text() const
{
    const int cherries = _rules.held(Behaviour::Cherry);
    const int gems = _rules.held(Behaviour::Gem);

    if (_rules.phase() == Phase::Finished) {
        // `victory` is one frame, so this line and the cue are the whole of the
        // flourish. Decision 7's note, paid.
        return util::format("ORCHARD CLEARED!   SCORE %d   BUMPS %d",
                            _rules.points(), _rules.bumps());
    }

    return util::format("SCORE %d   CHERRIES %d   GEMS %d   BUMPS %d",
                        _rules.points(), cherries, gems, _rules.bumps());
}

void Demo::draw_hud()
{
    if (_font == nullptr) {
        return;
    }

    // Upper case throughout, because Speedy.fon at 10 px is a bitmap face and
    // its lowercase is a smear at this size — the same reason coppers upper-
    // cases its scroller text.
    gfx::renderer::Rect rect;
    rect.x = hud_margin;
    rect.y = hud_margin;
    rect.w = 0;
    rect.h = 0;

    // Rasterised and uploaded per frame, which is what draw_text does. Every
    // figure in it changes as the run goes, so a cache would mostly miss.
    _context.draw_text(hud_text(), _font, hud_ink, &rect);
}

void Demo::run()
{
    rig::FrameClock clock(_options.target_fps);

    // What a device run cannot be asked afterwards: which controls arrived, why
    // the loop ended, and whether any frame stalled. Start and the Menu key
    // both end a run, and only the first of them is a binding under test.
    std::array<int, static_cast<std::size_t>(Verb::Count)> presses{};
    const char* stopped_by = nullptr;
    double previous_frame = 0.0;
    double longest_frame = 0.0;
    int clamped_frames = 0;

    bool running = true;
    while (running) {
        const double frame_start = rig::FrameClock::now();
        if (clock.frames() > 0) {
            longest_frame =
                std::max(longest_frame, frame_start - previous_frame);
        }
        previous_frame = frame_start;

        _pad.begin_frame();

        SDL_Event event;
        while (SDL_PollEvent(&event) != 0) {
            _pad.handle_event(event);
        }

        const Controls controls(_pad);
        for (std::size_t i = 0; i < presses.size(); ++i) {
            if (controls.pressed(static_cast<Verb>(i))) {
                ++presses[i];
            }
        }

        if (_pad.quit_requested()) {
            running = false;
            stopped_by = "a quit event or Escape";
        } else if (controls.pressed(Verb::Quit)) {
            running = false;
            stopped_by = "the Quit verb";
        }

        Intent intent;
        intent.left = controls.down(Verb::Left);
        intent.right = controls.down(Verb::Right);
        intent.jump_pressed = controls.pressed(Verb::Jump);
        intent.jump_held = controls.down(Verb::Jump);

        const double delta = clock.tick();
        if (clock.clamped()) {
            ++clamped_frames;
        }
        const double step_start = rig::FrameClock::now();
        step(intent, delta);
        _step_seconds += rig::FrameClock::now() - step_start;
        draw();

        const double present_start = rig::FrameClock::now();
        _context.present();
        _present_seconds += rig::FrameClock::now() - present_start;

        // The run ends when the rules say the victory pose has held long
        // enough, not when the animation ends — it is one frame on a repeat
        // loop and has no end to reach.
        if (_rules.run_over()) {
            running = false;
            stopped_by = stopped_by ? stopped_by : "the rules";
        }

        if (_options.seconds > 0.0 && clock.elapsed() >= _options.seconds) {
            running = false;
            stopped_by = stopped_by ? stopped_by : "--seconds";
        }
    }

    util::log_info("orchard: stopped by %s", stopped_by);
    for (std::size_t i = 0; i < presses.size(); ++i) {
        // Zero is printed too: "Jump pressed x0" after a run somebody played is
        // the finding.
        util::log_info("orchard: %s pressed x%d",
                       verb_name(static_cast<Verb>(i)), presses[i]);
    }

    report_run();

    // clock.fps() is an exponential moving average weighted 0.05 on the newest
    // frame, so it describes the last handful of frames and not the run. The
    // mean below is what a fill-rate figure has to be divided by, and the two
    // differ by more than the effects this instrument is used to measure.
    const double mean_ms = mean_frame_ms(clock);

    util::log_info("orchard: %llu frames, %.1f fps mean, %.1f fps recent",
                   static_cast<unsigned long long>(clock.frames()),
                   mean_ms > 0.0 ? 1000.0 / mean_ms : 0.0, clock.fps());

    if (clock.frames() > 0) {
        const double per_frame = static_cast<double>(_tiles_total) /
                                 static_cast<double>(clock.frames());
        const double ms = mean_ms;

        // us/tile is the tilemap's cost per tile, so it divides the tilemap's
        // own time. Dividing the frame's would fold in the clear, the entities
        // and a HUD string that is rasterised and uploaded every frame — which
        // is most of this demo's frame and none of its fill rate.
        const double tilemap_ms =
            1000.0 * _tilemap_seconds / static_cast<double>(clock.frames());
        const double us_per_tile =
            per_frame > 0.0 ? tilemap_ms * 1000.0 / per_frame : 0.0;
        util::log_info(
            "orchard: tilemap %d layers, %.0f tiles/frame (min %d, max %d "
            "over a %dx%d target), %.3f ms/frame of which %.3f ms is the "
            "tilemap, %.2f us/tile",
            static_cast<int>(_map->layers().size()), per_frame, _tiles_min,
            _tiles_max, _context.width(), _context.height(), ms, tilemap_ms,
            us_per_tile);

        const double per_frame_ms =
            1000.0 / static_cast<double>(clock.frames());
        util::log_info(
            "orchard: ms/frame step %.3f, clear %.3f, tilemap %.3f, "
            "entities %.3f, hud %.3f, present %.3f; longest frame %.1f ms, "
            "%d clamped",
            _step_seconds * per_frame_ms, _clear_seconds * per_frame_ms,
            tilemap_ms, _entities_seconds * per_frame_ms,
            _hud_seconds * per_frame_ms, _present_seconds * per_frame_ms,
            longest_frame * 1000.0, clamped_frames);
        // The voice fields are appended rather than woven in, so the four
        // figures results.md already records stay in the columns it reads them
        // from. They are here because the Risks section asks the fill-rate run
        // to report voice count alongside frame cost: a module and five cues
        // are per-frame mixer work on the same two Cortex-A7 cores that are
        // rasterising, and `--mute` is the other half of that measurement.
        //
        // `view` and the min/max are appended on the same terms. A tile count
        // is only meaningful against the target it was drawn into — this map is
        // 96x40 cells, so a large enough target holds all of it and the count
        // saturates at the map's filled-cell total. Reporting the target beside
        // the count is what stops that being read as the map being counted.
        std::printf("tiles/frame %.0f  ms/frame %.3f  fps %.1f  us/tile %.2f  "
                    "voices %d  peak %d  dropped %d  view %dx%d  tiles/frame "
                    "min %d max %d  tilemap ms/frame %.3f\n",
                    per_frame, ms, ms > 0.0 ? 1000.0 / ms : 0.0, us_per_tile,
                    _cues.voices(), _cues.peak_voices(), _cues.dropped(),
                    _context.width(), _context.height(), _tiles_min, _tiles_max,
                    tilemap_ms);
    }
}

bool Demo::render_to_file(const std::string& path, int frames)
{
    // A fixed delta and a scripted intent, so the same binary writes the same
    // pixels every time: nothing here reads a clock or a pad. Walking rather
    // than standing puts the run cycle in the frame, which is where a wrong
    // trim offset shows up as a sprite bobbing against its own baseline, and a
    // long enough --frames walks into the level rather than photographing the
    // spawn.
    const double delta = 1.0 / 60.0;

    for (int i = 0; i < frames; ++i) {
        step(scripted_intent(), delta);
        draw();

        // Before present(): SDL does not guarantee the back buffer survives the
        // swap, so reading after presenting can return undefined pixels.
        if (i + 1 == frames) {
            if (!_context.save_screenshot(path)) {
                util::log_error("orchard: could not write %s", path.c_str());
                return false;
            }
        }
        _context.present();
    }

    util::log_info("orchard: wrote %s after %d frames", path.c_str(), frames);

    // The same summary run() prints. A screenshot run is the only path that
    // drives the rules without a human, so it is the one most likely to be the
    // first to fire a cue — reporting from only the interactive loop would
    // leave the seam silent exactly where it is easiest to exercise.
    report_run();
    return true;
}

} // namespace orchard
