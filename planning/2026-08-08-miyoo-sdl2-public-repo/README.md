# Publishing the Miyoo Mini SDL2 drivers as their own repository

**Status:** `in progress` — scope decided 2026-08-10, stage 1 started
**Written:** 2026-08-08
**Blocked by:** nothing
**Serves:** [packaging-distribution](../2026-07-25-packaging-distribution/), the
LGPL obligation recorded in
[platform/miyoomini/sdl2/PROVENANCE.md](../../platform/miyoomini/sdl2/PROVENANCE.md),
and anyone else shipping SDL2 on this device
**Decision:** **taken 2026-08-10 — scope B, built so that C is a release-time
choice rather than a rebuild.** The repository is `sdl2-mini`, our own code in it
is zlib, and the conformance tooling is **new, minimal, portable C** that
supersedes `wreel-diag` rather than running beside it — §§ 3, 7

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
  Onion. Publishing implies a support matrix we have not earned, and this
  project's own rule is not to claim more than was verified. **A README that
  states exactly what was tested is the minimum honest version**, and it costs
  nothing.

  > **Corrected 2026-08-10: the Flip is owned, and always was.** This bullet said
  > the 752×560 panel was unowned; the fork snapshot's stage-1 task list called
  > item 4 "unverifiable without a Mini Flip in hand" and its § 8.7 called it
  > "the one target where the cap is wrong and cannot be tested". All three are
  > wrong, and they are the same error copied forward — no Flip *run* had
  > happened, and the document turned a gap in the evidence into a claim about
  > the hardware. The two are not the same thing and only one of them is
  > checkable from here, which is exactly why it went unchallenged for ten days.
  >
  > What it changes: item 4 (`max_texture_width/height` are literals the Flip
  > detection never reaches) moves from *unfixable-in-good-conscience* to
  > ordinary stage-1 work with a device run behind it, the tested-on matrix
  > becomes two panels rather than one, and the open question about "the artefact
  > for the Flip" below dissolves. Stock firmware still has far less exposure
  > than Onion, and that part of the bullet stands.
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

## 7. Decision, 2026-08-10

**Scope B, built so that C is a release-time choice rather than a rebuild.**

### The gate as written was larger than the thing it gated

Stage 0 below said nothing was worth starting until B or C was chosen, "because
the tooling effort differs by a lot". That reason does not survive § 2's own
table: **C is B plus prebuilt releases**, so the conformance tool, the evidence
and the examples are identical in both. The effort differs between *A and B*, not
between B and C. Stage 1 and stage 2 are a strict prefix of scope C's work, and
the B/C choice can be made when there is something to publish.

What genuinely had to be decided up front is narrower and cheaper: whether the
standalone build is reproducible by a stranger from day one. It is — the build
runs in the existing `union-miyoomini-toolchain` container, records the toolchain
it used, and emits a checksum beside the library. That costs a few lines now and
means choosing C later is a tag rather than a rebuild.

**The trigger for actually publishing a binary:** when the C tool's verdict table
has been produced on hardware *from the standalone build* rather than from this
tree, on both panels. That is the evidence § 5 says has not been earned yet, and
producing it is stage 2 regardless.

### Settled with it

| | |
|---|---|
| **Repository** | `sdl2-mini` — the drivers call themselves `mini` throughout (`SDL_MINI`, `Mini_VideoDriver`, the `"Miyoo Mini"` render backend), and the name does not read as a third clone next to `sdl2-mmiyoo` and `sdl2-steward-fu` |
| **Licence, our code** | **zlib**, matching upstream SDL2. The build glue, the C tool and the examples are new work; keeping them zlib leaves the LGPL boundary sitting exactly on steward-fu's driver files, which is the property `WREEL_SDL2_LINKAGE=SHARED` depends on, and lets someone lift an example into their app without thinking about it |
| **Licence, drivers** | **LGPL-2.1**, unchanged and non-negotiable — `PROVENANCE.md` travels with them |
| **The C tool** | **supersedes `wreel-diag`**, and is allowed to be a superset of it. § 3's "two instruments drifting" risk is closed by not having two |

### On the C tool superseding `wreel-diag`

The open question below asked whether it would, and noted it was cheaper to aim
at than to retrofit. It is aimed at.

The thing to be careful about is *sequencing*, not intent: the C tool is written
for the public repository's purpose and is not held back by this repository's
schedule for retiring `wreel-diag`. Concretely it takes no dependency on `util`,
`rig` or the readback layer — it is C against SDL2 and libc — and it emits the
verdict table in a form this repository can consume later. `wreel-diag` keeps
running here until the C tool's table has matched it on a device run; after that,
retiring it is a separate and much smaller decision.

---

## Tasks

**Stage 0 — decide the scope. DONE 2026-08-10**, § 7.

- [x] ~~Pick between scope B and scope C~~ **B, and the gate turned out to be
      A-vs-B rather than B-vs-C** — C is a superset of B by § 2's own table, so
      the choice was deferrable and the reason given for blocking on it was
      wrong. Settled with it: the repository name, zlib for our own code, and the
      C tool superseding `wreel-diag` rather than running beside it

**Stage 1 — the repository, scope A**

- [x] Driver sources, patch, `graft.cmake`, `PROVENANCE.md`, licence — done
      2026-08-10 at `~/Source/sdl2-mini`, uncommitted
- [x] A standalone build that fetches pinned SDL2 and produces
      `libSDL2-2.0.so.0` — CMake, not the autoconf route § 4 of the fork snapshot
      priced and rejected. **Built and inspected**: 1,480,704 bytes of armv7,
      839 exported symbols — the same count § 8's stage 0 measured, now
      reproduced from a build that shares no CMake code with this tree — and
      neither `libEGL.so` nor `libGLESv2.so` in `DT_NEEDED`. Not run on hardware
- [x] A README stating **exactly** what hardware and firmware it was tested on —
      including that nothing has been run from *that* build yet, which is the
      part the rule actually costs something to keep
- [x] Cross-reference from this repository's `PROVENANCE.md`, so the two do not
      drift into disagreeing about what was changed
- [ ] **Mechanise the `src/` sync.** The drivers are copied into the public
      repository, not shared, so the two can diverge silently — a fix in one, a
      bug report against the other, and nothing in either to say which is which.
      `PROVENANCE.md` carries a `diff -r` and a land-in-both rule, which is a
      convention rather than a mechanism and will hold exactly as long as
      somebody remembers it

**Stage 2 — the C tooling, scope B**

- [ ] Port [§ 8.4](../2026-07-31-miyoo-sdl2-fork/)'s traps **first**, as prose in
      the new repository, before any check is written against them
- [ ] A minimal C conformance tool: the verdict table, `/dev/fb0` readback, no
      dependencies beyond SDL2 and libc
- [ ] Examples: blended sprite, atlas sub-rect, software-renderer fallback,
      framebuffer readback
- [x] ~~Decide whether it supersedes `wreel-diag` here or runs alongside it~~
      **supersedes**, § 7 — with the sequencing caveat that `wreel-diag` keeps
      running here until the C tool's table has matched it on a device run

**Stage 3 — releases, scope C, only if chosen**

- [ ] Reproducible build with the toolchain recorded and checksums published —
      *the build side is stage 1 by § 7; what is left here is the publishing*
- [ ] A tested-on matrix that is honest about what has not been tested
- [ ] Item 4 (`max_texture_width/height`) fixed and **run on the Flip**, which
      § 5's correction makes ordinary work rather than a blind fix

---

## Risks

**~~Two instruments drifting~~** — **closed 2026-08-10** by § 7: the C tool is
the single instrument. What replaces it is a smaller, bounded risk — the window
between the C tool existing and `wreel-diag` being retired, during which the two
can still disagree. That window ends at one device run and is worth watching
rather than designing around.

**Publishing implies a support matrix we do not have.** § 5. Mitigated only by
saying so in the README, which requires the discipline to keep saying it when
someone asks whether it works on a device or firmware that has had no run —
which, with the Flip now in the matrix, means stock firmware and the plain Mini.

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
- ~~**Does the C tool replace `wreel-diag`?**~~ **Answered 2026-08-10: yes**, § 7.
- ~~**What is the artefact for the Flip?**~~ **Dissolved 2026-08-10.** It rested
  on the panel being unowned, which § 5's correction shows it never was. Item 4
  is a normal fix with a normal device run behind it, and the Flip is a row in
  the tested-on matrix rather than a hole in it.
- **Which firmwares get a run?** Onion on the Plus is the only one with real
  exposure. Stock is untested on both panels and the plain Mini is untested
  outright. This is now the honest edge of the matrix, and it is where the
  support-surface risk actually lives.

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
