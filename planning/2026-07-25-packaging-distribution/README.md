# Packaging and distribution

**Status:** `snapshot`
**Written:** 2026-07-25
**Blocked by:** [target-validation](../2026-07-25-target-validation/)

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

## Why this is blocked

Defining a bundle layout for a binary that has never run on the device is
speculative work. Two things must be known first, both from
[target-validation](../2026-07-25-target-validation/):

1. Whether the binary runs at all, and against which SDL2.
2. What the firmware expects — which is best learned by looking at how existing
   ports on that firmware are laid out.

## Per-firmware layouts to establish

Each of these needs confirming against a real SD card, not documentation.

| Firmware | Devices | Expected shape |
|---|---|---|
| **OnionOS** | Miyoo Mini | `Roms/` or `App/<Name>/` with `config.json` + `launch.sh` |
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
- No dynamic libraries to install — everything is static by design, which is one
  problem packaging does *not* have here. **This survived the GL work
  deliberately**: `gfx::gles2` resolves its entry points through
  `SDL_GL_GetProcAddress` rather than linking `libGLESv2`, so binaries still list
  only `libm` and `libc` as `NEEDED`. Linking it would also have meant a 2D-only
  game failing to *start* on a firmware with no GLES blob, since the loader resolves
  `DT_NEEDED` before `main()`.

## Steam depot

- Build inside the `sniper` container (`cmake --preset steam`)
- Verify glibc symbol requirements: `objdump -T build/steam/bin/skratch | grep -o 'GLIBC_[0-9.]*' | sort -Vu`
- Depot layout: binary + `data/` + a launch script that sets `LD_LIBRARY_PATH`
- Steam wants a `steam_appid.txt` during development
- Controller support: Steam Input will remap, but the raw joystick handling in
  `skratch/input.cc` should move to `SDL_GameController` first so mappings come
  from SDL's database rather than hard-coded Xbox 360 axis indices

## Tasks

- [x] Move asset resolution to `SDL_GetBasePath()` — done 2026-07-27, as
      `rig::asset_path()` rather than a `util` helper. `util` links no SDL
      deliberately, so the realtime-services module was created for this and for
      frame timing; see docs/TARGETS.md § Modules
- [x] Move `runlog.txt` to `SDL_GetPrefPath()` — done 2026-07-26 with the skratch port
- [ ] Confirm one firmware's layout against a real device, and implement it
- [ ] Flesh out `wreel_add_handheld_bundle()` per firmware
- [ ] Add a `WREEL_TARGET_FIRMWARE` option if layouts diverge enough to need it
- [ ] Verify the Steam build's glibc floor
- [~] Port input to `SDL_GameController`. **Half done 2026-07-27**: `rig::Pad`
      exists and `coppers` uses it — `SDL_GameController` with a raw-joystick
      fallback, keyboard equivalents, and full enumeration logged.
      `skratch/input.cc` still hard-codes Xbox 360 axis indices and has *not* been
      ported; that is a change to a working demo and wants its own commit
- [ ] Decide whether CPack is the right tool or a plain `install()` + `tar` is
      simpler for handheld bundles

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
