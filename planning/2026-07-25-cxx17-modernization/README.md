# C++17 modernization of the 2016 sources

**Status:** `snapshot`
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

## Scope

`include/util/string.hpp` (688 lines, ~20% of the codebase) and the handful of
files that consume it. Specifically:

1. Replace the 13 `ptr_fun`-wrapped ctype predicates with something that is not
   deprecated, and that does not pass `char` to `::isspace` (currently UB for
   negative values — an OBJ file with a stray high byte is undefined behaviour
   today).
2. Replace `not1(is_space)` with a lambda or a named predicate.
3. Drop the `std::unary_function` base from `token_generator`. It contributes
   only `argument_type`/`result_type` typedefs that nothing reads.
4. Fix the defects in [defects.md](defects.md) — several are UB, and two are in
   code paths the OBJ loader uses.
5. Turn on `WREEL_WERROR` and clear the resulting `-Wall -Wextra -Wpedantic
   -Wshadow -Wold-style-cast` output across the tree.

## Non-goals

- **Do not** redesign the tokenizer API. It works, `loaders/obj.cc` depends on
  its exact behaviour, and the test suite now pins that behaviour down. This is
  a language-level migration, not a rewrite.
- **Do not** reach for C++20. `std::ranges` and real `concept` declarations would
  express this far better and are unavailable — the Miyoo Mini toolchain is
  GCC 8.3. See [docs/TARGETS.md § 1](../../docs/TARGETS.md).
- **Do not** introduce Boost. The idioms here are boost-flavoured
  (`quoted_whitespace_tokenizer` is close to `boost::tokenizer`), but a Boost
  dependency on a 128 MB device with five cross-compile targets is not worth it.

## Prerequisite, already met

Tests come first, and they exist: `tests/test_string.cc` has 18 cases and 48
assertions pinning current tokenizer behaviour, including the surprising parts
(escapes keep their backslash; a blank line terminates `line_iterator`). Run it
before and after — behaviour should be identical except where
[defects.md](defects.md) says otherwise.

```sh
ctest --preset desktop-software -R test_string
```

## Tasks

- [ ] Replace ctype predicates; decide on the `unsigned char` cast convention
- [ ] Replace `not1` usages
- [ ] Remove the `std::unary_function` base from `token_generator`
- [ ] Fix `line_iterator`'s copy constructor (D1) and post-increment (D2)
- [ ] Fix `escaped_find`'s uninitialised member (D3)
- [ ] Fix `gfx::Context` discarding its window flags (D9) — gives a
      windowed `skratch`, which makes everything else easier to debug
- [ ] Decide on `line_iterator`'s blank-line behaviour (D4) — behaviour change
- [ ] Remove `using namespace std;` from inside `namespace util` (D5)
- [ ] Replace `__RESERVED__` include guards with `#pragma once` (D6)
- [ ] Clear `-Wall -Wextra -Wpedantic -Wshadow -Wold-style-cast` tree-wide
- [ ] Flip `WREEL_WERROR` default to `ON` in `cmake/ProjectOptions.cmake`
- [ ] Update the `WREEL_WERROR` row in `docs/DEVELOPMENT.md`

## Risks

**The blank-line behaviour is a real behaviour change, not a cleanup.**
`line_iterator` currently *terminates* on an empty stride, so an OBJ file with a
blank line in the middle is silently truncated. Fixing it means the loader starts
reading geometry it previously ignored — which is correct, but will change output
for any asset with a blank line. `data/ico.obj` and `data/teapot.obj` should be
checked before and after.

**`-Wold-style-cast` is the expensive one.** The 2016 code casts C-style
throughout, including in hot paths like `gfx/obj.cc`. Expect this to be the bulk
of the mechanical work, and consider whether it earns its place in the warning
set or should be dropped for legacy translation units only.

## Open questions

- Should the ctype predicates take `unsigned char` (correct) or be replaced with
  explicit ASCII range checks (faster, locale-independent, and arguably what a
  parser for OBJ/JSON actually wants)?
- Is `whitespace_tokenizer` still used? It defines no `token_type`, so it cannot
  be passed to `util::tokenize` — only `quoted_whitespace_tokenizer` can. If
  nothing consumes it, delete it rather than modernising it.

## References

- [docs/TARGETS.md § C++17 is the ceiling](../../docs/TARGETS.md)
- `include/util/string.hpp`, `loaders/obj.cc`, `tests/test_string.cc`
