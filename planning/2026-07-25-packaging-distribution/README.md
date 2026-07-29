# Packaging and distribution

**Status:** `snapshot`
**Written:** 2026-07-25
**Unblocked:** 2026-07-27 — see below. `coppers` gave the handheld targets
something worth putting on an SD card, and the bundle and the validation run are
now the same trip rather than one waiting on the other.
**Needs:** a Miyoo Mini Plus or Flip, and an SD card

## Motivation

A binary that builds is not a thing anyone can run. Each of the three
distribution channels expects a different shape, and none of them is "install to
`/usr/local`":

- **Handheld firmwares** launch from a self-contained directory with a shell
  entrypoint, discovered by scanning a specific path on the SD card. The exact
  layout differs per firmware.
- **Steam** wants a depot directory plus a launch configuration, built against
  the Steam Runtime.
- **Desktop** is the only one where a normal prefix install makes sense.

`cmake/Packaging.cmake` currently has the skeleton: `GNUInstallDirs`, a `data/`
install rule, a `wreel_add_handheld_bundle()` stub that writes a `launch.sh`, and
CPack configured for `TGZ`. It is deliberately thin because the target layouts are
guesses until hardware is in hand.

## Why this was blocked, and what changed

Defining a bundle layout for a binary that has never run on the device is
speculative work. Two things had to be known first, both from
[target-validation](../2026-07-25-target-validation/):

1. Whether the binary runs at all, and against which SDL2.
2. What the firmware expects — which is best learned by looking at how existing
   ports on that firmware are laid out.

**Neither is answered yet, but the first one is now askable.** As of 2026-07-27
there is something worth putting on an SD card: `coppers`
([2026-07-26-coppers-cracktro](../2026-07-26-coppers-cracktro/)) builds on
`miyoomini`, draws through `gfx::renderer`, and reports what it found. Before
that, the only demo needed a GPU the Miyoo Mini does not have, so "does the bundle
work" could not be asked at all.

So this stops being *blocked* and becomes *first*, in this order:

1. Build a bundle for one firmware — OnionOS on a Miyoo Mini Plus or Flip is the
   obvious first, since that is the hardware in hand.
2. Run the sequence in
   [target-validation/results.md](../2026-07-25-target-validation/results.md),
   which answers the display path, video driver, gamepad enumeration, audio spec
   and fill rate in one pass and writes them to a log.
3. Fix whatever that breaks, then generalise the layout to a second firmware.

Step 2 is why the bundle comes first rather than after validation: they are the
same trip.

### Read before starting

- **The Miyoo Mini Flip is 750×560, not 640×480**, on the same SSD202D as the
  Plus. Same preset, 37% more pixels, so it is the binding target for anything
  fill-rate bound and the more valuable of the two to test first.
  `docs/TARGETS.md § Target matrix` has the panel table.
- ~~**Whether upstream SDL2 runs on stock firmware is still the largest single
  unknown in the project**, and it is answered by whether the very first run
  works.~~ **Answered 2026-07-27, and it did not need the device.** It does not run:
  the pinned SDL2 builds `dummy`, `offscreen` and `wayland` for this target and SDL2
  has no framebuffer backend to build, so `WREEL_USE_SYSTEM_SDL2=ON` against the
  firmware's patched `mmiyoo` copy is mandatory rather than contingent. The decision
  did belong here, and is taken in
  [2026-07-27-onion-bundle](../2026-07-27-onion-bundle/) § decision 3.
- `coppers --screenshot` needs no display and no keyboard, so the first check is
  runnable over SSH before anything is known about the panel.

## Per-firmware layouts to establish

Each of these needs confirming against a real SD card, not documentation.

| Firmware | Devices | Expected shape |
|---|---|---|
| **OnionOS** | Miyoo Mini | **Established 2026-07-27** — `App/<Name>/` with `config.json` + `launch.sh`, implemented in [Packaging.cmake](../../cmake/Packaging.cmake). The Ports alternative (`Roms/PORTS/` across three trees) was considered and rejected; see [onion-bundle](../2026-07-27-onion-bundle/) § decision 1 |
| **muOS** | H700 Anbernics | `mnt/mmc/MUOS/application/<Name>/` |
| **ArkOS** | RK3326 | `roms/ports/<name>/` plus a `.sh` in `ports/` |
| **ROCKNIX** / Batocera | both | `/userdata/roms/ports/` |

Common requirements across all of them:

- ~~Assets resolved **relative to the executable**, not the working directory.~~
  **Done 2026-07-27** as `rig::asset_path()`, in the new `wreel::rig` module. It
  prefers `$WREEL_DATA_DIR`, then `data/` beside the executable, then the old
  working-directory behaviour, and logs which rule won — so a device whose bundle
  is laid out wrong says so instead of looking like one with missing assets.
  Verified the way this document asked for: `cmake --install` to a prefix, then
  `skratch --screenshot` launched from `/tmp`, which resolved `bin/data/` and
  rendered. The `install(DIRECTORY data/ DESTINATION bin/data)` rule in
  `cmake/Packaging.cmake` is what that relies on, so it is now load-bearing for a
  reason it can state rather than as a workaround.
- ~~`runlog.txt` is written to the current directory by `skratch/main.cc`.~~
  **Done 2026-07-26.** The log goes to `SDL_GetPrefPath("wreel", "skratch")`, which
  also took `<fstream>` out of a shipped executable — it had its own
  `std::ofstream logging` global, against docs/TARGETS.md § 1a.
- ~~No dynamic libraries to install — everything is static by design, which is one
  problem packaging does *not* have here.~~ **False on `miyoomini` as of
  2026-07-27**, and it was never going to survive contact with that device. Upstream
  SDL2 has no video driver for the SSD202D — no framebuffer backend exists in SDL2
  at all — so the core SDL2 there is the firmware's patched `mmiyoo` copy, loaded as
  a shared object from the bundle's `lib/`. Evidence and consequences in
  [2026-07-27-onion-bundle](../2026-07-27-onion-bundle/).

  It remains true everywhere else, and the two parts of it that were *reasoned*
  rather than accidental both still hold: `gfx::gles2` resolves its entry points
  through `SDL_GL_GetProcAddress` rather than linking `libGLESv2`, so binaries list
  only `libm` and `libc` as `NEEDED`; and linking it would have meant a 2D-only game
  failing to *start* on a firmware with no GLES blob, since the loader resolves
  `DT_NEEDED` before `main()`. Note the shape of the correction: the claim held for
  the reason given, and was falsified by a different one.

  SDL2_image, SDL2_ttf and SDL2_mixer stay static even there — they consume the
  SDL2 API, not the display — so the bundle gains one shared object rather than
  five, and libxmp, `stb_image` and FreeType stay pinned.

## Steam depot

- Build inside the `sniper` container (`cmake --preset steam`)
- Verify glibc symbol requirements: `objdump -T build/steam/bin/skratch | grep -o 'GLIBC_[0-9.]*' | sort -Vu`
- Depot layout: binary + `data/` + a launch script that sets `LD_LIBRARY_PATH`
- Steam wants a `steam_appid.txt` during development
- Controller support: Steam Input will remap, but the raw joystick handling in
  `skratch/input.cc` should move to `SDL_GameController` first so mappings come
  from SDL's database rather than hard-coded Xbox 360 axis indices

## Prerequisites carried in from the coppers work

Three things known to be outstanding, recorded here because this is the snapshot
that has to care about them.

- [ ] **`data/glyphs-16x16.png` has no licence and cannot ship.** The collection
      it came from carries no LICENSE and its curator states they do not know its
      provenance. Fine for development, an actual problem in a store build. Either
      identify the author and obtain terms or substitute a CC0/OFL sheet — see
      [`data/PROVENANCE.md`](../../data/PROVENANCE.md), which also records the
      honest "unknown" rows for the 2016 assets and flags the tracker modules as
      the sharper case, since tracker authors are normally named. Swapping the
      sheet is a data change rather than a code change **only until something
      depends on that specific grid**, so it is cheaper now than later.
- [ ] **Every shipped asset needs a known licence**, not just the glyph sheet.
      `data/PROVENANCE.md` is the checklist; most rows currently say "unknown".
- [ ] **`skratch/input.cc` still hard-codes Xbox 360 axis indices.** `rig::Pad`
      exists and `coppers` uses it, so the port is mechanical — but it is a change
      to a working demo and wants its own commit. Only blocking if `skratch` is
      shipped; `coppers` is unaffected.

## Tasks

- [x] Move asset resolution to `SDL_GetBasePath()` — done 2026-07-27, as
      `rig::asset_path()` rather than a `util` helper. `util` links no SDL
      deliberately, so the realtime-services module was created for this and for
      frame timing; see docs/TARGETS.md § Modules
- [x] Move `runlog.txt` to `SDL_GetPrefPath()` — done 2026-07-26 with the skratch port
- [~] Confirm one firmware's layout against a real device, and implement it.
      **Implemented 2026-07-27 for OnionOS** — `App/Coppers/` with `config.json` and
      `launch.sh`, taken from real packages in OnionUI/Onion rather than from a
      guide, and verified on the dev box: a bundle-shaped tree launched from `/`
      logs `assets: … (beside the executable)` and writes its log inside the bundle.
      **Not confirmed against a device**, which is the half that remains
- [ ] Flesh out `wreel_add_handheld_bundle()` for the remaining firmwares. Onion is
      done; muOS, ArkOS and ROCKNIX are still guesses and stay unwritten
- [x] Add a `WREEL_TARGET_FIRMWARE` option if layouts diverge enough to need it —
      added 2026-07-27. Justified sooner than expected: one target id can boot more
      than one firmware, since a Miyoo Mini runs stock or Onion and the Flip may run
      something else again on the same SSD202D. `onion` and `none`; anything else is
      rejected by name rather than silently ignored
- [ ] Verify the Steam build's glibc floor
- [~] Port input to `SDL_GameController`. **Half done 2026-07-27**: `rig::Pad`
      exists and `coppers` uses it — `SDL_GameController` with a raw-joystick
      fallback, keyboard equivalents, and full enumeration logged.
      `skratch/input.cc` still hard-codes Xbox 360 axis indices and has *not* been
      ported; that is a change to a working demo and wants its own commit
- [x] Decide whether CPack is the right tool or a plain `install()` + `tar` is
      simpler for handheld bundles — **plain staging plus `tar`, decided
      2026-07-27**. CPack's value is metadata and generators, and no handheld
      firmware consumes either; Onion's own distribution format is a `.7z` of
      exactly the directory tree. CPack stays for source and desktop tarballs and is
      out of the handheld path

## Open questions

- Is a single bundle-per-firmware worth it, or should there be one generic bundle
  plus per-firmware launcher shims? The latter is less duplication but relies on
  every firmware tolerating the same directory layout.
- Does the Steam release ship `software` or a GL backend? If the answer is
  "software", the entire `gl33` backend becomes unnecessary and
  [graphics-backends](../2026-07-25-graphics-backends/) shrinks considerably.

## References

- `cmake/Packaging.cmake` — the current skeleton
- [docs/TARGETS.md](../../docs/TARGETS.md)
