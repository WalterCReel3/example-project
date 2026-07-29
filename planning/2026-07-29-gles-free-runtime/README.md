# Shipping the Miyoo Mini bundle without a GL stack

**Status:** `snapshot`
**Written:** 2026-07-29
**Blocked by:** nothing
**Serves:** [onion-bundle](../2026-07-27-onion-bundle/), and any future handheld
bundle built on the same runtime

## Motivation

The OnionOS bundle is 30 MB staged and 13 MB compressed. **21.8 MB of that is
`libGLESv2.so` — a SwiftShader software OpenGL implementation that nothing in
this project calls.** `coppers` renders through `gfx::renderer`; `gfx::gles2` is
excluded by construction on this target, since `WREEL_ENABLE_GLES2` is rejected
for a device with no GPU.

It is in the bundle because the vendored `libSDL2-2.0.so.0` declares it as a
`DT_NEEDED`, and the loader resolves those before `main()`. So a demo that never
issues a GL call cannot start without 21.8 MB of GL sitting beside it.

This snapshot is about removing it — and the evidence below says that is much
cheaper than it sounds.

---

## Verified before planning

All of this is re-checkable without a device; the commands are in §5.

### 1. `libSDL2` references **zero** symbols from `libGLESv2.so`

```console
$ readelf --dyn-syms -W libSDL2-2.0.so.0 | awk '$7=="UND"{print $8}' | sed 's/@.*//' | sort -u > und.txt
$ readelf --dyn-syms -W libGLESv2.so | awk '$4=="FUNC" && $7!="UND"{print $8}' | sort -u > prov.txt
$ comm -12 und.txt prov.txt
(nothing)
```

The `DT_NEEDED` is a **link-time artefact** — `-lGLESv2` passed without
`--as-needed` — not a dependency. 21.8 MB is loaded to satisfy a reference that
does not exist.

### 2. `libEGL.so` is a real dependency, and a small one

Eleven symbols, all EGL entry points plus one non-standard hook:

```
eglCreateContext  eglCreateWindowSurface  eglGetConfigAttrib  eglGetConfigs
eglGetDisplay     eglGetProcAddress       eglInitialize       eglMakeCurrent
eglSwapBuffers    eglTerminate            eglUpdateBufferSettings
```

`eglUpdateBufferSettings` is the one that matters: `Mini_CreateWindow` calls it
at window creation to register the `GFX_CB` presentation callback. The other ten
are only reached when a GL context is created, which this project never does on
this target. `libEGL.so` is **55 KB**.

### 3. `libGLESv2` is reached by `dlopen`, not by linkage

`libEGL.so` does **not** list it as `DT_NEEDED`. It opens it by name on demand:

```console
$ strings -a libEGL.so | grep -i libGLESv2
libGLESv2.so
libGLESv2.so.2
libGLESv2_swiftshader
```

So removing the `DT_NEEDED` from `libSDL2` does not break EGL — it defers the
GL library to the moment something asks for a GL context. Nothing here does, and
nothing here *can*: `gfx::gles2` is not compiled for this target.

### 4. `libjson-c.so.5` is genuinely used

Four symbols (`json_object_from_file`, `json_object_get_int`,
`json_object_object_get_ex`, `json_object_put`). 51 KB. Stays.

---

## Options

| | Approach | Bundle after | Effort | Keeps |
|---|---|---|---|---|
| **A** | Remove the unused `DT_NEEDED` from the shipped `libSDL2` | **~8.4 MB** (3 MB tarball) | minutes | vendor binary otherwise untouched |
| **B** | Rebuild the mmiyoo SDL2 with its GL backend compiled out | ~8.3 MB | days | full source control, no EGL at all |
| **C** | Ship stub `libEGL.so` / `libGLESv2.so` | ~8.4 MB | hours | no vendor GL binaries at all |
| **D** | Leave it | 30 MB | none | — |

**Recommended: A now, C or B later only if there is a reason beyond size.**

A removes 99.7% of the waste for a one-line build step. B and C then contest the
remaining 55 KB, which is not a size argument — it would be a licensing or
provenance one, and it should be made on those terms rather than smuggled in as
an optimisation.

### Why A is not a hack

It removes a reference to a library that is provably never resolved through that
reference. The loader's own `--as-needed` would have removed it at link time had
it been used; the vendor simply did not pass the flag. The check in §1 is exactly
the check `--as-needed` performs.

What makes it *feel* like a hack is that it edits a third-party binary. That is
answerable: do it as a **build step with the evidence attached**, not as a
one-off `patchelf` someone ran once and forgot. See the tasks.

### What B would actually involve

Recorded so the cost is not underestimated if it is chosen later:

- `steward-fu/sdl2` has `configure.ac` but **no committed `configure`**, and
  neither the host nor the buster toolchain container has autoconf — and this
  project deliberately does not run `apt` against buster's archive. So it needs
  `autoreconf` in a modern container, then `configure`/`make` in the union one.
- Their `Makefile.mk` already passes `--disable-video-opengl`,
  `--disable-video-opengles` and `--disable-video-opengles2`, and the prebuilt
  still links EGL — so the GL entry points in `SDL_video_mini.c` are not gated by
  those switches. Removing them means patching the driver: dropping the
  `device->GL_*` assignments and the `eglUpdateBufferSettings` call in
  `Mini_CreateWindow`.
- **That last call is load-bearing and must be checked, not assumed.** It
  registers `GFX_CB`, the window-framebuffer presentation callback. Our path
  (`Mini_QueueCopy` → `GFX_Copy` → `GFX_Flip`) does not use it, so removing it
  should be harmless — but "should be" is what this project has been wrong about
  three times on this device.
- The result is a patched vendor library we maintain, which needs its patch
  recorded and its provenance pinned like every other dependency.

---

## Tasks

**Stage 1 — drop the unused dependency**

- [ ] Add the removal to `wreel_add_handheld_bundle()` as a build step, not a
      manual one. `patchelf --remove-needed libGLESv2.so` over the staged copy,
      leaving `WREEL_ONION_SDL2_RUNTIME`'s source directory untouched
- [ ] **Guard it with the check that justifies it**: fail the build if the
      library turns out to reference any symbol from `libGLESv2`, so this cannot
      silently become wrong when the runtime is updated
- [ ] Make `patchelf` an optional tool — if absent, skip the step with a status
      message and ship the larger bundle, rather than failing the build
- [ ] Record it in `docs/MIYOO-MINI.md` beside the rest of the runtime's
      anatomy, with the symbol evidence

**Stage 2 — verify on the device**

- [ ] `readelf -d` on the staged binary: no `libGLESv2` in `NEEDED`
- [ ] The bundle's `lib/` holds three files, not four
- [ ] It runs: bars, scroller and HUD as before. **This is the whole test** —
      if the loader wanted that library for a reason the symbol table did not
      show, the program will not start and the failure is immediate and obvious
- [ ] Record the before/after sizes in
      [target-validation/results.md](../2026-07-25-target-validation/results.md)

**Stage 3 — pin what we ship**

Carried over from [onion-bundle](../2026-07-27-onion-bundle/), and this is the
natural time:

- [ ] Record the runtime's origin, version and licence in `docs/TARGETS.md`
      alongside every other pinned dependency. Right now it is "whatever was in
      that directory" — `steward-fu/sdl2`, `prebuilt/640x480/`, SDL 2.0.20,
      LGPL-2.1
- [ ] Add a `THIRD-PARTY.md` for shipped binaries. `data/PROVENANCE.md` covers
      assets; a vendored LGPL library is not an asset and has different
      obligations — dynamic linking plus attribution plus the means to relink,
      which is exactly what stage 1's build step must not quietly break

---

## Verification

After stage 1 the following must all hold, and the first two are the ones that
would catch a mistake:

```sh
# nothing references the removed library
comm -12 <(readelf --dyn-syms -W lib/libSDL2-2.0.so.0 | awk '$7=="UND"{print $8}' | sed 's/@.*//' | sort -u) \
         <(readelf --dyn-syms -W libGLESv2.so | awk '$4=="FUNC" && $7!="UND"{print $8}' | sort -u)

# and it is no longer demanded at load time
readelf -d lib/libSDL2-2.0.so.0 | grep NEEDED

# our own binary still resolves everything it imports
nm -D --undefined-only bin/coppers | awk '{print $2}' | grep -E '^(SDL_|IMG_|TTF_|Mix_)' \
  | comm -23 - <(readelf --dyn-syms -W lib/libSDL2-2.0.so.0 | awk '$7!="UND"{print $8}' | sed 's/@.*//' | sort -u)
```

Expected sizes: staged bundle **30 MB → ~8.4 MB**, tarball **13 MB → ~3 MB**.

---

## Risks

**Something requests a GL context anyway.** Then `libEGL` `dlopen`s
`libGLESv2.so`, fails to find it, and GL initialisation fails at that point
rather than at load. On this target nothing can: `gfx::gles2` is not compiled and
`WREEL_ENABLE_GLES2` is rejected. The failure mode is also loud rather than
subtle.

**A future runtime actually uses it.** Which is why stage 1's guard is a task
rather than a nicety: the removal must be conditional on the symbol check
passing, evaluated at build time against the library actually being shipped.

**Editing a vendored LGPL binary.** Not a licence problem in itself — the
obligation is source availability and the ability to relink, and removing an
unused `DT_NEEDED` obstructs neither. It does mean the modification must be
documented rather than silent, which stage 3 covers.

**The Flip.** Unchanged by this work: it needs a runtime built for 752×560
regardless, and whatever that build is will need the same treatment. Worth
checking the symbol evidence again for that binary rather than assuming it
matches.

---

## Open questions

- **Is the same true of the other vendor runtimes?** Onion's `parasyte` SDL2 and
  XK9274's fork have not been checked for unused `DT_NEEDED` entries. If the
  pattern holds, it is worth saying so in `docs/MIYOO-MINI.md` as a general
  observation about these builds rather than a fact about one file.
- **Should the bundle carry `wreel-probe` as well?** It is small, it answers
  questions on a device that nothing else does, and it is currently not shipped —
  which meant the first device runs could not enumerate render drivers when that
  was exactly the open question.
- **Does `eglUpdateBufferSettings` matter to our path at all?** If it does not,
  option C's stub becomes trivial and option B's patch becomes small. Answerable
  by reading `SDL_gles_mini.c`, which has not been read yet.

## References

- [docs/MIYOO-MINI.md](../../docs/MIYOO-MINI.md) — the runtime's anatomy and
  where these libraries come from
- [onion-bundle](../2026-07-27-onion-bundle/) — the bundle this shrinks, and
  decision 3 for why the runtime is vendored at all
- [steward-fu/sdl2](https://github.com/steward-fu/sdl2) — source of the
  prebuilt, LGPL-2.1
