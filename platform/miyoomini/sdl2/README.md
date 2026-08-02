# The Miyoo Mini's SDL2 drivers

The SSD202D video, render and audio drivers, compiled into this project's pinned
upstream SDL2. Where they came from, what we changed and why is
[PROVENANCE.md](PROVENANCE.md) — read that first if the question is licensing or
correctness. This file is how the build works.

## What is here

```
src/video/mini/     SDL_video_mini.c   the MI_GFX context, the fbdev present, display modes
                    SDL_event_mini.c   evdev on a thread; the pad is a keyboard here
                    SDL_fb_mini.c      window framebuffer — what makes SDL's software renderer present
src/render/mini/    SDL_render_mini.c  the "Miyoo Mini" render backend
src/audio/mini/     SDL_audio_mini.c   MI_AO
patches/            0001-register-mini-drivers.patch
graft.cmake         copies src/ into the fetched SDL2 and applies the patch
```

The sources are not upstream SDL2's and are not ours either: they are
steward-fu's, LGPL-2.1, listed with their original blob hashes in PROVENANCE.md
so the import is verifiable byte-for-byte.

## How it builds

`WREEL_MINI_SDL2` — defaulted ON by `cmake/toolchains/miyoomini.cmake`, and
meaningless elsewhere, since no other target has an MI SDK to compile against.
When it is on, `cmake/Dependencies.cmake` declares SDL2 with a `PATCH_COMMAND`
that runs `graft.cmake` after FetchContent populates the tree:

1. `src/` is copied over SDL2's `src/`, adding `{video,render,audio}/mini/` and
   touching nothing else.
2. `patches/0001-register-mini-drivers.patch` is applied — 46 inserted lines
   across 8 upstream files, none removed or changed. It adds an `SDL_MINI`
   option, a build block, three `#cmakedefine` entries, and one bootstrap-array
   entry plus one `extern` for each of video, render and audio.

Both steps are idempotent: `PATCH_COMMAND` is not guaranteed to run exactly once,
and a reconfigure after an interrupted populate would otherwise fail on an
already-applied patch.

### Editing a driver afterwards

`PATCH_COMMAND` runs when the tree is *populated*, and never again — so on its
own it would graft these sources once and then ignore every later edit, silently
building the drivers as they were the day the tree was fetched.
`_wreel_sync_mini_drivers()` in [cmake/Dependencies.cmake](../../../cmake/Dependencies.cmake)
closes that: it copies `src/` into the populated tree at **configure** time, just
before `FetchContent_MakeAvailable`, with `configure_file(... COPYONLY)`.

Two consequences worth knowing:

- **There is nothing to run.** Each driver source becomes a dependency of the
  build system, so editing one makes `cmake --build` re-run CMake, re-copy and
  rebuild `libSDL2` by itself. `bundle-onion` already depends on the `SDL2`
  target and re-copies `$<TARGET_FILE:SDL2>`, so the rebuilt library reaches the
  staged bundle and the tarball with no extra step.
- **It cannot be a build-time target**, which is the obvious shape and the wrong
  one. Ninja decides what is dirty before it runs the first command, so a copy
  performed during the build is seen one build too late.

A tree populated *without* the graft — fetched before this existed, or with
`WREEL_MINI_SDL2=OFF` — keeps its stamp and will never be patched now. The sync
stops the configure and says to delete it, because that tree otherwise builds a
`libSDL2` that links fine and has no video driver for this panel.

The vendor SDK needs no vendoring. The toolchain sysroot's `mi_gfx.h`,
`mi_sys.h`, `mi_common.h` and `mi_ao.h` are byte-identical to the copies in
steward-fu's tree, and the matching `libmi_*.so` are there too.

```sh
docker run --rm -v "$PWD":/src -w /src wreel-miyoomini \
    bash -c 'cmake --preset miyoomini && cmake --build build/miyoomini -j4'
```

## Regenerating the patch when the SDL2 pin moves

`graft.cmake` fails the build rather than forcing a patch that no longer fits,
because a half-applied registration produces link errors a long way from the
cause. To move the pin:

1. Configure once with `WREEL_MINI_SDL2=OFF` so the tree is populated unpatched.
2. Redo the eight edits by hand — the existing patch is the specification, and
   every hunk is an insertion.
3. Regenerate:

   ```sh
   cd build/<preset>/_deps/sdl2-src
   git -c safe.directory="$PWD" diff > <repo>/platform/miyoomini/sdl2/patches/0001-register-mini-drivers.patch
   ```

   The `safe.directory` override is needed because the container creates that
   tree as root.

4. Re-check the backend signatures. Six changed between 2.0.20 and 2.32.10 and
   the survey that found five of them missed one, so this is a compile-and-read
   exercise rather than a diff-the-header one. PROVENANCE.md lists what moved.

## Verifying a change on the device

`wreel-diag` is the regression suite. It draws known content and reads
`/dev/fb0` back, so it measures what reached the panel rather than what SDL
returned:

```sh
cmake --build build/miyoomini --target bundle-onion
# copy pkg/coppers-*-onion.tar.gz onto the card, run "Wreel Diagnostics"
# from the Apps menu, then read App/WreelDiag/diag.txt
```

Run it before and after a driver change and diff the two reports. The same
binary on `desktop-software` runs against SDL's own software renderer and is the
control: a line that reads OK there and IGNORED here is a gap in this driver, and
one that reads IGNORED in both is a bug in the check.

Findings so far, and the traps that produced wrong ones first, are in
[planning/2026-07-31-miyoo-sdl2-fork](../../../planning/2026-07-31-miyoo-sdl2-fork/)
§ 8.
