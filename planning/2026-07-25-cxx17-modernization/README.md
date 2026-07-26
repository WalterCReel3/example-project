# C++17 modernization of the 2016 sources

**Status:** `in-progress`
**Written:** 2026-07-25
**Blocks:** [graphics-backends](../2026-07-25-graphics-backends/)

## Motivation

The tree compiles today, but only because libstdc++ still ships constructs that
C++17 removed. `include/util/string.hpp` is built on `std::ptr_fun`,
`std::not1` and `std::unary_function`; GCC keeps them alive under
`_GLIBCXX_USE_DEPRECATED`, which defaults on. That is a borrowed reprieve, not a
working state:

- Any compiler or standard-library that honours the removal breaks the build.
- `WREEL_WERROR` cannot be turned on, because the deprecation warnings are
  errors. So the project currently has **no** warnings-as-errors gate, which is
  how the original `if(GCC)` typo went unnoticed for nine years.

The constructs are also hiding real defects — see [defects.md](defects.md).

## Measured warning inventory

Measured 2026-07-25 on Debian 12 / GCC 12.2 by touching every source and header
and rebuilding both presets. The full warning set in `cmake/ProjectOptions.cmake`
is already applied to project targets — only `-Werror` is missing — so these are
the real numbers, not an estimate.

`desktop-software` emits 67 warnings; `desktop-debug` emits 167 and is the
superset. Breakdown of the 167:

| Flag | Count | Where |
|---|---|---|
| `-Wdeprecated-declarations` | 96 | all `include/util/string.hpp` — the `ptr_fun` / `not1` / `unary_function` cluster |
| `-Wold-style-cast` | 26 | `loaders/image.cc` 6, `util/string.hpp` 6, `gfx/context.cc` 5, `skratch/application.cc` 4, `util/io.hpp` 4, `gfx/utils.cc` 1 |
| `-Wshadow` | 21 | all `gfx/spritesheet.cc` |
| `-Wdouble-promotion` | 17 | `skratch/application.cc` 14, `loaders/obj.cc` 3 |
| `-Wdeprecated-copy` | 4 | `include/gfx/utils.hpp` |
| `-Wunused-parameter` | 3 | `util/posix/fileimpl.cc`, `skratch/application.cc`, `gfx/obj.cc` |

**`include/util/string.hpp` is 102 of the 167** — 61% of the whole gate lives in
this one header, and clearing it is most of the work toward `-Werror`.

### Progress

Re-measured the same way after each block landed:

| Preset | Start | After `string.hpp` / `ascii.hpp` | After the surviving-code sweep |
|---|---|---|---|
| `desktop-software` | 67 | 31 | **0** |
| `desktop-debug` | 167 | 65 | **33** |

All 96 `-Wdeprecated-declarations` went with the first block, and nothing
deprecation-related remains anywhere. **`desktop-software` — the default working
preset and the Miyoo Mini code path — is now completely warning-free.**

Note the difference between warning *instances* and *sites*: the 6
`-Wold-style-cast` attributed to `string.hpp` were 2 unique lines reported once
per including translation unit, and the 32 cleared in the second sweep were 14
unique sites. Counts in the table are instances.

### Why the remaining 33 are deliberately not being fixed

Classified by what [graphics-backends](../2026-07-25-graphics-backends/) does to
each file. Nothing left is in code that survives:

| Fate | Count | Files |
|---|---|---|
| **Deleted** with the `gl_legacy` retirement | 11 | `gfx/context.cc` 5, `gfx/utils.hpp` 4, `gfx/utils.cc` 1, `gfx/obj.cc` 1 |
| **Rewritten** substantially | 22 | `skratch/application.cc` 19 (ported off direct GL), `loaders/obj.cc` 3 (rewritten for `gfx::Mesh`) |

Verified rather than assumed: `gfx/utils.hpp` is included only by `gfx/utils.cc`,
`gfx/context.cc` and `skratch/application.hpp` — all three in that table.
`gfx/spritesheet.cc` contains no GL references at all, which is why it counted as
surviving code and was cleaned.

Fixing casts in a translation unit that is about to be deleted is work thrown
away, so these wait for that snapshot rather than being polished now.

Verified alongside: `test_string` passes 21 cases / 75 assertions — the original
18 cases unchanged in behaviour, plus the three added for the `line_iterator`
copy, assignment and post-increment fixes — and the OBJ loader produces
byte-identical geometry —
`data/ico.obj` 42 vertices / 240 indices and `data/teapot.obj` 3644 / 18960,
matching the pre-change values recorded in
[docs/DEVELOPMENT.md](../../docs/DEVELOPMENT.md#running-the-skratch-demo).

Two corrections to the original estimates in this document:

- **`-Wold-style-cast` is not the expensive one.** 26 warnings, and *not* in the
  files predicted below: `gfx/obj.cc` and `gfx/utils.cc` — named as the costly hot
  paths — contribute **1 between them**. The concentration is in `loaders/image.cc`
  and `gfx/context.cc`. It earns its place; keep it tree-wide.
- **Two files not previously in scope carry real clusters.**
  `gfx/spritesheet.cc` (21 `-Wshadow`) and `skratch/application.cc`
  (14 `-Wdouble-promotion` + 4 old-style-cast + 1 unused-parameter) are the
  second and third largest after `string.hpp`.

### Reachability of `string.hpp`

Its only consumers are `loaders/obj.cc` and `tests/test_string.cc`. What
`obj.cc` actually instantiates is `quoted_whitespace_tokenizer`,
`token_generator`, `line_iterator`, and through them `find_escaped`,
`token_break`, `is_space` and `tokenizer_traits`.

Everything else has **zero consumers**: `escaped_find`, `whitespace_tokenizer`
(test-only), `parse`, `copy_until`, `copy_until_if`, `split_value`,
`split_sequence`, `contains`, `sequence_contains`, both `strip` overloads,
`to_string`, `to_wstring`, `wctomb_insert_iterator`, `wstring_to_string`,
`reset`, `long_of_string`, `token_traits`, `base_parser`, `mbtowc_iterator`.

Two further findings from that read:

- There are **two** `std::unary_function` bases, not one — `token_generator`
  (line 191) and `wstring_to_string` (line 489).
- Of the 13 ctype predicates, only `is_space` is reachable. It is used twice:
  `not1(is_space)` in both tokenizers, and `util::is_space(value)` inside
  `token_break`.

## Scope

`include/util/string.hpp` (688 lines, ~20% of the codebase) and the handful of
files that consume it. Specifically:

1. Move the 13 `ptr_fun`-wrapped ctype predicates to a new
   `include/util/ascii.hpp` that is not deprecated and does not pass `char` to
   `::isspace` (currently UB for negative values — an OBJ file with a stray high
   byte is undefined behaviour today, and behaves differently again on ARM, where
   `char` is unsigned). See *Decisions*.
2. Replace `not1(pred)` with `std::not_fn(pred)` — both tokenizers and both
   `strip()` overloads.
3. Drop the `std::unary_function` base from `token_generator` **and**
   `wstring_to_string`. It contributes only `argument_type`/`result_type`
   typedefs that nothing reads.
4. Fix the defects in [defects.md](defects.md) — several are UB, and two are in
   code paths the OBJ loader uses.
5. Turn on `WREEL_WERROR` and clear the resulting output across the tree. The
   warning set in `cmake/ProjectOptions.cmake` is wider than the five flags this
   document originally named; see *Measured warning inventory* for what is
   actually there.

## Non-goals

- **Do not** redesign the tokenizer API. It works, `loaders/obj.cc` depends on
  its exact behaviour, and the test suite now pins that behaviour down. This is
  a language-level migration, not a rewrite.
- **Do not** reach for C++20. `std::ranges` and real `concept` declarations would
  express this far better and are unavailable — the Miyoo Mini toolchain is
  GCC 8.3. See [docs/TARGETS.md § 1](../../docs/TARGETS.md). The one place a
  post-C++17 facility is *tracked* rather than used is `util/ascii.hpp`, which
  borrows P3688's names so the eventual `<ascii>` adoption is a shim rather than a
  rename — see *Decisions*.
- **Do not** introduce Boost. The idioms here are boost-flavoured
  (`quoted_whitespace_tokenizer` is close to `boost::tokenizer`), but a Boost
  dependency on a 128 MB device with five cross-compile targets is not worth it.

## Decisions

Both settled 2026-07-25, before implementation.

### Character classification moves to `include/util/ascii.hpp`, tracking P3688

The 13 `ptr_fun`-wrapped ctype predicates are replaced by a new
`include/util/ascii.hpp` whose names and semantics follow
[P3688R6 "ASCII character utilities"](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2026/p3688r6.html)
(Schultke and Jabot, revised 2026-02-21), the C++26 paper that standardizes a
`<ascii>` header for exactly this problem. Implemented as `inline constexpr`
callable objects rather than P3688's free function templates:

```cpp
namespace util
{
struct ascii_is_whitespace_fn {
    template<typename CharT>
    constexpr bool operator()(CharT c) const noexcept
    {
        const std::make_unsigned_t<CharT> u = c;
        return u == ' ' || (u >= '\t' && u <= '\r');
    }
};
inline constexpr ascii_is_whitespace_fn ascii_is_whitespace{};
}
```

**Why not `<cctype>`.** The framing that settled this is that it was never a
modern-versus-venerable tradeoff. What is worth keeping from POSIX is the
*interface* — one-argument predicates, usable as function objects with standard
algorithms. What does not survive scrutiny is the *dispatch*, because `::isspace`
is locale-dependent by contract, and for a parser of ASCII-defined formats
(OBJ, JSON) locale-dependence is a defect wearing portability's clothes.

Three reasons, the first measured:

1. **It diverges across this project's own targets.** `char` is signed on
   x86-64 and unsigned on both ARM targets:

   ```
   g++ (x86-64)              char is SIGNED
   aarch64-linux-gnu-g++     char is UNSIGNED
   arm-linux-gnueabihf-g++   char is UNSIGNED
   ```

   For an asset byte ≥ 0x80, the dev box passes a negative `int` to `::isspace`
   — UB, since C99 7.4p1 admits only values representable as `unsigned char` or
   `EOF` — while aarch64 and armhf pass 128–255, which is defined but is a
   locale-table lookup. Same byte, different path on the dev box versus every
   shipping target. That is backwards from the portability the POSIX call is
   being kept for.

2. **There is a latent locale coupling that will fire later.** Nothing in the
   tree calls `setlocale`, so everything runs in the `"C"` locale and the ctype
   calls are ASCII in practice today. But `util::to_wstring` / `util::to_string`
   are built on `mbsrtowcs` / `wcrtomb`, which do nothing useful until someone
   calls `setlocale(LC_CTYPE, "")`. The day the wide-char path is made to work,
   the OBJ tokenizer's definition of whitespace changes underneath it — glibc's
   ISO-8859-1 tables make `isspace(0xA0)`, a non-breaking space, true. An
   unrelated fix would silently rewrite the parser.

3. It costs a table indirection per character in the tokenizer's inner loop on
   two Cortex-A7 cores, buying nothing.

The stable, portable, pre-locale semantics actually wanted here *are* the
C-locale semantics, which are a fixed ASCII table expressible in one line each.

**Why not `<locale>`.** `std::use_facet<std::ctype<char>>(std::locale::classic())`
is the standard C++ answer and does fix the UB — it takes `charT`, not `int`. It
was rejected because it is not `constexpr`, it requires threading a locale object
or a hoisted facet reference through both tokenizers and `strip()`, and it leaves
the parser locale-*parameterized*: the semantics remain a function of what a
caller passes in.

One thing that is **not** a reason: binary size. From scratch the `<locale>` path
costs 869 KB statically linked, which would matter on a 128 MB device — but the
measured marginal cost in this tree is **0 bytes**, because
`include/util/logging.hpp` includes `<fstream>` and `string.hpp` includes
`logging.hpp`, so every consumer already instantiates the locale facets.
`test_string` already contains them. Recorded because the size argument looks
compelling and is void here.

**Why not `SDL_isspace`.** SDL 2.32.10 exposes 12 of the 13 predicates and is
already a dependency on every target, which makes it the obvious candidate for
not writing our own. It does not work: `SDL_stdlib.c:483` gates two
implementations on `HAVE_CTYPE_H`, and the generated `SDL_config.h` defines it as
`1` on both `desktop-software` and the armv7 `miyoomini` build. So `SDL_isspace`
*is* `::isspace` on every target here — same locale dependence, same `int`
parameter, same negative-`char` UB — with an added call across a static library
boundary. It would also give `util` an SDL dependency, which it currently does
not have.

**What 2020s C++ actually does.** Hand-rolled ASCII predicates are the idiom, not
a workaround, which is what makes this a forward-port rather than a local
invention:

| Library | Spelling | Note |
|---|---|---|
| P3688 / C++26 | `std::ascii_is_whitespace(T)` → `bool` | `constexpr`, `noexcept`, templated over character type |
| Abseil | `absl::ascii_isspace(unsigned char)` → `bool` | 256-entry bitfield table; docs call locale reliance "problematic" |
| LLVM | `llvm::isSpace(char)` → `bool` | "Locale-independent version of the C standard library isspace" |
| nlohmann/json 3.12 | inline literal comparisons | **already a `util` dependency**; `lexer.hpp:1515`, documented locale-independent at `:966` |
| POCO | `Poco::Ascii` | same pattern |

P3688's own rationale against `<cctype>`/`<locale>` is item-for-item the defect
list for this header: signed-`char` UB, `int` return instead of `bool`, `EOF`
handling violating zero-overhead, no `constexpr`, and locale dependency.

Mechanical consequences, which are why this shape and not another:

- `make_unsigned_t<CharT>` is the entire UB fix, and it is a no-op on ARM.
- **Callable objects, not P3688's free function templates.** A function template
  cannot be passed as a predicate, and this code does that twice —
  `find_if(first + 1, last, is_space)` in both tokenizers — plus `strip()` takes a
  `Predicate` parameter. `inline constexpr` objects (C++17, GCC 7+) keep both call
  syntaxes working and hold call-site churn to the rename. This is the niebloid
  shape `std::ranges` uses for the same reason. The cost is that adopting `<ascii>`
  later becomes a thin shim rather than a `using` declaration.
- It replaces 13 mutable namespace-scope `static`s in a header (one copy per
  translation unit) with one `constexpr` entity each.
- Unused `constexpr` variables do not warn, so **the full P3688 set is provided**
  even though only whitespace is reachable today. It covers all 13 existing
  predicates — `is_space`→`ascii_is_whitespace`,
  `is_blank`→`ascii_is_horizontal_whitespace`, `is_ascii`→`ascii_is_any`,
  `is_xdigit`→`ascii_is_hex_digit`, `is_alpha`→`ascii_is_alphabetic`,
  `is_alnum`→`ascii_is_alphanumeric`, the rest direct — and adds
  `ascii_is_bit`, `ascii_is_octal_digit`, `ascii_to_lower` and `ascii_to_upper`.
- `not1(pred)` becomes `std::not_fn(pred)` — the direct C++17 replacement, in
  libstdc++ since GCC 7. **Unverified on the actual Miyoo toolchain** until that
  container runs; see [target-validation](../2026-07-25-target-validation/) step 3.
- A separate header, rather than more of `string.hpp`, because it mirrors the
  `<ascii>` boundary and because `string.hpp` reaches `<fstream>` through
  `logging.hpp`. The upcoming JSON and MIDI-mapping parsers get character
  classification without the tokenizers or iostreams.

**Behaviour change: none, today.** The C locale is the ASCII table, so all 18
`test_string` cases should pass bit-identically. That property is what makes this
safe to do first.

### The unreachable two-thirds of `string.hpp` is modernized, not deleted

Despite having no consumers, the dead code listed under *Reachability* above
stays. It is treated as a util library with future consumers rather than as
scaffolding for `obj.cc`: the `unary_function` bases come off, the warnings get
cleared, and `not1` usages inside it get replaced, but nothing is removed.

Consequences to be aware of:

- **D3 changes resolution.** `escaped_find` gets `escaped_character_(false)`
  rather than being deleted as [defects.md](defects.md) originally proposed.
- `strip()` takes a `Predicate` and calls `not1` on it, so both overloads need
  `std::not_fn` even though nothing calls them.
- `mbtowc_iterator` is kept despite being non-functional — see D11.
- `whitespace_tokenizer` is kept and modernized. It still defines no
  `token_type`, so it still cannot be passed to `util::tokenize`; that asymmetry
  with `quoted_whitespace_tokenizer` is now a deliberately retained wart rather
  than an open question.

## Prerequisite, already met

Tests come first, and they existed before any of this landed: `tests/test_string.cc`
opened with 18 cases and 48 assertions pinning current tokenizer behaviour,
including the surprising parts (escapes keep their backslash; a blank line is
yielded as a one-character `"\n"` stride rather than terminating iteration — the
claim that it terminated was D4, and it was wrong). Now 21 cases and 75
assertions. Run it before and after — behaviour should be identical except where
[defects.md](defects.md) says otherwise.

```sh
ctest --preset desktop-software -R test_string
```

## Tasks

Ordered. The `string.hpp` / `ascii.hpp` block is 61% of the warning gate;
everything after it is independent.

- [x] Measure the real warning inventory instead of estimating it
- [x] Decide the character-classification approach (see *Decisions*)
- [x] Decide the fate of the unreachable code (see *Decisions*)
- [x] New `include/util/ascii.hpp` — P3688-named `inline constexpr` predicates
- [x] `tests/test_ascii.cc` — 12 cases, 1017 assertions. Covers each predicate's
      range edges, the partition properties, and specifically that every byte in
      0x80–0xFF classifies false rather than invoking UB, asserted through both a
      signed-`char` and an unsigned-`char` spelling so the ARM behaviour is
      pinned on the host
- [x] Repoint `token_break` and both tokenizers at `util::ascii_is_whitespace`
- [x] Replace `not1` with `std::not_fn` — both tokenizers, both `strip` overloads
- [x] Remove the `std::unary_function` base from `token_generator` **and**
      `wstring_to_string`
- [x] Fix `line_iterator`'s copy constructor (D1), post-increment (D2) and the
      `_stop_sequence` omission in its `operator=` — both hand-rolled functions
      deleted rather than repaired, since the implicitly generated ones are
      correct and complete. Three new test cases pin the copy, the assignment and
      the post-increment
- [x] Initialise `escaped_find::escaped_character_` (D3)
- [x] Fix `to_wstring`'s unreachable error check and its UB error path (D12) —
      found while clearing the old-style casts from the same two lines
- [x] **Withdraw D4.** Verified not to be a defect; corrected in `defects.md`,
      the risk note below, and the repository `CLAUDE.md`. `test_string.cc` now
      asserts the real behaviour exactly instead of carrying the wrong claim in a
      comment
- [x] Fix `posix::FileImpl::seek` discarding its offset (D14), and cover it with
      a non-zero-offset test. Found by `-Wunused-parameter`
- [x] Clear the 32 warnings in code that survives `graphics-backends`:
      `gfx/spritesheet.cc` (made `SpritesheetFrame` an aggregate and dropped its
      four hand-rolled special members — Rule of zero), `loaders/image.cc`,
      `util/io.hpp`, `util/posix/fileimpl.cc`. `desktop-software` is now at zero
- [ ] Remove `using namespace std;` from inside `namespace util` (D5) — this is
      what forces explicit qualification through the rest of the header
- [x] Fix `gfx::Context` discarding its window flags (D9) — the `fullscreen`
      parameter is honoured, and `SDL_WINDOW_FULLSCREEN_DESKTOP` replaces the
      mode-change request. Fullscreen stays the default: this is a game and
      demoscene codebase, and display takeover is the intended presentation. A
      windowed context is now reachable for debugging, which was the point
- [ ] Decide on `line_iterator`'s blank-line behaviour (D4) — behaviour change,
      still open
- [ ] Replace `__RESERVED__` include guards with `#pragma once` (D6)
- [ ] The remaining 33 warnings — **deferred to
      [graphics-backends](../2026-07-25-graphics-backends/) by decision**, since
      every one is in a file that snapshot deletes or rewrites. See the table above
- [ ] Flip `WREEL_WERROR` default to `ON` in `cmake/ProjectOptions.cmake`, once
      the tree is clean rather than via a per-target allowlist. `desktop-software`
      already qualifies; `desktop-debug` clears when `gl_legacy` goes
- [ ] Update the `WREEL_WERROR` row and the status table in `docs/DEVELOPMENT.md`

## Risks

~~**The blank-line behaviour is a real behaviour change, not a cleanup.**~~
**Withdrawn — the defect was not real.** This warned that `line_iterator`
terminates on a blank line and that fixing it would change output for any asset
containing one. Neither holds: an empty stride occurs only at genuine exhaustion,
and an `ico.obj` with a blank line injected between its vertex and face blocks
parses identically. Full reasoning and verification in
[defects.md § D4](defects.md). There is now no behaviour change anywhere in this
snapshot.

~~**`-Wold-style-cast` is the expensive one.**~~ **Withdrawn — measured wrong.**
This claimed the bulk of the mechanical work would be old-style casts in hot
paths like `gfx/obj.cc`. It is 26 warnings tree-wide, and `gfx/obj.cc` plus
`gfx/utils.cc` contribute **1 between them**. It earns its place; keep it
tree-wide. Kept here rather than deleted because it is a reminder that an
estimate written from reading code is not a measurement.

## Open questions

- **`line_iterator`'s blank-line behaviour (D4)** — the one genuine behaviour
  change left in this snapshot. Still open.
- Should `include/util/ascii.hpp` gain a `#if __cpp_lib_ascii` path that
  `using`-declares the `std::` versions once a toolchain in the matrix ships
  `<ascii>`? Cheap to add later, and no compiler in the matrix is close — GCC 8.3
  sets the floor. Probably a comment now and a decision when it matters.

## Resolved questions

- ~~Should the ctype predicates take `unsigned char` or explicit ASCII range
  checks?~~ Neither exactly: they move to `util::ascii.hpp` with P3688 names and
  ASCII semantics, templated over character type. See *Decisions*.
- ~~Is `whitespace_tokenizer` still used?~~ Test-only — `tests/test_string.cc:61`
  is its sole consumer. Kept and modernized rather than deleted, per the
  keep-everything decision. It still defines no `token_type`, so it still cannot
  be passed to `util::tokenize`; that asymmetry with
  `quoted_whitespace_tokenizer` is now a deliberately retained wart.

## References

- [docs/TARGETS.md § C++17 is the ceiling](../../docs/TARGETS.md)
- [P3688R6 ASCII character utilities](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2026/p3688r6.html)
  — the C++26 paper `util/ascii.hpp` tracks
- `include/util/string.hpp`, `loaders/obj.cc`, `tests/test_string.cc`
