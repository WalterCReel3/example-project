# Third-party binaries

Shipped libraries this project did not build, and what is known about their
origin and licensing. Companion to [data/PROVENANCE.md](data/PROVENANCE.md),
which covers assets — a vendored library is not an asset and carries different
obligations, so it gets its own file.

Written 2026-07-30, reconstructing the provenance of a runtime that had been
recorded as "whatever was in that directory". Same rule as the asset file:
**everything gets a row, including the ones where the honest answer is
"unknown".**

## What ships, and only where

One target. Every other preset links everything statically — this is
`miyoomini`'s documented exception, because upstream SDL2 has no video driver
that can reach that panel ([docs/TARGETS.md](docs/TARGETS.md), the Miyoo Mini
exception).

The bundle's `lib/` after `cmake --build --target bundle-onion`:

| File | Origin | Pin | Licence |
|---|---|---|---|
| `libSDL2-2.0.so.0` | [steward-fu/sdl2](https://github.com/steward-fu/sdl2) `prebuilt/640x480/` | blob `7dba96fb`, path last changed in commit `68ce3172` (2025-12-17) | LGPL-2.1 |
| `libEGL.so` | same repo and path | blob `9b14b5b3`, same commit | **Unknown — see below** |
| `libjson-c.so.5` | [OnionUI/Onion](https://github.com/OnionUI/Onion) — **not** steward-fu | blob `80db6c95`, present since commit `78b3660d` (2023-12-17) | MIT upstream; **version unknown** |

Staged but **removed** by the build before it ships, so it is not in the table
above:

| File | Origin | Why it goes |
|---|---|---|
| `libGLESv2.so` | steward-fu/sdl2 `prebuilt/640x480/`, blob `1d47a720` | 21.8 MB of SwiftShader that `libSDL2` references zero symbols from. See [planning/2026-07-29-gles-free-runtime](planning/2026-07-29-gles-free-runtime/) |

### How these were identified

None of it was recorded when the files were fetched, so it was reconstructed on
2026-07-30 from the bytes. The method is repeatable and worth stating, because
it is what makes the pins above claims rather than guesses:

```console
$ git hash-object libSDL2-2.0.so.0        # the blob SHA GitHub reports for a file
7dba96fb8a011fb4951d605897e256d1c0057d7a
```

Comparing that against the GitHub contents API for a candidate path is an exact
byte-for-byte identity test, and it needs no download. All four files were
matched that way. The two SDL2 builds also carry their author's build path in
undropped debug info, which corroborates it independently:

| Build | `strings` shows | Registers |
|---|---|---|
| steward-fu prebuilt — **what we ship** | `/home/steward/Data/sdl2/sdl2` | video `mini` |
| Onion `parasyte` — not shipped | `/home/steward/Data/mmiyoo/sdl2` | video `mmiyoo` |

Both are steward's work from two different source trees, which is worth knowing:
"it is steward-fu's port" does not by itself identify which binary you have, and
[docs/MIYOO-MINI.md § 3](docs/MIYOO-MINI.md) records that they disagree on
driver names.

## The unknowns, stated plainly

**`libEGL.so`'s licence is not established.** GitHub reports LGPL-2.1 for
`steward-fu/sdl2` as a whole, and that is right for the SDL2 library. It does not
follow for the other binaries in that repository, which are redistributed vendor
and third-party objects rather than SDL2 — `libGLESv2.so` in the same directory
is demonstrably SwiftShader, which is Apache-2.0 upstream and not LGPL at all. So
the repository licence cannot be read across to `libEGL.so`, and its actual terms
are unknown. It is 55 KB, it is stripped, and it carries no licence string.

**`libjson-c.so.5` has no identifiable version.** Fully stripped: no build path,
no compiler string, no `JSON_C_VERSION`. Upstream json-c is MIT, which is the
licence recorded above, but *which* json-c this is cannot be told from the file.

**It also does not come from where the rest of the runtime comes from**, which
was the surprise. `steward-fu/sdl2` does carry a `libjson-c.so.5`, in
`examples/` — and it is a **different file**, 85,936 bytes against our 50,884.
Ours matches a blob that appears in OnionUI/Onion, in the Drastic and PICO-8
packages. So the bundle's `lib/` is assembled from two upstreams, not one.

**There are no tags or releases on `steward-fu/sdl2`**, so a commit SHA is the
only pin available. The commit recorded is the last one to touch
`prebuilt/640x480/`, not the repository head, which is the stable thing to cite:
the head moves for unrelated reasons and the prebuilt does not.

## Obligations

**LGPL-2.1 on `libSDL2-2.0.so.0`** is met the way the licence is designed for:
dynamic linking, attribution here, and the ability to relink against a modified
copy. That last one is a live constraint rather than a formality, and it is the
reason the GL removal is done the way it is —
`scripts/drop-unused-needed.sh` removes an unused `DT_NEEDED` and changes nothing
else, so a user substituting their own build of that library is unobstructed.

**The modification must be declared, and this is the declaration.** The
`libSDL2-2.0.so.0` this project ships is **not** byte-identical to the upstream
blob pinned above: one `DT_NEEDED` entry, `libGLESv2.so`, is removed at bundle
time. Nothing else is touched. The upstream file is recoverable from the pin, and
the transformation is one documented command.

**`libEGL.so` is the open item.** Shipping a binary of unknown licence is fine
for personal hardware and is not fine for distribution. That makes it the thing
to settle before any channel that counts as distribution, and it is recorded as
such in
[planning/2026-07-25-packaging-distribution](planning/2026-07-25-packaging-distribution/).

> **Corrected 2026-07-31.** This paragraph used to continue: "It is 55 KB and one
> symbol of it is load-bearing — `eglUpdateBufferSettings`, called from
> `Mini_CreateWindow` — so it cannot simply be dropped the way the GL library
> was."
>
> **No symbol of it is load-bearing.** `Mini_CreateWindow` calls
> `glUpdateBufferSettings`, a static function inside the port's own
> `SDL_gles_mini.c` that assigns the callback to a file-scope variable and
> returns. The libEGL import `eglUpdateBufferSettings` — one letter apart — is
> called only from `glCreateContext`, which is reached only through
> `SDL_GL_CreateContext`. Nothing on this target creates a GL context, and
> `gfx::gles2` is not compiled here.
>
> So `libEGL.so` is a `DT_NEEDED` that must be *satisfied* at load time and is
> never *called*. It cannot be removed the way `libGLESv2.so` was — the loader
> resolves `NEEDED` entries whether or not their symbols are used, and unlike
> that case the symbols genuinely are imported — but it can be **replaced**, by
> a stub exporting the eleven EGL entry points, or dropped outright by a source
> build that does not pass `-lEGL`. Read from source in
> [planning/2026-07-31-miyoo-sdl2-fork § 1.7](planning/2026-07-31-miyoo-sdl2-fork/),
> which has the evidence; the unknown-licence blocker is removable without a fork.

## Onion's `parasyte` runtime — identified, deliberately not shipped

Recorded because the two sets are easy to confuse, and a `WREEL_ONION_SDL2_RUNTIME`
pointed at the wrong one would build a bundle that looks right:

| File | Blob | From |
|---|---|---|
| `libSDL2-2.0.so.0` | `d1d94c82` | `static/build/.tmp_update/lib/parasyte/` in OnionUI/Onion, GPL-3.0 as a repository |
| `libEGL.so.1` | `ea67654c` | same |
| `libGLESv2.so.2` | `e0f2c339` | same |

**None of it is in any bundle.** Decision 3 of
[planning/2026-07-27-onion-bundle](planning/2026-07-27-onion-bundle/) explains
why: its EGL is Mesa and pulls in gbm, glapi, X11, xcb and libdrm plus a matched
loader and libc. Tell them apart by soname — parasyte uses `libEGL.so.1` and
`libGLESv2.so.2`, the vendored set uses `libEGL.so` and `libGLESv2.so` — or by
the blobs above.
