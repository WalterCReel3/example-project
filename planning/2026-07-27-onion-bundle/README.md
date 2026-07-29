# Delivering `coppers` to a Miyoo Mini Plus on OnionOS

**Status:** `in-progress`
**Written:** 2026-07-27
**Serves:** [packaging-distribution](../2026-07-25-packaging-distribution/),
[target-validation](../2026-07-25-target-validation/) steps 3 and 4,
[coppers-cracktro](../2026-07-26-coppers-cracktro/) stage 4
**Needs:** the union toolchain container, a Miyoo Mini Plus running OnionOS, an SD
card

## Motivation

[packaging-distribution](../2026-07-25-packaging-distribution/) says the next thing
to do is "build a bundle for one firmware — OnionOS on a Miyoo Mini Plus or Flip is
the obvious first". This is that bundle, scoped to the device actually in hand.

It is deliberately one firmware and one device. The generic
`wreel_add_handheld_bundle()` across four firmwares stays a stub until a second
device says what varies, because the layouts are guesses until a real SD card
disagrees with them.

---

## Verified while writing this

Six checks. Two of them change the plan, and one of them answers the question this
project has been carrying as its largest open unknown since it started.

### 1. The `miyoomini` binary in the tree today cannot load on the device

```console
$ objdump -T build/miyoomini/bin/coppers | grep -o 'GLIBC_[0-9.]*' | sort -Vu | tail -3
GLIBC_2.33
GLIBC_2.34
GLIBC_2.36
```

That is the Debian armhf cross-GCC compile-check build, exactly as
[cmake/toolchains/miyoomini.cmake](../../cmake/toolchains/miyoomini.cmake) says it
would be, and exactly what `docs/TARGETS.md § 2` warns never to ship. Nothing can be
delivered until the union toolchain container has produced a binary, which is
[target-validation](../2026-07-25-target-validation/) step 3 — never run.

So step 3 is not a parallel task to this work. It is step one *of* this work.

### 2. Upstream SDL2 cannot drive this panel — and that settles the open question

[target-validation](../2026-07-25-target-validation/) asks "is upstream SDL2 viable
on Miyoo Mini stock firmware, or is `WREEL_USE_SYSTEM_SDL2=ON` mandatory?" and calls
it the largest single unknown in the project. It does not need a device to answer.
It needed someone to read the generated config:

```console
$ grep -E "define SDL_VIDEO_DRIVER_" \
    build/miyoomini/_deps/sdl2-build/include-config-release/SDL2/SDL_config.h
#define SDL_VIDEO_DRIVER_DUMMY 1
#define SDL_VIDEO_DRIVER_OFFSCREEN 1
#define SDL_VIDEO_DRIVER_WAYLAND 1
```

Dummy, offscreen, and Wayland. There is no display path there, and there is no
audio one either — the same file has `DISK`, `DUMMY`, `JACK`, `OSS`, `PULSEAUDIO`
and `SNDIO`, and no ALSA.

This is not a misconfiguration to be fixed by turning something on.
[Dependencies.cmake](../../cmake/Dependencies.cmake) already sets `SDL_KMSDRM ON`,
and it silently produced nothing because libdrm and gbm are absent from the cross
environment — worth knowing on its own, since a flag that quietly fails detection is
a flag nobody can trust. But KMSDRM would not have helped: **SDL2 has no framebuffer
backend at all.** The 2.32.10 tree's `src/video/` holds `kmsdrm`, `x11`, `wayland`,
`vivante`, `raspberry`, `pandora`, `directfb` and the rest — SDL 1.2's `fbcon` has
no SDL2 successor. The SSD202D exposes SigmaStar's `MI_GFX` and a framebuffer, and
no DRM device.

**So the answer is no, and `WREEL_USE_SYSTEM_SDL2=ON` is mandatory on this target.**
Not a contingency to test on hardware — a fact about which drivers exist.

### 3. OnionOS already ships the patched SDL2, and it is the `mmiyoo` one

Onion installs a full alternate library set at
`/mnt/SDCARD/.tmp_update/lib/parasyte/`, and `libSDL2-2.0.so.0` is in it. Downloaded
from the Onion repository and read directly rather than taken on description:

```console
$ readelf -d libSDL2-2.0.so.0 | grep NEEDED
  ... libEGL.so.1  libGLESv2.so.2  libmi_ao.so  libshmvar.so
      libmi_common.so  libmi_sys.so  libmi_gfx.so  libc.so.6 ...

$ strings libSDL2-2.0.so.0 | grep -iE "^(mmiyoo|MMIYOO|software|opengles2)$" | sort -u
MMIYOO
mmiyoo
software
```

It links the vendor MI SDK — `libmi_gfx` for the display, `libmi_ao` for audio — and
registers an `mmiyoo` video driver. Its build path is still in the debug info
(`/home/steward/Data/mmiyoo/sdl2`), so this is [steward-fu's
port](https://github.com/steward-fu/sdl2), LGPL-2.1.

Its version, bracketed by symbol presence rather than by a version string it does not
carry:

| Symbol | Added in | Present |
|---|---|---|
| `SDL_RenderFlush` | 2.0.10 | yes |
| `SDL_SoftStretchLinear` | 2.0.16 | yes |
| `SDL_RenderGetWindow` | 2.0.22 | **no** |
| `SDL_GetWindowSizeInPixels` | 2.26.0 | **no** |

So 2.0.20 or 2.0.21, which agrees with `SDL_version.h` in steward-fu's tree (2.0.20).
Its highest glibc requirement is `GLIBC_2.27`.

**The only render driver it registers is `software`** — which is precisely
`gfx::renderer`'s baseline path on this target, and what `desktop-software` has been
exercising all along.

### 4. Our code fits inside that SDL2's ABI, with nothing missing

The obvious risk in building against a 2.32 tree and running against a 2.0.20 runtime
is a symbol added in between. Checked mechanically rather than reasoned about:

```sh
# Functions the device's SDL2 exports (798 of them)
readelf --dyn-syms -W libSDL2-2.0.so.0 | awk '$4=="FUNC" && $7!="UND" {print $8}' \
  | sed 's/@.*//' | sort -u > dev_syms.txt

# SDL functions our own code calls, and those the static satellite libs import
grep -rhoE '\bSDL_[A-Za-z0-9_]+' coppers gfx rig audio loaders probe util include \
  | sort -u | comm -12 - our_syms.txt | comm -23 - dev_syms.txt
for l in SDL2_image SDL2_mixer SDL2_ttf; do
    nm -u build/miyoomini/lib/lib$l.a | grep -oE 'U SDL_[A-Za-z0-9_]+' | awk '{print $2}'
done | sort -u | comm -23 - dev_syms.txt
```

87 SDL functions from our code, 107 imported by the statically-linked
SDL2_image/mixer/ttf. **Both difference sets are empty.** Nothing we call postdates
that runtime.

Two related things also check out, both by reading the code rather than hoping:

- The fork's own documentation warns that `TARGET` textures do not work and that a
  `STREAMING` texture is the supported path. `gfx` uses `SDL_TEXTUREACCESS_STATIC`
  in [texture.cc](../../gfx/renderer/texture.cc) and `SDL_TEXTUREACCESS_STREAMING`
  in [layer.cc](../../gfx/renderer/layer.cc), and nothing anywhere renders to a
  texture. The design decision `coppers` took for authenticity happens to be the one
  this runtime supports.
- Nothing in `gfx`, `rig` or `coppers` calls `SDL_RenderSetLogicalSize` or sets a
  single `SDL_HINT`, so there is no scaling behaviour to be surprised by.

### 4b. That SDL2 has no headless mode, so the cheap first check needs the static build

Added 2026-07-27, and it removes a step this document was relying on.

The plan had `SDL_VIDEODRIVER=dummy coppers --screenshot` over SSH as the first
thing to run on the device: no panel, no keyboard, no MI_GFX, just proof that the
binary loads and the code path executes. **That cannot work with the firmware's
SDL2.** Its build disables `dummy` and `offscreen` outright
(`--disable-video-dummy` in steward-fu's `Makefile.mk`), and `mmiyoo` is the only
driver it registers:

```console
$ strings libSDL2-2.0.so.0 | grep -xE 'mmiyoo|dummy|offscreen'
mmiyoo
```

So every run of the *bundled* binary goes through MI_GFX, and if MainUI has not
let go of the display there is no safer mode to fall back to.

What replaces it is the artefact stage 0 produced on the way past: the **statically
linked** GCC 8.3 build has `dummy` and nothing else, which makes it exactly the
headless smoke test — copy it next to the bundle, run it with `--screenshot`, and
a working BMP proves the toolchain, the loader, the glibc floor, asset resolution
and the whole render path on the device, with the display question still untouched.
Two binaries, each answering the half the other cannot.

### 4d. `audioserver` owns MI_AO, and Onion preloads an audio shim

From the second device run, which produced exactly two lines:

```
[MI ERR ]: MI_AO_SetPubAttr[3364]: Dev0 failed to set pub attr!!! error number:0xa0052009!!!
[MI ERR ]: MI_AO_DisableChn[3667]: Dev0 has not been enabled.
```

The risk section below predicted contention over audio from the presence of a
`KillAudioserver` flag in Onion's ports template. This is that contention, failing
in the vendor layer before SDL sees it: **MI_AO is single-owner and the owner is
`audioserver`.**

The firmware ships the remedy — `/mnt/SDCARD/.tmp_update/script/stop_audioserver.sh`
— and Onion's own ports launcher sources it when the flag is set. `launch.sh` now
does the same, defaulting on and overridable with `COPPERS_KILL_AUDIOSERVER=0`,
because `audio::Device` already tolerates a mixer that will not open and a silent
demo still measures fill rate.

**Reading that launcher turned up a second thing nobody would have guessed**: it
does `unset LD_PRELOAD` before starting the game, because Onion preloads
`libpadsp.so`, an audio shim, into launched programs. A shim that hooks audio
calls underneath an SDL2 talking to MI_AO directly is a conflict rather than a
help, so the bundle unsets it too. That would have been a genuinely difficult
failure to diagnose from symptoms.

### 4e. The launcher has to capture its own evidence

Two device runs, and neither produced a usable record — not because the program
was silent but because of where its output went.

- The first log interleaved two runs, because `tee -a` appends. The stale half
  was a `GLIBC_2.36` loader failure from a binary built by the *Debian* cross-GCC,
  and it read as current.
- The second produced only the MI_AO lines. Everything `coppers` itself logs goes
  to `SDL_GetPrefPath`, which is `.local/share/wreel/coppers/` — **a dotted
  directory**, invisible in most file managers and easy to miss over SFTP. The
  structured record of the run may well have been sitting there the whole time.

Iterating on a device is expensive — copy, boot, run, fetch — so a run that fails
to produce evidence costs a whole cycle. `launch.sh` now redirects the entire
script rather than piping the last command, so a failed guard, a loader error or a
crash before `main()` all land in the same file; truncates instead of appending;
records `uname`, the binary, `lib/`, `HOME` and `LD_PRELOAD`; runs the loader
once with `LD_TRACE_LOADED_OBJECTS=1` so an unresolved library says "not found"
rather than failing silently; and finally `cat`s `coppers.log` into the same file
so one download has everything.

This is not tidying. On this device the log *is* the instrument, the same way
`--seconds` and `--screenshot` are.

### 4c. The two builds disagree on the driver's name — found on the device

Added 2026-07-27, from the first output this project has ever produced on real
hardware:

```
Audio target 'mmiyoo' not available
```

`launch.sh` was exporting `SDL_VIDEODRIVER=mmiyoo` and `SDL_AUDIODRIVER=mmiyoo`,
on the reasoning in decision 4b that naming the driver makes a failure report
itself rather than silently selecting `dummy`. The reasoning was fine and the
result was wrong, because **the two mmiyoo SDL2 builds do not use the same name**:

| Build | Registers |
|---|---|
| Onion's `parasyte` copy | `mmiyoo` |
| steward-fu's `prebuilt/640x480` — the one the bundle vendors | `mini` |

Which is visible in the libraries themselves — 803 occurrences of `mmiyoo` in one
and zero in the other, where the other has `mini`. The name was read off the copy
that was inspected first and then used with the copy that was shipped.

**The fix is to set neither.** Each of these libraries compiles in exactly one
video and one audio driver, so leaving the choice to SDL selects the only
candidate — correct for both, and for a firmware that ships a third. The original
worry, that an unset variable lets SDL quietly fall through to `dummy`, does not
apply here: these builds have no `dummy` to fall through to (4b), and `coppers`
logs the driver it was given, so the answer is still recorded.

The general lesson is the one this repository keeps relearning: a fact checked
against one artefact was applied to a different artefact. It is the same shape as
D19 and D21 — the check was real, it just was not pointed at the thing that
shipped.

### 4a. The panel geometry is compiled *into* that SDL2 — added 2026-07-27

Found while working out how to build it. The `mmiyoo` video driver carries the
framebuffer size as a compile-time constant, and nothing reads an environment
variable to override it:

```c
/* sdl2/src/video/mini/SDL_video_mini.h */
#define DEF_FB_W 640
#define DEF_FB_H 480

/* SDL_video_mini.c, the only uses of either */
FB_W = DEF_FB_W;
FB_H = DEF_FB_H;
```

Which is why steward-fu's repository ships `prebuilt/320x240/` and
`prebuilt/640x480/` as separate builds of the same library.

Two consequences:

- **For a Mini Plus this is a non-issue** — 640×480 is the panel, and the prebuilt
  and Onion copies are both that build.
- **It is a trap for the Flip**, and a subtle one. That device is 750×560, no
  prebuilt matches it, and a library compiled for 640×480 will happily report
  640×480 to `SDL_GetDesktopDisplayMode`. So
  [coppers-cracktro](../2026-07-26-coppers-cracktro/)'s open question — "does the
  Flip report 750×560 to SDL, or interpose a scaler?" — **cannot be answered by
  running `wreel-probe` against a borrowed SDL2**. It would answer confidently and
  wrongly, and the wrong answer looks exactly like a firmware scaler. A Flip needs
  its own build with those two constants changed, and the probe result is worthless
  until it has one.

### 5. Only the *core* SDL2 has to be shared

`SDL2_image`, `SDL2_ttf` and `SDL2_mixer` are consumers of the SDL2 API, not of the
display. They keep being built from our pinned sources and linked statically, calling
into whichever `libSDL2-2.0.so.0` is loaded. That is what check 4's second half
confirms: the 107 SDL symbols they import all exist in the device's copy.

This matters more than it sounds. It means libxmp, `stb_image` and the vendored
FreeType stay hermetic and pinned — the tracker music, the PNG glyph sheet and
`Speedy.fon` all keep working exactly as measured — and the bundle acquires **one**
shared object rather than five.

### 6. The Onion layout, from real bundles rather than documentation

Onion's own documentation describes app config only in passing, so this comes from
packages in the Onion and Ports-Collection repositories.

**Apps** are `App/<Name>/` with two required files. Verbatim, from Battery Monitor:

```json
{
	"label":	"Battery Monitor",
	"icon":	"/mnt/SDCARD/Icons/Default/app/battery_monitor.png",
	"launch":	"launch.sh",
	"description":	"Monitor your battery usage"
}
```

```sh
#!/bin/sh
echo $0 $*
cd $(dirname "$0")
./batteryMonitorUI
```

**Ports** are three parallel trees — `Roms/PORTS/Games/<Name>/` for the payload,
`Roms/PORTS/Shortcuts/<Category>/<Name>.sh` for the launcher, and
`Roms/PORTS/Imgs/<Name>.png` for the artwork.

The instructive one is Sonic Mania, the native SDL2 port in that collection: it
ships `lib/libSDL2-2.0.so.0` alongside `libEGL.so` and `libjson-c.so.5` inside its
own directory. Every SDL2 title on this device carries its own SDL2. Meanwhile
Onion's *system* libraries in `static/build/miyoo/lib/` are SDL **1.2** throughout —
so "the firmware's SDL2" means the parasyte copy specifically, not a system library
in the ordinary sense.

Also verified: Onion's Tweaks app offers SSH/SFTP, Samba, FTP and an HTTP file
browser, so iterating does not mean pulling the SD card every time.

---

## Decisions

### 1. `App/Coppers/`, not a Port

`coppers` is an instrument that happens to be a demo. It wants to be one
self-contained directory that can be copied, SFTP'd into, and read back out of —
not a payload, a shortcut and a cover image in three separate trees.

The App layout also matches `rig::asset_path()` exactly as it already works: the
binary and its `data/` sit in one directory, and the "beside the executable" rule
resolves without help. Nothing about the Ports layout would improve on that, and its
shortcut scripts are built around `launch_retroarch.sh`, which has nothing to do
with us.

Revisit if a game ships from this tree — a game wants box art and a Ports entry.

### 2. Dynamic core SDL2, everything else static

Forced by finding 2, and it costs something worth stating plainly:
[packaging-distribution](../2026-07-25-packaging-distribution/) currently records
"No dynamic libraries to install — everything is static by design, which is one
problem packaging does *not* have here." **That is now false for `miyoomini`**, and
that snapshot needs correcting rather than quietly diverging.

It stays true everywhere else. The Mali targets and Steam keep the fully static
build; this is one target's documented exception, which is exactly what
`WREEL_USE_SYSTEM_SDL2` was put there for.

The LGPL-2.1 obligation on the SDL2 copy is met by dynamic linking plus attribution,
which is the arrangement it is designed for. It gets a row in
[`data/PROVENANCE.md`](../../data/PROVENANCE.md) — or rather in a `THIRD-PARTY.md`
beside it, since a shipped library is not an asset.

### 3. Vendor the runtime; do **not** borrow the firmware's copy

**Revised 2026-07-27 after reading the dependency closure. The first version of
this decision had it backwards**, and the reason is worth keeping because the
wrong answer was the intuitive one: prefer the firmware's own library, since it is
the copy guaranteed to match the vendor blobs beneath it.

That copy is `/mnt/SDCARD/.tmp_update/lib/parasyte/libSDL2-2.0.so.0`, and the
original plan was to copy **just that one file** into the bundle rather than put a
directory containing its own `ld-linux-armhf.so.3`, `libc.so.6` and
`libpython2.7.so` on `LD_LIBRARY_PATH`. One file turns out not to be an option:

```console
$ readelf -d parasyte/libSDL2-2.0.so.0 | grep NEEDED
  libEGL.so.1  libGLESv2.so.2  libmi_ao.so  libmi_gfx.so  ...

$ readelf -d parasyte/libEGL.so.1 | grep NEEDED
  libgbm.so.1  libglapi.so.0  libexpat.so.1  libX11-xcb.so.1  libxcb.so.1
  libxcb-dri2.so.0  libxcb-xfixes.so.0  libdrm.so.2  ...
```

Its EGL is **Mesa**, so satisfying that library means dragging in gbm, glapi, X11,
xcb and libdrm — on a device with no DRM node — and at that point you are using
the whole parasyte tree, including its matched loader and libc. Which is what
parasyte *is*: an alternate userland for binaries built against a newer glibc. Ours
is built against the device's own 2.28, so it wants none of it.

**steward-fu's prebuilt is self-contained, and that settles it.** Its stack needs
only base system libraries:

| File | Size | Needs beyond libc/libm/libdl/libpthread |
|---|---|---|
| `libSDL2-2.0.so.0` | 5.7 MB | `libEGL.so`, `libGLESv2.so`, `libjson-c.so.5`, and the firmware's `libmi_*` |
| `libEGL.so` | 55 KB | `libz`, `libstdc++`, `libgcc_s` |
| `libGLESv2.so` | 21 MB | `libz`, `libstdc++`, `libgcc_s` |
| `libjson-c.so.5` | 51 KB | — |

No Mesa, no X11, no libdrm, no second libc. The three base libraries it does want —
`libstdc++.so.6`, `libz.so.1`, `libgcc_s.so.1` — are all in the toolchain's sysroot,
which is built from this firmware, so they are almost certainly on the device. That
is evidence, not proof, and it is the kind of thing the first run settles in a
second.

**And this is how Onion's own SDL2 ports do it.** Sonic Mania — the native SDL2
port in the Ports-Collection — ships `libSDL2-2.0.so.0`, `libEGL.so` and
`libjson-c.so.5` in its own `lib/`, rather than pointing at parasyte. The pattern
was there to be read.

The firmware's copy stays as the fallback rather than the primary: if the vendored
library fails on a firmware revision it was not built against, the parasyte tree
is what the device has and `LD_LIBRARY_PATH` pointed at all of it is the thing to
try — accepting the libc risk, because at that point it is a diagnosis rather than
a design.

**The cost, stated because it is silly and should not be quietly accepted.**
`libGLESv2.so` is SwiftShader — a software GL implementation — and it is 21 MB of
the bundle's 30 MB. `coppers` uses no GL at all; it is a `DT_NEEDED` of that SDL2
build, so the loader maps it at startup regardless. Building an `mmiyoo` SDL2 with
its GL backend compiled out would remove it outright and is the obvious follow-up,
listed in the tasks. It is not on the critical path for a first run.

### 4. `HOME` points at the bundle, so the log lands where it can be read

`coppers` writes to `rig::pref_path()`, i.e. `SDL_GetPrefPath("wreel", "coppers")`,
which resolves under `$XDG_DATA_HOME` or `$HOME/.local/share`. On a handheld `HOME`
is whatever the firmware's init left it as, which may be a read-only path or unset —
and the log is the entire point of the first run.

`launch.sh` sets `HOME` to the bundle directory, so the log appears at
`App/Coppers/.local/share/wreel/coppers/coppers.log` on the SD card and comes back
over SFTP, or in a card reader if the device will not boot far enough for SSH.

### 5. `WREEL_DATA_DIR` is deliberately **not** set

It would work, and it would also mask exactly the failure the bundle needs to prove
it does not have. `rig::asset_path()` logs which rule won and warns when it falls
through to the working directory; setting the override makes that instrumentation
report nothing useful. Leave it unset, and let the log line say `beside the
executable`.

### 6. The bundle ships the four assets `coppers` opens, not all of `data/`

`glyphs-16x16.png`, `Speedy.fon`, and the three `.mod` files. Not `teapot.obj`, not
the test fixtures, not `FreebooterUpdated.ttf`.

Partly size, but mostly the licensing position in
[`data/PROVENANCE.md`](../../data/PROVENANCE.md): the fewer files of unknown
provenance that leave this repository, the smaller the problem being deferred. A
personal SD card is not a distribution, so this is not the licence question being
answered — it is the question not being made worse.

### 7. A staged directory plus `tar`, not CPack

This answers the open question packaging-distribution carries as "Decide whether
CPack is the right tool or a plain `install()` + `tar` is simpler". For this
firmware it is plainly the latter: what the device wants is a directory copied to a
path on a FAT32 card. CPack's value is metadata and generators, and Onion consumes
neither — its own distribution format is a `.7z` of exactly this directory tree.

CPack stays configured for source and desktop tarballs. It is not in the handheld
path.

---

## The bundle

```
App/Coppers/
    config.json                  label, icon, launch, description
    launch.sh                    the entrypoint MainUI runs
    coppers                      armv7, union toolchain, static libstdc++
    lib/
        libSDL2-2.0.so.0         the firmware's copy, or ours as the fallback
    data/
        glyphs-16x16.png
        Speedy.fon
        complications.mod
        complications ii.mod
        her bloody weekend.mod
    .local/share/wreel/coppers/  created at run time by SDL_GetPrefPath
        coppers.log
```

`launch.sh`, with each line earning its place:

```sh
#!/bin/sh
cd "$(dirname "$0")" || exit 1

# The firmware's SDL2 (see decision 3), and nothing else from parasyte.
export LD_LIBRARY_PATH="$PWD/lib:/customer/lib:$LD_LIBRARY_PATH"

# The mmiyoo backends are the only ones this SDL2 has; naming them means a
# failure says "driver not found" instead of silently selecting dummy and
# drawing nothing.
export SDL_VIDEODRIVER=mmiyoo
export SDL_AUDIODRIVER=mmiyoo

# So SDL_GetPrefPath lands inside the bundle — decision 4.
export HOME="$PWD"

./coppers "$@" 2>&1 | tee -a stdout.log
```

`config.json` follows Battery Monitor's shape exactly, including the tab-separated
formatting Onion's own tooling writes.

---

## Tasks

Ordered so that each step's failure is informative, and so the cheapest checks come
before the expensive ones.

**Stage 0 — a binary that can load — MOSTLY LANDED 2026-07-27**

- [x] Docker access
- [x] Build `union-miyoomini-toolchain`, then `docker/miyoomini.Dockerfile` on top of
      it. **GCC 8.3.0 confirmed**, and the layered image works as designed — buster's
      CMake 3.13 is too old for the presets and the layered 3.31.6 wins on `PATH`
- [x] **The first GCC 8.3 build of this codebase.** 53 errors, one cause
      ([D21](../2026-07-25-cxx17-modernization/defects.md)); after the fix, zero
      errors and zero warnings under `-Werror`, 15/15 suites under qemu, and a
      binary whose glibc floor is **2.28** rather than the Debian cross build's
      2.36. Full result in
      [target-validation/results.md](../2026-07-25-target-validation/results.md)
- [x] An SDL2 dev tree to link against — **upstream SDL 2.0.20 built in the
      container**, rather than steward-fu's tree. Same public ABI as the runtime
      (their fork *is* upstream 2.0.20 plus internal drivers), one command, no
      autotools, and it comes with a proper `SDL2Config.cmake`. The mmiyoo-specific
      bits are all internal to the library, so nothing we compile against differs
- [x] `WREEL_USE_SYSTEM_SDL2=ON` against it — which needed a toolchain fix before
      it could work at all, see below

**What stage 0 corrected, and it is decision 3's mechanics.** This document said
"`find_package(SDL2 REQUIRED)` needs a dev tree" and assumed the sysroot might
supply one. It does not:

```console
$ ls <sysroot>/usr/lib | grep -i sdl
libSDL-1.2.so.0   libSDL_image-1.2.so.0   libSDL_mixer-1.2.so.0   libSDL_ttf-2.0.so.0
```

**SDL 1.2.15 only.** The union toolchain predates any SDL2 on this platform, so
"system SDL2" can never mean "found in the sysroot" here — it means the device's
runtime copy plus headers we supply. What the sysroot *does* carry is the complete
SigmaStar MI SDK — `mi_gfx.h`, `mi_ao.h` and friends, with both `.so` and `.a` for
each — which is exactly what building an `mmiyoo` SDL2 needs, and removes the need
for steward-fu's vendored `mini/` copies.

Also learned, and it changes the order: **the fallback runtime already exists as a
binary.** steward-fu ships `prebuilt/640x480/libSDL2-2.0.so.0`, and its exported
function set is **identical** to Onion's parasyte copy — 798 functions, no
difference. So building that library from source is not on the critical path for a
Mini Plus at all; it is only needed for a panel neither prebuilt matches, which
means the Flip (decision 4a). Two of the three reasons for building it have
evaporated, and the remaining one is headers.

Building it from source is also **not** as cheap as assumed: the tree has
`configure.ac` but no committed `configure`, and neither the host nor the buster
container has autoconf — and this project deliberately does not run `apt` against
buster's archive. Generating `configure` elsewhere and building in the union
container works, but it is a second container in the loop rather than one command.

**Stage 1 — the bundle — LANDED 2026-07-27**

- [x] `wreel_add_handheld_bundle()` implemented for Onion behind
      `WREEL_TARGET_FIRMWARE=onion`: stages `App/Coppers/`, generates `config.json`
      and `launch.sh` from `cmake/templates/onion/`, copies the binary and the five
      assets `coppers` opens
- [x] A `bundle-onion` target that tars the staging tree from above `App/`, so it
      unpacks straight over the root of an SD card
- [x] Verified on the dev box: a bundle-shaped tree holding the `desktop-software`
      binary, launched from `/`, logs `assets: /tmp/sdcard/App/Coppers/data/
      (beside the executable)`, writes `coppers.log` inside the bundle, and renders
      a screenshot of the expected 1,228,922 bytes. `launch.sh` is `sh -n` and
      shellcheck clean. **Size only** — the bytes were not compared, and with the
      HUD on they could not have matched anything, because the HUD draws measured
      microseconds and makes the image differ between two runs of one binary. See
      the correction in
      [target-validation/results.md](../2026-07-25-target-validation/results.md)

**What stage 1 changed from the plan above.** Two things:

- **The asset list belongs on the target, not in packaging.** Which files ship is a
  property of the program — `coppers` opens five of the twenty in `data/` — so it is
  a `WREEL_BUNDLE_ASSETS` property set in
  [coppers/CMakeLists.txt](../../coppers/CMakeLists.txt), beside the code that opens
  them, and packaging reads it back. A missing file is a configure-time error naming
  the asset rather than a bundle that is quietly short one `.mod`.
- **`launch.sh` assigns the SDL drivers only if unset**, rather than exporting them
  unconditionally as this document first had it. Hard-coding `mmiyoo` would have
  blocked the one check that makes the first device run cheap:
  `SDL_VIDEODRIVER=dummy ./launch.sh --screenshot frame.bmp` over SSH answers "does
  the whole path execute on this hardware" without depending on the panel, on
  MI_GFX, or on MainUI having let go of the display. The default is unchanged.

**Stage 2 — the device**

- [ ] Copy to the SD card; enable SSH in Tweaks
- [ ] Confirm `ldd --version`, and that `libstdc++.so.6`, `libz.so.1` and
      `libgcc_s.so.1` are present — the vendored GL stack needs all three, and the
      evidence for them is the toolchain's sysroot rather than the device
- [ ] **The static build first**, since it is the only one that can run headless:
      `SDL_VIDEODRIVER=dummy ./coppers-static --screenshot frame.bmp --frames 3`.
      A valid BMP proves the toolchain, loader, glibc floor, asset resolution and
      render path without involving MI_GFX at all
- [ ] Then the bundle, which goes through `mmiyoo` whether you want it to or not.
      **This is where the SDL2 question gets its practical answer**
- [ ] `wreel-probe > probe.txt` — the display path, the video driver, the real mode
- [ ] Launch from the Apps menu and look at the panel
- [ ] The rest of the first-pass sequence in
      [target-validation/results.md](../2026-07-25-target-validation/results.md):
      fill rate at both internal resolutions, both scroller paths, the pad
      enumeration line

**Follow-ups, none blocking a first run**

- [ ] Build an `mmiyoo` SDL2 with the GL backend compiled out, removing 21 MB of
      SwiftShader the demo never calls. Needs `configure` generated outside the
      buster container, which has no autoconf
- [ ] A Flip build of that library with `DEF_FB_W`/`DEF_FB_H` at 750×560, without
      which no measurement from a Flip means anything (decision 4a)
- [ ] Pin the runtime the way every other dependency is pinned — `docs/TARGETS.md`
      records a version and a reason for each, and right now the bundle's SDL2 is
      "whatever was in that directory"

**Stage 3 — write down what happened**

- [ ] `results.md` gets real output, per command
- [ ] `docs/DEVELOPMENT.md § Status` rows move
- [ ] `docs/TARGETS.md` records the SDL2 exception with its reasoning and the pin
- [ ] Correct packaging-distribution's "no dynamic libraries" claim

---

## Risks

**MainUI and the audio server are still running.** Onion's own Ports template
carries `KillAudioserver` and `PerformanceMode` flags, which is evidence that
contention over audio is real on this firmware rather than theoretical. `coppers`
plays music through `libmi_ao`, and something else may already hold it. If the first
run is silent or refuses to open the mixer, that flag is the first thing to try —
`audio::Device` already logs and continues without sound rather than failing, so this
degrades rather than blocks.

**The panel may not be reached even with the right SDL2.** `mmiyoo` talks to
`libmi_gfx`, and whether MainUI has the display when our process wants it is
unverified. This is the one thing in this document that genuinely cannot be checked
without the device.

**Version skew if the wrong headers get used.** Building against 2.32 headers and
running against 2.0.20 happens to be safe for the symbols we call (finding 4), but
it is safe by measurement, not by construction — a future call to something newer
would link fine on the desktop and fail to load only on the device. Compiling
against the 2.0.20 headers makes it a compile error instead, which is why stage 0
builds them.

**The Flip is not covered by any of this.** Same preset, same SoC, 750×560, and
possibly a different firmware. Everything here is written for a Plus on Onion and
should be assumed wrong for the Flip until it is checked — including whether the
parasyte SDL2 is present at all.

**The assets remain unlicensed.** Unchanged by this work and not made worse by it.
Personal hardware is not distribution; a Steam depot is, and that is where
[packaging-distribution](../2026-07-25-packaging-distribution/)'s prerequisite list
still applies.

---

## Open questions

- **Does Onion's parasyte SDL2 exist on every install, or only some?** It is in the
  repository's `static/build/`, which suggests every install, but the device is the
  authority. If it is absent, the fallback becomes the default and decision 3
  inverts.
- **Should the bundle carry its SDL2 unconditionally?** Self-contained is more
  robust and the file is 5 MB; using the firmware's copy is more likely to match the
  blobs underneath it. Deliberately deferred to what the first run does, because
  either answer is currently a guess.
- **What is the device's actual glibc?** The parasyte SDL2 needs at most
  `GLIBC_2.27`, which is a lower bound rather than the answer. `ldd --version` over
  SSH settles it, and it is worth recording in `docs/TARGETS.md` next to the
  never-ship-Debian-cross-GCC rule, which currently states the hazard without
  stating the number.
- **Does `SDL_GetPrefPath` even succeed on this firmware?** It creates directories;
  a FAT32 SD card is fine, but the bundle may be mounted somewhere unexpected.
  `coppers` handles the empty return by not opening a log file, which would make the
  first run silent in the worst possible way — worth a fallback to `stdout.log`,
  which is why `launch.sh` tees.

## References

- [docs/MIYOO-MINI.md](../../docs/MIYOO-MINI.md) — **the platform reference this
  snapshot produced.** Everything learned about the device and its software
  stack, with sources and re-checkable commands. Read that for what the platform
  *is*; read this for how the decisions were reached.

- [packaging-distribution](../2026-07-25-packaging-distribution/) — the parent
  snapshot; this is its first firmware
- [target-validation](../2026-07-25-target-validation/) — steps 3 and 4, and the SDL2
  question finding 2 answers
- [coppers-cracktro](../2026-07-26-coppers-cracktro/) — stage 4 is what this trip
  takes
- [steward-fu/sdl2](https://github.com/steward-fu/sdl2) — the `mmiyoo` SDL2 port,
  LGPL-2.1, SDL 2.0.20
- [OnionUI/Onion](https://github.com/OnionUI/Onion) — `static/packages/App/` for the
  app layout, `static/build/.tmp_update/lib/parasyte/` for the runtime
- [OnionUI/Ports-Collection](https://github.com/OnionUI/Ports-Collection) — Sonic
  Mania is the native-SDL2 reference bundle
