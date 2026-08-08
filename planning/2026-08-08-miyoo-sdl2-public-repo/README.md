# Publishing the Miyoo Mini SDL2 drivers as their own repository

**Status:** `snapshot` — assessed, not started
**Written:** 2026-08-08
**Blocked by:** nothing, but see § 5 — the honest version needs a second device
or an explicit scope statement
**Serves:** [packaging-distribution](../2026-07-25-packaging-distribution/), the
LGPL obligation recorded in
[platform/miyoomini/sdl2/PROVENANCE.md](../../platform/miyoomini/sdl2/PROVENANCE.md),
and anyone else shipping SDL2 on this device
**Decision:** **not taken.** One thing is settled and it shapes everything else:
the conformance and example tooling would be **new, minimal, portable C**, not an
extraction of `wreel-diag` — § 3

---

## Motivation

Seven defects fixed, every one measured on hardware rather than argued, in a
driver that **two independent forks share**. `steward-fu/sdl2` and
`XK9274/sdl2_miyoo` stub the same five entry points and descend from the same
`QueueCopy` ([MIYOO-MINI.md § 3.1](../../docs/MIYOO-MINI.md)), so the defects
below are not ours alone to have found:

| | What it was |
|---|---|
| 1 | `SDL_UpdateTexture` kept the caller's pointer — a **use-after-free reachable through `SDL_CreateTextureFromSurface`**, which is what every `SDL_ttf` text path lands on. It took `wreel-diag` down with a SIGSEGV |
| 20 | Destination rects mirrored in x and not y, so **every sprite landed in the wrong place**. Invisible full-screen, which is why nobody had seen it |
| 2, 3 | The staging copy ignored the source rect's `y` and the format was inferred from the wrong width — **an atlas could not render** |
| 10, 11 | Blend modes and colour/alpha modulation accepted and silently dropped, on a driver that tells SDL it supports them |
| 21 | `Mini_UpdateWindowFramebuffer` was `return 0`, so **SDL's own software renderer could not present** |

**The audience already consumes exactly this artefact.** Onion and MainUI ports
vendor `libSDL2-2.0.so.0` inside the app directory — Sonic Mania ships its own
([MIYOO-MINI.md § 6.1](../../docs/MIYOO-MINI.md)) — and the bundle this project
builds *is* that one file.

**And the evidence may be worth more than the patches.** Nobody else has a
conformance harness for this device. A verdict table before and after, on
hardware, is what makes a stranger's `libSDL2` trustworthy; a patch list is not.

### How this snapshot nearly got framed wrongly

The first pass treated publishing as a variant of
[miyoo-sdl2-fork § 5](../2026-07-31-miyoo-sdl2-fork/)'s **option C** — "send the
patches to XK9274 and adopt their build" — which that document rejects, on the
grounds that it spends our time on someone else's timeline and costs a runtime
swap.

**They are not the same question.** Option C couples us to another project's
driver name, build system and release cadence. Publishing couples us to nothing:
we build what we already build, and others take it or do not. The rejection of C
says nothing about this, and carrying it across nearly buried the strongest
argument — that these are real fixes to code other people ship today.

---

## 1. What is actually separable

**Cleanly:** the 1,480 driver lines, `graft.cmake`, the registration patch, and
`PROVENANCE.md`. That set is already the shape
[§ 5.2](../2026-07-31-miyoo-sdl2-fork/) chose — *vendor the drivers, not SDL2* —
and the LGPL boundary sits exactly around it. Upstream SDL2 is zlib; it is
steward-fu's driver files that carry `// LGPL-2.1 License`.

**Not cleanly:** the build environment. `WREEL_MINI_SDL2` reaches into
`ProjectOptions.cmake` (it forces `WREEL_SDL2_LINKAGE=SHARED` for the licence
reason), `Dependencies.cmake` (the FetchContent declaration, the SDL option
matrix, `_wreel_sync_mini_drivers`), `toolchains/miyoomini.cmake`, and
`Packaging.cmake`. A public repo needs its own build, not a lift of ours.

**Deliberately not moved:** `wreel-diag`. See § 3.

---

## 2. Three concentric scopes

The decision is how far out to go, not whether to start.

| | Contents | Ongoing cost |
|---|---|---|
| **A** | drivers, patch, graft, provenance, a standalone build | near zero — it is what we maintain anyway |
| **B** | **+ conformance tool, evidence, examples** | the tooling has to be written and kept honest |
| **C** | **+ prebuilt `libSDL2-2.0.so.0` releases** | strangers' firmwares become our problem |

A alone discharges the licence obligation by URL and lets someone reproduce the
build. B is what makes it *credible*. C is what makes it *usable by people who
do not own a cross toolchain* — which, for this ecosystem, is most of them.

---

## 3. The tooling: new, minimal, portable C

**Settled 2026-08-08.** The conformance checks, diagnostics and examples in the
new repository are **written fresh in C**, informed by `wreel-diag` but not
extracted from it, and scoped to the narrow purpose of supporting a Miyoo Mini
SDL2 build.

This is the right call and it removes the objection that would otherwise have
dominated this document. `wreel-diag` is C++ and depends on `util::format`,
`util::log_*`, `rig::` and this project's readback layer; exporting that chain to
carry a diagnostic would have meant publishing a slice of the engine to prove a
driver works. A C program that links SDL2 and libc builds with a cross-gcc and
nothing else, which is also what the audience writes.

**Two costs come with it, and both are manageable if named now.**

- **Two instruments measuring one driver.** `wreel-diag` stays here as the
  engine's regression suite; the C tool lives there. They can drift, and a check
  that disagrees between them is a third class of bug on top of the two
  [§ 8.4](../2026-07-31-miyoo-sdl2-fork/) already documents. Worth deciding early
  whether the C tool eventually becomes the single instrument, with this repo
  consuming it — that is the tidy end state and it is cheaper to aim at than to
  retrofit.
- **The traps have to be carried over as prose, because they are not in the
  code.** § 8.4 records three checks that produced confident wrong answers before
  their own logic was right: a window sized from the driver's mode list is
  800×600 on a 640×480 panel; a no-op `RenderClear` cannot be measured across two
  presents when present pans between buffer halves; a sub-rect check passes by
  accident if the staging buffer already holds the expected colour. A fresh
  implementation that has not read that section will rediscover all three, and
  publish wrong verdicts in the meantime. **Porting § 8.4 is more important than
  porting any check.**

**Examples earn their place.** Minimal programs for a blended sprite, an atlas
sub-rect blit, the software-renderer fallback and framebuffer readback are
simultaneously documentation of what now works, regression coverage, and the
smallest possible reproduction for a bug report against us.

---

## 4. What publishing binaries actually obliges

Scope C only, and it is worth stating plainly because we have been on the
receiving end of getting it wrong.

`THIRD-PARTY.md` in this repository records a prebuilt `libSDL2-2.0.so.0` of
**unidentifiable version**, next to a `libjson-c.so.5` we could not identify
either. That entry is what publishing badly looks like from the other side.
Shipping binaries means: a reproducible build from the published source, the
toolchain recorded, checksums, and a statement of what was tested — or it means
becoming someone else's unknown blob.

---

## 5. The constraints that do not go away

- **One device, one firmware.** Everything verified here is a Miyoo Mini Plus on
  Onion. The Flip's 752×560 panel is unowned and item 4 is explicitly
  unverifiable without one; stock firmware has had far less exposure than Onion.
  Publishing implies a support matrix we have not earned, and this project's own
  rule is not to claim more than was verified. **A README that states exactly
  what was tested is the minimum honest version**, and it costs nothing.
- **The base differs from the ecosystem's.** Ours is SDL 2.32.10; both forks are
  2.0.20. The symbol set is a strict superset — 839 against 798, checked at stage
  0 — so it links and resolves. But twelve years of render-frontend change sit
  underneath, and a port written against 2.0.20 behaviour can break in a way that
  looks like our defect. This is the single most likely source of "your library
  broke my app".
- **A device loop with an audience.** Every change still needs an SD card. That
  is priced already; what is new is people waiting for it.

---

## 6. Options

| | Approach | Gets | Costs | |
|---|---|---|---|---|
| **1** | Status quo — drivers stay in this tree | nothing new | the fixes stay invisible to everyone else | |
| **2** | Scope A — source, patch, graft, provenance, standalone build | licence discharged by URL, reproducible by anyone with a toolchain | a second build to maintain | |
| **3** | Scope B — **+ C conformance tool, evidence, examples** | the fixes become *checkable* rather than asserted | the tooling, and § 3's two costs | **likely right** |
| **4** | Scope C — **+ prebuilt releases** | usable by people without a cross toolchain, which is most of the audience | § 4's obligations, and a support surface | the real decision |

**The question that decides it:** are we willing to own a support surface, or do
we publish source and evidence and let others build? 2 and 3 cost almost nothing
ongoing and deliver most of the reuse. 4 is where other people's firmware becomes
our problem — and also where the reuse actually lands for most of the audience.

---

## Tasks

**Stage 0 — decide the scope.** Nothing below is worth starting until 3 or 4 is
chosen, because the tooling effort differs by a lot.

- [ ] Pick between scope B and scope C, on § 6

**Stage 1 — the repository, scope A**

- [ ] Driver sources, patch, `graft.cmake`, `PROVENANCE.md`, licence
- [ ] A standalone build that fetches pinned SDL2 and produces
      `libSDL2-2.0.so.0` — CMake, not the autoconf route § 4 of the fork snapshot
      priced and rejected
- [ ] A README stating **exactly** what hardware and firmware it was tested on
- [ ] Cross-reference from this repository's `PROVENANCE.md`, so the two do not
      drift into disagreeing about what was changed

**Stage 2 — the C tooling, scope B**

- [ ] Port [§ 8.4](../2026-07-31-miyoo-sdl2-fork/)'s traps **first**, as prose in
      the new repository, before any check is written against them
- [ ] A minimal C conformance tool: the verdict table, `/dev/fb0` readback, no
      dependencies beyond SDL2 and libc
- [ ] Examples: blended sprite, atlas sub-rect, software-renderer fallback,
      framebuffer readback
- [ ] Decide whether it supersedes `wreel-diag` here or runs alongside it

**Stage 3 — releases, scope C, only if chosen**

- [ ] Reproducible build with the toolchain recorded and checksums published
- [ ] A tested-on matrix that is honest about what has not been tested

---

## Risks

**Two instruments drifting.** § 3. The mitigation is to decide early whether the
C tool becomes the single instrument.

**Publishing implies a support matrix we do not have.** § 5. Mitigated only by
saying so in the README, which requires the discipline to keep saying it when
someone asks whether it works on their Flip.

**The 2.32 base breaking a 2.0.20-era port.** § 5. Unmitigable in general;
worth a prominent note, and worth keeping the symbol-superset evidence published
so the failure mode can be diagnosed rather than guessed at.

**Scope creep into a second engine.** The new repository has a narrow purpose —
supporting a Miyoo Mini SDL2 build. Examples are the obvious vector: one more
demo, one more helper, and it has a `util`. The narrow purpose is the boundary,
and it belongs in that repository's README rather than only here.

---

## Open questions

- **Does XK9274's fork want these fixes?** They are in shared ancestry, so they
  apply in spirit if not as patches — ours are against 2.32, theirs is 2.0.20.
  [§ 5](../2026-07-31-miyoo-sdl2-fork/) rejects *offering* them uninvited and
  that still stands; a public repository makes them findable without anyone
  spending time on anyone else's timeline, which is the point.
- **Does the C tool replace `wreel-diag`?** Cheaper to aim at than to retrofit,
  and it decides how much of stage 2 is duplicated work.
- **What is the artefact for the Flip?** Item 4 fixes the texture cap for a panel
  nobody here owns. Publishing a build for hardware we cannot test is exactly
  what § 5 warns against — but leaving Flip users on a driver with a known wrong
  constant is not obviously better.

## References

- [miyoo-sdl2-fork](../2026-07-31-miyoo-sdl2-fork/) — the work being published;
  § 3 for what we own, § 5.2 for why the drivers are vendored and SDL2 is not,
  § 8 for the device record
- [packaging-distribution](../2026-07-25-packaging-distribution/) — the
  distribution question this serves
- [platform/miyoomini/sdl2/PROVENANCE.md](../../platform/miyoomini/sdl2/PROVENANCE.md)
  — the declaration of modification, which a public repository would carry
- [THIRD-PARTY.md](../../THIRD-PARTY.md) — what publishing badly looks like from
  the receiving end
