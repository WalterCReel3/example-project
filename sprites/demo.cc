#include "demo.hpp"

#include <SDL.h>

#include <loaders/animations.hpp>
#include <loaders/image.hpp>
#include <loaders/sparrow.hpp>
#include <loaders/tmx.hpp>
#include <rig/assets.hpp>
#include <util/format.hpp>
#include <util/logging.hpp>

#include <cstdio>
#include <memory>
#include <stdexcept>
#include <utility>

namespace sprites
{

namespace
{

const gfx::renderer::Color background = {40, 48, 72, 255};

gfx::renderer::Driver driver_for(const Options& options)
{
    return options.software ? gfx::renderer::Driver::Software
                            : gfx::renderer::Driver::PreferAccelerated;
}

// Uploads a sheet named by a Sparrow document, resolving it beside the other
// assets rather than relative to the document — the flat data/ layout makes
// those the same place, and rig::asset_path is what survives a launcher
// changing directory.
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

} // namespace

Demo::Demo(const Options& options)
    : _options(options)
    , _system()
    , _context("sprites", options.width, options.height, options.fullscreen,
               driver_for(options))
    , _pad()
    // A placeholder so the member is constructed before load() replaces it.
    // Atlas has no default constructor by design — it owns a Texture, and a
    // Texture without a renderer is not a thing — so this is the cost of
    // loading in the body rather than the initialiser list, which it has to be
    // because the animations resolve against the atlas.
    , _atlas(gfx::renderer::Texture(_context, 1, 1), gfx::Atlas::Frames())
{
    util::log_info("sprites: %dx%d, driver %s%s", _context.width(),
                   _context.height(), _context.driver_name().c_str(),
                   _context.accelerated() ? " (accelerated)" : "");
    util::log_info("sprites: input %s", _pad.description().c_str());

    load();
    if (_options.tilemap) {
        load_tilemap();
    }
}

void Demo::load_tilemap()
{
    const loaders::TmxMap map =
        loaders::load_tmx(rig::asset_path("sunnyland.tmx"));

    gfx::TileSet tiles(upload_sheet(_context, map.tileset.image),
                       map.tileset.tile_width, map.tileset.tile_height,
                       map.tileset.columns, map.tileset.tile_count,
                       map.tileset.margin, map.tileset.spacing);

    if (!tiles.fits_texture()) {
        util::log_warning("sprites: tileset claims %d tiles that do not fit "
                          "its %dx%d image",
                          map.tileset.tile_count, tiles.texture().width(),
                          tiles.texture().height());
    }

    _map.reset(
        new gfx::TileMap(std::move(tiles), map.layers, map.width, map.height));
    _map->set_object_layers(map.object_layers);

    util::log_info("sprites: map %dx%d tiles, %zu layers, %d tiles per screen "
                   "row",
                   map.width, map.height, map.layers.size(),
                   _context.width() / map.tile_width);
}

void Demo::load()
{
    const loaders::SparrowAtlas parsed =
        loaders::load_sparrow(rig::asset_path("foxy.xml"));

    _atlas =
        gfx::Atlas(upload_sheet(_context, parsed.image_path), parsed.frames);

    if (!_atlas.contains_frames()) {
        // Not fatal: it still draws, just wrongly. Saying so is the difference
        // between a five-minute diagnosis and an afternoon of squinting at
        // sprites that are almost right.
        util::log_warning("sprites: atlas frames run outside %dx%d sheet",
                          _atlas.texture().width(), _atlas.texture().height());
    }

    _animations =
        loaders::load_animation_set(_atlas, rig::asset_path("foxy.anim.xml"));

    util::log_info("sprites: %zu frames, %zu animations", _atlas.size(),
                   _animations.size());

    // One of each animation, laid out in a grid, every one running at its own
    // authored rate. This is the part that shows several things animating
    // independently off a single sheet.
    const int scale = _context.width() >= 480 ? 2 : 1;
    const int cell = 40 * scale;
    const int columns = _context.width() / cell;
    const int top = _context.height() / 3;

    for (gfx::AnimationSet::Index i = 0; i < _animations.size(); ++i) {
        game::Entity actor;
        actor.sprite = gfx::AnimatedSprite(_atlas, _animations[i]);
        actor.scale = scale;
        actor.x = static_cast<int>(i % columns) * cell + cell / 8;
        actor.y = top + static_cast<int>(i / columns) * cell;
        _entities.add(actor);
    }

    // And one large one walking across the top, to make the trim handling
    // visible: a wrong offset shows up as a sprite that bobs against its own
    // baseline as the frames change.
    _hero_animation = _animations.find("run");
    if (_hero_animation == gfx::AnimationSet::npos) {
        _hero_animation = 0;
    }
    game::Entity hero;
    hero.sprite = gfx::AnimatedSprite(_atlas, _animations[_hero_animation]);
    hero.scale = scale * 2;
    hero.y = top - 34 * hero.scale;
    // 60 pixels a second at 1x, so the walk reads the same on a 320-wide panel
    // as on a 640-wide one. The store integrates it.
    hero.velocity_x = 60.0 * hero.scale;
    // In front of the grid, which is what the layer is for.
    hero.layer = 1;
    _hero = _entities.add(hero);
}

void Demo::play_hero(const gfx::Animation& animation)
{
    if (game::Entity* hero = _entities.find(_hero)) {
        hero->sprite.play(animation);
    }
}

void Demo::step(double delta)
{
    _entities.update(delta);

    if (_map && _options.scroll) {
        // Across the map and back. Scrolling matters for the measurement: a
        // still camera lets a driver skip work between frames, and the number
        // wanted here is the cost of drawing a moving screenful.
        const double map_span =
            _map->pixel_width() * _options.tile_scale - _context.width();
        _camera += 80.0 * delta * _camera_direction;
        if (_camera > map_span) {
            _camera = map_span;
            _camera_direction = -1;
        } else if (_camera < 0.0) {
            _camera = 0.0;
            _camera_direction = 1;
        }
    }

    // Turn the hero around at the edges. The store integrates the position;
    // what it does not model is anything reacting to where that position ends
    // up, which is the game layer this deliberately stops short of.
    game::Entity* hero = _entities.find(_hero);
    if (hero == nullptr) {
        return;
    }

    const double span = _context.width() - 33.0 * hero->scale;
    if (hero->x > span) {
        hero->x = span;
        _hero_direction = -1;
    } else if (hero->x < 0.0) {
        hero->x = 0.0;
        _hero_direction = 1;
    }
    hero->velocity_x = 60.0 * hero->scale * _hero_direction;
}

void Demo::update(double delta)
{
    if (_animations.empty()) {
        return;
    }

    if (_pad.pressed(rig::Button::A) || _pad.pressed(rig::Button::Right)) {
        _hero_animation = (_hero_animation + 1) % _animations.size();
        play_hero(_animations[_hero_animation]);
        util::log_info("sprites: hero playing \"%s\"",
                       _animations[_hero_animation].name.c_str());
    } else if (_pad.pressed(rig::Button::Left)) {
        _hero_animation =
            (_hero_animation + _animations.size() - 1) % _animations.size();
        play_hero(_animations[_hero_animation]);
        util::log_info("sprites: hero playing \"%s\"",
                       _animations[_hero_animation].name.c_str());
    }

    step(delta);
}

void Demo::draw()
{
    _context.clear(background);

    if (_map) {
        _tiles_drawn = _map->draw(_context, static_cast<int>(_camera), 0,
                                  _options.tile_scale);
        _tiles_total += _tiles_drawn;
    }

    // Camera 0 for the entities: the grid and the hero are laid out in screen
    // space, and only the tilemap scrolls.
    _entities.draw(_context, 0, 0);
}

void Demo::run()
{
    rig::FrameClock clock(_options.target_fps);

    bool running = true;
    while (running) {
        _pad.begin_frame();

        SDL_Event event;
        while (SDL_PollEvent(&event) != 0) {
            _pad.handle_event(event);
        }

        if (_pad.quit_requested() || _pad.pressed(rig::Button::Start)) {
            running = false;
        }

        const double delta = clock.tick();
        update(delta);
        draw();
        _context.present();

        if (_options.seconds > 0.0 && clock.elapsed() >= _options.seconds) {
            running = false;
        }
    }

    util::log_info("sprites: %llu frames, %.1f fps average",
                   static_cast<unsigned long long>(clock.frames()),
                   clock.fps());

    if (_map && clock.frames() > 0) {
        const double per_frame = static_cast<double>(_tiles_total) /
                                 static_cast<double>(clock.frames());
        const double ms = clock.fps() > 0.0 ? 1000.0 / clock.fps() : 0.0;
        util::log_info(
            "sprites: tilemap %d layers, %.0f tiles/frame, %.3f ms/frame, "
            "%.2f us/tile",
            static_cast<int>(_map->layers().size()), per_frame, ms,
            per_frame > 0.0 ? ms * 1000.0 / per_frame : 0.0);
        std::printf("tiles/frame %.0f  ms/frame %.3f  fps %.1f  us/tile %.2f\n",
                    per_frame, ms, clock.fps(),
                    per_frame > 0.0 ? ms * 1000.0 / per_frame : 0.0);
    }
}

bool Demo::render_to_file(const std::string& path, int frames)
{
    // A fixed delta rather than a real clock: a screenshot taken over SSH
    // should show the same frame every time, and a wall clock would make it
    // depend on how loaded the machine was.
    const double delta = 1.0 / 60.0;

    for (int i = 0; i < frames; ++i) {
        step(delta);
        draw();

        // Before present(): SDL does not guarantee the back buffer survives the
        // swap, so reading after presenting can return undefined pixels.
        if (i + 1 == frames) {
            if (!_context.save_screenshot(path)) {
                util::log_error("sprites: could not write %s", path.c_str());
                return false;
            }
        }
        _context.present();
    }

    util::log_info("sprites: wrote %s after %d frames", path.c_str(), frames);
    return true;
}

} // namespace sprites
