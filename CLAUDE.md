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
  build time. This is a house pattern, used twice: `util::File` selects its
  `FileImpl` by `#ifdef`, and `gfx` selects its backend via
  `WREEL_GFX_BACKEND_*`. No vtable, no runtime branch.
- **Errors are types.** `include/posix/errors.hpp` macro-generates an exception
  per errno value and `posix::wrap()` throws the matching one. Follow that rather
  than returning error codes.
- **Wrap third-party types at the module boundary.** JSON access goes behind a
  `util::json` facade; the same applies to any future MIDI or audio dependency.
  Vendor types don't belong in module signatures.

### Style

[.clang-format](.clang-format) codifies what's already in the tree: 4-space
indent, 80 columns, brace-on-next-line for functions and types, attached for
control flow, `Type* name`. Run it on files you touch. Don't reformat files you
aren't otherwise changing.

The old sources carry `vim: set sts=2 sw=2` modelines that contradict their own
indentation — they're wrong, drop them as you touch files.

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

- `include/util/string.hpp` compiles **only** because libstdc++ still provides
  `ptr_fun`, `not1` and `unary_function`, which C++17 removed. This is why
  `WREEL_WERROR` defaults to `OFF`.
- `util::line_iterator` **stops at a blank line** — an OBJ file with a blank line
  in the middle silently loads partial geometry.
- `util::line_iterator`'s copy constructor initialises one member of five, and its
  post-increment returns a reference to a local. Pre-increment only.
- `math::Vector3::operator+` **mutates its left operand** and returns a reference.
- `skratch/application.cc` calls fixed-function GL directly, so the demo can't
  follow any new backend. `gfx::ObjModel` holds `GLuint` handles, which is why
  `loaders/obj.cc` can't build under the software backend.
- Assets are opened by **relative path**, so anything that changes the working
  directory breaks the demo.

Tests exist for the tokenizers specifically so this code can be changed safely:
`tests/test_string.cc`, 18 cases pinning current behaviour including the
surprising parts. Run them before and after any change to `string.hpp`.
