# Test suite.
#
# wreel_add_test() (cmake/Testing.cmake) is the modern form of the original
# `test_module` macro, and keeps both of its important properties: one
# executable per test, and the repository root as the working directory so
# data/ fixture paths resolve.

# --- Currently building ------------------------------------------------------
#
# These need no graphics context, so they run on every target — including under
# qemu-user-static for cross builds.

wreel_add_test(test_ascii   LIBS wreel::util)
wreel_add_test(test_file    LIBS wreel::util wreel::posix)
wreel_add_test(test_logging LIBS wreel::util wreel::posix)
wreel_add_test(test_json    LIBS wreel::util)
wreel_add_test(test_number  LIBS wreel::util)
wreel_add_test(test_integer LIBS wreel::util)
# Header-only, but it links wreel::gfx for the Rect predicates in
# gfx/renderer/types.hpp — which needs no renderer, only the header.
wreel_add_test(test_algorithm LIBS wreel::util wreel::gfx)
wreel_add_test(test_obj     LIBS wreel::loaders wreel::posix)
wreel_add_test(test_string  LIBS wreel::util)
# Parses only — no Context, no texture, no image load — which is the property
# that keeps it in this group. See include/loaders/sparrow.hpp.
wreel_add_test(test_sparrow LIBS wreel::loaders wreel::posix)
wreel_add_test(test_xml     LIBS wreel::util wreel::posix)
wreel_add_test(test_audio   LIBS wreel::audio)

# wreel::rig links SDL2, but neither of these initialises a subsystem:
# FrameTiming is arithmetic over supplied timestamps, and asset resolution is
# SDL_GetBasePath plus a stat.
wreel_add_test(test_assets  LIBS wreel::rig)
wreel_add_test(test_timing  LIBS wreel::rig)

# Free functions over a TileLayer and two rectangles, so despite linking
# wreel::game — and through it wreel::gfx — it opens no window, loads no
# texture and reads no fixture. The layers are built in the test.
wreel_add_test(test_collide LIBS wreel::game)

# Arithmetic over supplied positions — the camera is told where its target is
# rather than reading one off an entity store, so this needs no context either.
wreel_add_test(test_camera  LIBS wreel::game)

# Sits here rather than with test_atlas because game::Level takes parsed layers
# instead of a gfx::TileMap, which is the whole reason it does: a TileMap owns a
# texture and would have pulled a Context into what is validation arithmetic.
# The layers are built in the test, so no .tmx is read either.
wreel_add_test(test_level   LIBS wreel::game)

# The most headless test here: game::ActionMap is a template over a documented
# Source concept, so the fixture is a stub written in the test rather than a
# rig::Pad. It links wreel::game only for action_name and the constructor — no
# device is opened, and rig::Button is a plain enum in a header.
wreel_add_test(test_action  LIBS wreel::game)

# coppers/bars.cc is compiled into the test rather than linked, because the demo
# is an executable and its sources are not a library. Worth a library if a second
# consumer ever appears; not worth one for a single test.
wreel_add_test(test_bars
               SOURCES test_bars.cc ../coppers/bars.cc
               LIBS wreel::gfx)

# Loads the real data/glyphs-16x16.png. Needs no video subsystem: IMG_Load does
# not require one, so the whole glyph path is covered headlessly and under qemu.
wreel_add_test(test_glyphs
               SOURCES test_glyphs.cc ../coppers/glyphs.cc
               LIBS wreel::loaders wreel::rig wreel::gfx)

# Opens the mixer under the dummy audio driver and loads the real .mod files, so
# it covers libxmp actually accepting and rejecting them rather than a stand-in.
wreel_add_test(test_playlist
               SOURCES test_playlist.cc ../coppers/playlist.cc
               LIBS wreel::audio wreel::rig)

# Needs a video subsystem, so it runs under SDL_VIDEODRIVER=dummy like the audio
# tests run under the dummy audio driver.
wreel_add_test(test_renderer LIBS wreel::gfx)

# Same, and for a stronger reason: half of it reads back rendered pixels through
# Context::save_screenshot to assert which atlas cell landed where. That is the
# assertion test_renderer.cc could not make before Texture and the source-rect
# draw existed.
wreel_add_test(test_atlas    LIBS wreel::gfx wreel::loaders)

# Playback is arithmetic over supplied deltas, so most of this needs nothing —
# but the cases that resolve animation names against the real atlas need a
# texture, and therefore a Context.
wreel_add_test(test_animation
               LIBS wreel::gfx wreel::loaders wreel::posix)

# Parser cases are headless; the culling cases need a Context, because "drew the
# whole map" and "drew the visible rectangle" look identical on screen and only
# differ in the tile count draw() returns.
wreel_add_test(test_tmx      LIBS wreel::gfx wreel::loaders wreel::posix)

# Links the store rather than compiling it in, because it is a module now and
# not a source file inside one demo. A third of its cases need a Context — the
# draw counts, the pixel-rounding cases and the two that pin draw order by
# reading back which colour landed on top — which is why it sits with test_atlas
# rather than in the headless group.
wreel_add_test(test_entities LIBS wreel::game)

# --- Priority queue ----------------------------------------------------------
#
# test_string.cc above is the important one and is not yet complete.
# include/util/string.hpp is 688 lines — a fifth of the codebase — and its
# std::ptr_fun usage must be removed for C++17. Its only consumer is
# loaders/obj.cc, so a broken tokenizer edge case shows up as malformed geometry
# rather than a compile error. Cover it *before* that refactor.
#
# Still to write:
#   test_errors   — posix::wrap() errno dispatch across the generated types
#
# See docs/TARGETS.md § "Where the test surface actually is".
