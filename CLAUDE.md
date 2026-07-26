# Working in this repository

SDL2 toy-game and live-graphics scaffold targeting Linux handhelds and Steam on
Linux. A revival of a 2016 C++ tree: the organisation and tooling discipline are
kept deliberately, the language level and rendering path are being brought
forward.

Start with [README.md](README.md), then [docs/TARGETS.md](docs/TARGETS.md) for the
constraints that drive every decision here.

---

## Who you are working with

A software engineer with ~20 years of experience. Calibrate accordingly:

- **Don't explain the basics.** No RAII tutorials, no "as you may know, a virtual
  destructor…". Assume fluency with the STL, template mechanics, move vs. copy
  semantics, ADL, ODR, and the usual footguns.
- **Do explain the non-obvious**: a tradeoff with a real cost, a constraint that
  isn't visible from the code, a reason one of two reasonable designs was picked.
  That's the part worth words.
- **Boost familiarity is assumed**, including older idioms. Note that this project
  deliberately does *not* depend on Boost — 128 MB of RAM and five cross-compile
  targets don't justify it — but boost-shaped idioms are fine and some already
  exist (`util::quoted_whitespace_tokenizer` is close to `boost::tokenizer`).
- **Skip the praise.** "Great question", "excellent catch" — cut it. Answer.
- Terse and precise beats padded and hedged. If something is uncertain, say what
  is uncertain and why, not "it may potentially be possible that".

---

## How to think here

### Correctness over speed

Correctness is preferred over speed of delivery, and firmly over "just get it
done". A wrong answer delivered quickly costs more than a right answer delivered
slowly, because the wrong one gets built upon.

Concretely:

- Prefer the fix that addresses the cause over the one that makes the symptom go
  away. If you're only able to do the latter, say so explicitly.
- Don't paper over a failure to make output look clean. A failing test that
  reveals a real defect is a better outcome than a passing one that hides it.
- If finishing properly needs another step — a doc read, a build, a question —
  take the step.

### Developer convenience is not a correctness criterion

This is a game and demoscene-adjacent codebase. Fullscreen, hiding the cursor,
grabbing the mouse, running at the panel's native mode and generally taking over
the display are the **intended presentation**, not misfeatures to be designed
around. The same goes for a tight uncapped render loop or writing directly to a
framebuffer.

So don't reclassify an intended behaviour as a defect because it makes debugging
less pleasant. Where a debugging affordance is genuinely wanted, add it as an
option and leave the default alone — that is what `Context`'s `fullscreen`
parameter is for. A defect is behaviour that contradicts what the code says it
does: `gfx::Context` computing `flags` and then discarding them was a real one;
defaulting to fullscreen never was.

### Trust but verify

**Do not assume the user is correct.** Experience does not make every statement
accurate, and a confidently-stated wrong premise is the most expensive kind. If a
claim is checkable, check it before building on it. If it turns out to be wrong,
say so plainly and move on — no hedging, no apology loop.

This has already earned its place in this repo. Facts that turned out to matter,
found only by checking:

- The Miyoo Mini toolchain is **GCC 8.3**, which sets the whole project's C++17
  ceiling. Found by reading the toolchain's `setup-env.sh`, not by assuming.
- `libgles2-mesa-dev` is a **transitional dummy** package on bookworm; the real
  one is `libgles-dev`. Guides still name the dummy.
- CMake 4.0 **rejects** `cmake_minimum_required(VERSION < 3.5)`, which SDL2_ttf's
  bundled FreeType still declares.
- `util::format` reused a consumed `va_list` — UB that had been in the tree for
  nine years, on an error path nobody exercised.
- The `util::line_iterator` "blank line truncates an OBJ file" landmine, recorded
  here and in `defects.md` as D4, **was not real**. Reading `_next()` closely and
  then parsing an `ico.obj` with a blank line injected between its vertex and face
  blocks both showed identical output. A defect inventory is a set of claims, not
  a set of facts — check them before acting on one.

### Fresh documentation reads beat recalled knowledge

Library APIs, package names, version numbers, CMake option spellings and target
names change. A fresh read of the actual source, header, or upstream docs is worth
more than a confident recollection.

- Read the header before calling the API. The `TTF_Font` forward declaration in
  `include/gfx/software/context.hpp` was wrong because it was written from memory
  of an older SDL_ttf; SDL_ttf 2.24 uses a different struct tag.
- Verify package names against `apt-cache policy`, not from memory.
- Verify upstream versions against the actual tags, not from memory.
- When pinning a dependency, record *why* that pin, in
  [docs/TARGETS.md](docs/TARGETS.md).

### Ask when it's genuinely ambiguous

If two readings of a request would lead to materially different work, ask. Don't
guess and don't build both.

But don't ask about things with an obvious default, or things you can check
yourself. "Which JSON library?" was worth asking. "Should I put the tests in
`tests/`?" is not.

### Don't claim more than you verified

State plainly what was run and what wasn't. "Builds and 3/3 tests pass on
`desktop-software`; the cross presets have only had their error paths checked" is
useful. "The build system works" is not, when six of seven presets have never
been run.

[docs/DEVELOPMENT.md § Status](docs/DEVELOPMENT.md#status) keeps this distinction
explicitly. Maintain it.

### Decisions are recorded, not re-litigated

Design decisions live in the repository with their reasoning:

- [docs/TARGETS.md](docs/TARGETS.md) — targets, constraints, dependency choices
  and the alternatives that were rejected, with reasons
- [planning/](planning/) — dated scope snapshots for work not yet started

Read these before proposing a direction. If you think a recorded decision is
wrong, say so and say why — but engage with the recorded reasoning rather than
starting from scratch. When a new decision gets made, write it down in the same
places.

---

## Hard constraints

Full detail in [docs/TARGETS.md](docs/TARGETS.md). The three that bite:

### 1. C++17 is the ceiling, and GCC 8.3 sets it

The Miyoo Mini toolchain bundles GCC 8.3. It is the oldest compiler in the matrix,
so it fixes the language floor for all shared code. Language support is good;
the **library** has holes:

| Unavailable | Needs | Use instead |
|---|---|---|
| `std::from_chars`/`to_chars` for float | GCC 11 | `strtod`/`strtof`/`snprintf` |
| `std::span` | C++20 | pointer + length |
| Ranges, real `concept` declarations, `<=>` | C++20 | see below |
| `std::filesystem` without `-lstdc++fs` | GCC 10 | already linked by `wreel::options` |

**On "concept interfaces":** there is no `concept` keyword available. Compile-time
interface constraints are expressed the C++17 way — traits classes, SFINAE /
`enable_if`, tag dispatch, and documented (not enforced) concepts. The codebase
already does this: `util::tokenizer_traits<T>::token_type` is a documented concept
requirement, and a type failing it produces a template error rather than a
constraint diagnostic. Prefer traits over `enable_if` where either works; it reads
better and the errors are less awful.

### 2. Never ship Debian's cross-GCC output

Debian 12's `aarch64-linux-gnu-g++` links against glibc 2.36. That binary will not
load on any handheld. Cross-GCC is a **compile-check** tool. Shippable binaries
come from the device SDK containers.

### 3. Miyoo Mini has no GPU

The SSD202D has no 3D block at all — no GL, no GLES, no EGL. The `software`
backend is the only option there, which makes it the baseline everywhere rather
than a fallback. 128 MB RAM total, shared with the OS.

---

## C++ expectations

Standard good hygiene, stated because it's cheaper than re-deriving it:

- **Ownership is explicit.** No raw owning pointers in new code. The legacy tree
  has `new`/`delete` pairs and one leaked singleton (`gfx::System`); don't add
  more. `unique_ptr` by default.
- **Rule of zero** first. Rule of three/five only when you're actually managing a
  resource, and then completely. The legacy `DISALLOW_COPY_AND_ASSIGN` macro
  predates deleted functions — new code uses `= delete`.
- **Move vs. copy is deliberate.** Pass sinks by value and move, take
  `const&` for read-only, return by value and let elision work. Don't `std::move`
  a return value.
- **`const`-correctness** on members, locals, and parameters.
- **No `using namespace` in headers.** `include/util/string.hpp` does this inside
  `namespace util` and it's a tracked defect (D5).
- **Include what you use.** Forward-declare in headers where it's enough — but
  match the upstream declaration *exactly* (see the `TTF_Font` lesson above).
- **Prefer standard algorithms** to hand-rolled loops where they read better. This
  codebase leans hard into iterator-and-functor style; that's its character, keep
  it consistent rather than mixing paradigms file by file.
- **Compile-time polymorphism over virtual dispatch** where the choice is fixed at
  build time. This is a house pattern: `util::File` selects its `FileImpl` by
  `#ifdef`, and `util::from_string` dispatches on a traits tag. No vtable, no
  runtime branch. Note that `gfx` is **not** an example any more — its renderers
  turned out not to be alternative implementations of one interface, so
  `gfx::renderer` and `gfx::gles2` coexist and each executable picks one. Applying
  the pattern where the things being selected are genuinely different jobs was the
  mistake there.
- **Errors are types.** `include/posix/errors.hpp` macro-generates an exception
  per errno value and `posix::wrap()` throws the matching one. Follow that rather
  than returning error codes.
- **No iostreams in shipped code.** `<iostream>`, `<fstream>`, `<sstream>` and
  `<iomanip>` cost 596 KB statically on armv7 — 1.6× the whole `<cstdio>`-only
  runtime floor. Removing them from the logger took 28% off the handheld binary.
  Use `util::log_*` (printf-style, `-Wformat=2`-checked) and `<cstdio>`. Neither
  `std::print` nor fmt is an escape hatch; see
  [docs/TARGETS.md § 1a](docs/TARGETS.md).
- **Wrap third-party types at the module boundary.** JSON access goes behind a
  `util::json` facade; the same applies to any future MIDI or audio dependency.
  Vendor types don't belong in module signatures.

### Style

[.clang-format](.clang-format) codifies what's already in the tree: 4-space
indent, 80 columns, brace-on-next-line for functions and types, attached for
control flow, `Type* name`. Run it on files you touch. Don't reformat files you
aren't otherwise changing.

### Modern spelling beats local precedent

Consistency with the surrounding code is worth something, but not as much as
moving toward the current standard idiom. Where the two conflict, take the modern
one — this is a modernization project, and matching a 2016 habit in new code just
extends its life.

Concretely: `#pragma once` rather than the `WREEL_<PATH>_HPP` guards the other
authored headers happen to use; `buffer.data()` rather than `&buffer[0]` now that
it is non-const; `= delete` rather than `DISALLOW_COPY_AND_ASSIGN`.

This does **not** license mixing paradigms within a file, or rewriting working
code you have no other reason to touch. It settles which way to go when you are
already writing the line.

### Comments describe the code as it is, not as it was

**Don't leave defect archaeology in the source.** A comment saying what a line
used to do wrong, which member was previously uninitialised, or that something
"was removed in C++17" is noise to the next reader — they're looking at the
current state and have to spend attention working out that the comment is
history, not a live constraint.

That reasoning does belong somewhere, just not here:

- **the defect inventory** —
  [planning/2026-07-25-cxx17-modernization/defects.md](planning/2026-07-25-cxx17-modernization/defects.md),
  which is where a fix gets recorded with its severity and its cause
- **the commit message**, which is the durable record of why a diff looks the
  way it does

Comments *should* still explain a non-obvious constraint that holds right now.
The distinction is tense, not subject matter:

```cpp
// mbsrtowcs(3) reports failure as (size_t)-1, so this must be tested before
// the terminator slot is added.                              // good: a live constraint

// The error check used to happen after the +1, so the throw was unreachable
// and an invalid sequence fell through to buffer(0).         // bad: history
```

Same rule for `// defect D3` style cross-references: the defect document points
at the code, not the other way round.

The old sources carry `vim: set sts=2 sw=2` modelines that contradict their own
indentation — they're wrong, drop them as you touch files.

**Naming follows C/STL/Boost, not Google.** No `kConstantName`, no `g_global`, no
Hungarian decoration. Constants and file-scope variables are plain `snake_case`
like everything else — `message_max`, `current_level`, `output_file`, matching the
tree's own `window_width` / `bytes_per_sample` and the standard library's own
lowercase `npos`. Types are `CamelCase`, members carry a leading underscore in the
2016 classes (`_window`) or a trailing one in `util` (`quote_marker_`); match the
file you are in. `SCREAMING_SNAKE` appears only for the class-scope constants in
`skratch/input.cc` and is not a pattern to extend.

---

## Build and test

```sh
# The software renderer, natively — same code path as the Miyoo Mini,
# no device or cross-compiler needed. This is the default working preset.
cmake --preset desktop-software
cmake --build --preset desktop-software
ctest --preset desktop-software

# Legacy GL backend (needs libglew-dev + libglu1-mesa-dev)
cmake --preset desktop-debug

cmake --list-presets        # all seven
```

First configure of any preset fetches and builds five pinned dependencies —
a couple of minutes. Later configures are ~2 seconds. `ccache` is used
automatically when present.

Host setup: `./scripts/bootstrap-debian.sh --dry-run` then without the flag.
Debian 12 or derivative assumed.

Do **not** hand-edit files under `build/`. Do **not** commit `build/` or `dist/`.

---

## Landmines in the legacy code

Inventory with severity in
[planning/2026-07-25-cxx17-modernization/defects.md](planning/2026-07-25-cxx17-modernization/defects.md).
The ones most likely to surprise you:

- Assets are opened by **relative path**, so anything that changes the working
  directory breaks the demo. `SDL_GetBasePath()` is the fix and it is not done yet;
  `skratch`'s *log* already moved to `SDL_GetPrefPath()`.
- `util/string.hpp` still has `using namespace std;` inside `namespace util` (D5),
  which is why the rest of that header qualifies everything explicitly.

Two long-standing ones are gone rather than fixed, both by deletion in the renderer
rework: `math::Vector3::operator+` mutating its left operand (D7 — glm replaced the
header), and `skratch` calling fixed-function GL directly (it renders through
`gfx::gles2` now, and `gfx::ObjModel` with its `GLuint` handles went with the 2016
backend).

Tests exist for the tokenizers specifically so this code can be changed safely:
`tests/test_string.cc`, 21 cases pinning current behaviour including the
surprising parts. Run them before and after any change to `string.hpp`.
