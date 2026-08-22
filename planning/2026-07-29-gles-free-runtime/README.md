# Shipping the Miyoo Mini bundle without a GL stack

**Status:** `done` — closed 2026-08-01, the goal met at source rather than by patching

> **Closed, and by a route this document argued against.** Stages 1–3 landed as
> written and took the bundle from 30 MB to 8.8 MiB by removing an unused
> `DT_NEEDED` from a binary we did not build. Since 2026-08-01 that binary is
> gone: this project compiles its own `libSDL2-2.0.so.0` from pinned upstream
> 2.32.10 with the SSD202D drivers grafted in, and the GL path is simply not
> compiled.
>
> So there is no `libGLESv2.so` to drop and no `libEGL.so` to stub. Confirmed on
> the device — the loader maps `libSDL2-2.0.so.0` and the firmware's `libmi_*`,
> and nothing else. `lib/` is one file and the bundle is **4.7 MB**.
>
> `scripts/drop-unused-needed.sh` and the `WREEL_ONION_DROP_GLES` option stay:
> they still apply to `WREEL_ONION_SDL2_RUNTIME`, the escape hatch for staging
> somebody else's prebuilt. They are simply not on the default path any more.
>
> The reasoning that dated fastest is worth naming. This document priced option
> B at "~8.3 MB, days" — 55 KB better than option A for days of work — because
> it assumed a rebuild bought only the removal of EGL. It bought the removal of
> EGL, GLESv2 **and** json-c, a library whose headers match its binary, a live
> upstream, and the ability to fix the driver at all. See
> [miyoo-sdl2-fork](../2026-07-31-miyoo-sdl2-fork/).
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

`libEGL.so` is **55 KB**, and **none of the eleven is reached on this target**.

> **Corrected 2026-07-31.** This paragraph used to read: "`eglUpdateBufferSettings`
> is the one that matters: `Mini_CreateWindow` calls it at window creation to
> register the `GFX_CB` presentation callback. The other ten are only reached when
> a GL context is created, which this project never does on this target."
>
> The second sentence was right about ten symbols and the first was wrong about
> the eleventh. `Mini_CreateWindow` calls **`glUpdateBufferSettings`**, a static
> function in the port's own `SDL_gles_mini.c` which stores the callback in a
> file-scope variable and returns 0. The EGL import `eglUpdateBufferSettings` is
> called from `glCreateContext` and nowhere else. Two names one letter apart, and
> the wrong one was attributed — from a symbol list, without reading the call
> site. Evidence in
> [miyoo-sdl2-fork § 1.7](../2026-07-31-miyoo-sdl2-fork/).
>
> This makes option C's stub **safe rather than hopeful**: eleven functions that
> return failure satisfy a dependency nothing calls. It does not change stage 1's
> outcome, which was about `libGLESv2.so`.

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

> **Revised 2026-07-31: that licensing argument now exists, and C answers it.**
> The § 2 correction shows no symbol of `libEGL.so` is called on this target, so
> the stub is eleven functions returning failure and cannot break a path that
> never runs. [THIRD-PARTY.md](../../THIRD-PARTY.md)'s unknown-licence item is
> [packaging-distribution](../2026-07-25-packaging-distribution/)'s blocker for
> anything that counts as distribution, and **C removes it in an afternoon
> without a rebuild.** So: A landed; **C when distribution matters**; B on the
> merits in [miyoo-sdl2-fork](../2026-07-31-miyoo-sdl2-fork/), which are no
> longer about EGL.

> **Option B was mis-costed, 2026-07-31.** The table prices B at "~8.3 MB, days"
> — i.e. 55 KB better than A for days of work — because it treats a rebuild as
> buying nothing but the removal of EGL. That was written on the understanding
> that the vendor render backend implements one operation because `MI_GFX` offers
> one operation. It does not: the SDK exports `MI_GFX_QuickFill`, and `BitBlit`'s
> per-call options carry rotation, mirroring, clipping, colour-keying and eleven
> blend operands, none of which the port plumbs through. See
> [docs/MIYOO-MINI.md § 4.6](../../docs/MIYOO-MINI.md).
>
> So B's real column is not 55 KB. It is `SDL_RenderFillRect`, correct
> sub-rectangle placement (D25), hardware blend modes, a texture cap that is a
> `#define` rather than a limit, and possibly the removal of a per-frame staging
> `memcpy` that exists only because GFX surfaces are physical addresses.
>
> **This does not change this snapshot's recommendation**, which is about bundle
> size and where A remains right. It moves the argument for B out of here
> entirely: a rebuild is now a capability decision, and it is scoped as one in
> [2026-07-31-miyoo-sdl2-fork](../2026-07-31-miyoo-sdl2-fork/). The "what B would
> actually involve" notes below remain accurate and are the practical starting
> point for it.

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
  still links EGL. **Why, established 2026-07-31:** `configure.ac`'s
  `CheckMiniVideo()` appends `-L. -lEGL -lGLESv2` to `EXTRA_LDFLAGS`
  unconditionally, so the `--disable-video-opengl*` switches never had anything
  to do with it. Dropping those two flags and not compiling `SDL_gles_mini.c` is
  the whole of the removal; the `device->GL_*` assignments then have nothing to
  point at and go with it.
- ~~**That last call is load-bearing and must be checked, not assumed.**~~
  **Checked 2026-07-31, and it is not.** The call in `Mini_CreateWindow` is to
  `glUpdateBufferSettings` — the port's own static setter — not to libEGL's
  `eglUpdateBufferSettings`. See the correction in § 2 above. The instinct that
  it "must be checked, not assumed" was the right one; what it was checked
  against, twice, was a symbol list rather than the call site.
- The result is a patched vendor library we maintain, which needs its patch
  recorded and its provenance pinned like every other dependency.

---

## Tasks

**Stage 1 — drop the unused dependency — LANDED 2026-07-29**

- [x] Add the removal to `wreel_add_handheld_bundle()` as a build step, not a
      manual one. `patchelf --remove-needed libGLESv2.so` over the staged copy,
      leaving `WREEL_ONION_SDL2_RUNTIME`'s source directory untouched
- [x] **Guard it with the check that justifies it**: fail the build if the
      library turns out to reference any symbol from `libGLESv2`, so this cannot
      silently become wrong when the runtime is updated
- [x] Make `patchelf` an optional tool — if absent, skip the step with a status
      message and ship the larger bundle, rather than failing the build
- [x] Record it in `docs/MIYOO-MINI.md` beside the rest of the runtime's
      anatomy, with the symbol evidence

**What stage 1 changed from the plan above.** Three things, and the first
changes a task's meaning rather than its outcome.

- **`patchelf` is not in the build container**, and the graceful skip this
  document asked for would therefore have skipped on the only build that
  produces a shippable binary. The host has it; `wreel-miyoomini` does not, and
  the Dockerfile deliberately never runs `apt` against buster's archive. So the
  image layers patchelf 0.19.1 the same way it already layers CMake and Ninja —
  official upstream binary, no compiling. Its release build is `static-pie` with
  no libc floor at all, so buster is a non-issue. The skip stayed, because it is
  still right for a host without the tool; it is just no longer the common case.
  `readelf` is treated as equally load-bearing: without it the check cannot run,
  and the removal is conditional on the check rather than on the tool that
  performs it.
- **`patchelf --remove-needed` exits 0 on a soname that is not there**, and the
  two runtimes in circulation spell it differently — steward-fu's declares
  `libGLESv2.so`, Onion's `parasyte` copy declares `libGLESv2.so.2`. A runtime
  swap would have silently no-op'd the patch while the CMake side had already
  dropped the file from the bundle. The script checks the entry exists before
  patching. Same shape as decision 4c in
  [onion-bundle](../2026-07-27-onion-bundle/): a fact checked against one
  artefact, applied to another.
- **It is a shell script, not a CMake one.** The check *is* the
  `readelf | awk | comm` pipeline in §5 below, and writing it in CMake meant a
  40-line regex parser standing in for three lines of awk, plus a step nobody
  could run by hand against a staged bundle. `sort` and `comm` both collate
  under the locale and a disagreement between them yields an *empty*
  intersection rather than an error — which here would read as "the dependency
  is unused" — so the script pins `LC_ALL=C`.

**Stage 2 — verify on the device — LANDED 2026-07-30**

- [x] `readelf -d` on the staged binary: no `libGLESv2` in `NEEDED`
- [x] The bundle's `lib/` holds three files, not four
- [x] It runs: bars, scroller and HUD as before. **This is the whole test** —
      if the loader wanted that library for a reason the symbol table did not
      show, the program will not start and the failure is immediate and obvious.
      It ran on **two** firmwares, which is one more than this document
      anticipated — see below
- [x] Record the before/after sizes in
      [target-validation/results.md](../2026-07-25-target-validation/results.md)

**What stage 2 settles.** The 21.8 MB was genuinely unreferenced. The failure
mode this document called immediate and obvious did not occur, on either
firmware, which is the whole of what the symbol evidence predicted.

Two things it turned up that belong to other snapshots rather than this one:

- **The bundle runs on stock Miyoo firmware, not just OnionOS.** Nothing here
  or in [onion-bundle](../2026-07-27-onion-bundle/) predicted that; every
  firmware-specific decision there was made for Onion. It is good news and it
  widens what `docs/MIYOO-MINI.md § 6` needs to say, since that section is
  written as though Onion were the only target.
- **Audio reads `silent` on stock.** Not caused by this work — the removed
  library has nothing to do with MI_AO — but found by the run that verified it.
  Diagnosed from the log the same day: the same single-owner MI_AO contention as
  Onion, on a firmware that ships no `stop_audioserver.sh` to release it.
  Recorded in [results.md](../2026-07-25-target-validation/results.md), with
  D26 for the fact that SDL had no error string to report it with.

The load-time evidence is the part that belongs to *this* snapshot, and it is
unambiguous — sixteen libraries resolved on the device and `libGLESv2.so` is not
among them:

```
libSDL2-2.0.so.0 => /mnt/SDCARD/App/Coppers/lib/libSDL2-2.0.so.0
libEGL.so        => /mnt/SDCARD/App/Coppers/lib/libEGL.so
libjson-c.so.5   => /mnt/SDCARD/App/Coppers/lib/libjson-c.so.5
```

`libEGL.so` loaded and was never asked for a context, which is what the §3
`dlopen` reasoning predicted. 863 frames at 59.7 fps, `exited 0`.

**Stage 3 — pin what we ship — LANDED 2026-07-30**

Carried over from [onion-bundle](../2026-07-27-onion-bundle/), and this is the
natural time:

- [x] Record the runtime's origin, version and licence in `docs/TARGETS.md`
      alongside every other pinned dependency. Right now it is "whatever was in
      that directory" — `steward-fu/sdl2`, `prebuilt/640x480/`, SDL 2.0.20,
      LGPL-2.1
- [x] Add a `THIRD-PARTY.md` for shipped binaries. `data/PROVENANCE.md` covers
      assets; a vendored LGPL library is not an asset and has different
      obligations — dynamic linking plus attribution plus the means to relink,
      which is exactly what stage 1's build step must not quietly break

**Nobody had recorded where the binaries came from**, so it was reconstructed
from the bytes. `git hash-object` produces the same blob SHA that GitHub's
contents API reports, which makes identity an exact test needing no download.
All four files matched:

| File | Traced to | Blob |
|---|---|---|
| `libSDL2-2.0.so.0` | steward-fu/sdl2 `prebuilt/640x480/` | `7dba96fb` |
| `libEGL.so` | same | `9b14b5b3` |
| `libGLESv2.so` | same | `1d47a720` |
| `libjson-c.so.5` | **OnionUI/Onion**, not steward-fu | `80db6c95` |

Three things that turned up, none of which this document assumed:

- **The `lib/` is assembled from two upstreams.** `steward-fu/sdl2` does ship a
  `libjson-c.so.5` in `examples/`, and it is a *different file* — 85,936 bytes
  against ours at 50,884. Ours matches a blob in Onion's Drastic and PICO-8
  packages.
- **`libEGL.so`'s licence does not follow from the repository's.** GitHub reports
  LGPL-2.1 for `steward-fu/sdl2`, which is right for SDL2 and cannot be read
  across to redistributed vendor binaries sitting beside it — `libGLESv2.so` in
  that same directory is SwiftShader, Apache-2.0 upstream. So `libEGL.so` ships
  with terms unknown, and that is now written down rather than assumed benign.
- **There are no tags or releases on that repository**, so the pin is a commit —
  `68ce3172`, the last to touch `prebuilt/640x480/` rather than the head, which
  moves for unrelated reasons.

It also found that `docs/TARGETS.md` had gone stale in four places, all of them
claims later work had already contradicted: it described the *parasyte* copy as
what runs here (decision 3 reversed that), attributed it to the wrong source
tree, said the driver registers `mmiyoo` (the shipped one registers `mini` —
the very confusion decision 4c cost a device run to find), said `software` is
the only render driver (the device reports `Miyoo Mini (accelerated)`), and told
the reader to let CMake find SDL2 in a sysroot that has only SDL 1.2.

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

**Measured 2026-07-29**, staging the GCC 8.3 `coppers` against steward-fu's
`prebuilt/640x480/` runtime in the toolchain container: staged bundle **30 MB →
8.8 MiB**, tarball **13 MB → 3.5 MB**, and `lib/` holds three files. All three
checks above pass.

Where the 8.8 MiB goes, since the estimate said 8.4: `lib/` 5.57, the binary
2.85, `data/` 0.31. The estimate was the first two — it did not count the
assets.

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

**The Flip.** Unchanged by this work. **Corrected 2026-07-31:** this said the
Flip "needs a runtime built for 752×560 regardless". It does not — the shipped
runtime detects the panel at startup and sets `FB_W`/`FB_H` from it. What it
needs is a runtime whose *texture cap* follows that detection, which is a
one-line defect rather than a separate build. Either way the resulting binary
wants the same symbol check as this document applied to ours, rather than an
assumption that it matches.

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
- ~~**Does `eglUpdateBufferSettings` matter to our path at all?**~~ **Answered
  2026-07-31: it does not** — and both consequences this question predicted
  followed. `SDL_gles_mini.c` was read; see the correction in § 2. Option C's
  stub is eleven functions returning failure, and option B's EGL removal is two
  `-l` flags. The remaining reason to prefer one over the other is no longer
  about EGL at all — it is in
  [miyoo-sdl2-fork](../2026-07-31-miyoo-sdl2-fork/), which now proposes building
  against the pinned SDL 2.32.10 rather than the 2.0.20 base this document's
  option B assumed.

## References

- [docs/MIYOO-MINI.md](../../docs/MIYOO-MINI.md) — the runtime's anatomy and
  where these libraries come from
- [onion-bundle](../2026-07-27-onion-bundle/) — the bundle this shrinks, and
  decision 3 for why the runtime is vendored at all
- [steward-fu/sdl2](https://github.com/steward-fu/sdl2) — source of the
  prebuilt, LGPL-2.1
