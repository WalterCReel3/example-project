# Defect inventory

Found while reading the 2016 sources and writing `tests/test_string.cc`. Each is
real and reachable; none is currently asserted against, because several are
undefined behaviour rather than merely surprising output.

Severity: **UB** = undefined behaviour, **WRONG** = defined but incorrect,
**HYGIENE** = correct today, fragile tomorrow.

---

## D1 — `line_iterator` copy constructor initialises one member of five

**UB.** `include/util/string.hpp`

```cpp
line_iterator(const line_iterator & rh)
    : _stride(rh._stride)
{
}
```

`_valid`, `_cur`, `_end` and `_stop_sequence` are left uninitialised. Any copy of
a `line_iterator` is a garbage object. Reading `_valid` — which `operator==` does
on every comparison — is UB.

Not currently triggered by `loaders/obj.cc`, which only copy-constructs the end
sentinel via the default constructor. It *is* triggered by D2.

**Fix:** copy all members, or `= default` the whole thing once the members are
sane.

---

## D2 — `line_iterator::operator++(int)` returns a reference to a local

**UB.** `include/util/string.hpp`

```cpp
line_iterator& operator++(int)
{
    line_iterator tmp(*this);
    _next();
    return tmp;          // dangling
}
```

Two defects stacked: `tmp` is built with the broken copy constructor from D1, and
then a reference to it is returned after it goes out of scope. Post-increment
cannot be used at all.

`loaders/obj.cc` uses pre-increment, which is why this has never fired.

**Fix:** return by value, and make it depend on a correct copy constructor. Or
delete post-increment outright — nothing needs it, and an input-iterator-shaped
type is not obliged to provide it.

---

## D3 — `escaped_find::escaped_character_` is never initialised

**UB.** `include/util/string.hpp`

```cpp
escaped_find(const EqualityComparable &quote_marker,
             const EqualityComparable &escape)
    : quote_marker_(quote_marker),
      escape_(escape) {}          // escaped_character_ not in the list
```

The first `operator()` call reads an indeterminate `bool`. If it happens to be
truthy, the first character is silently treated as escaped.

`escaped_find` appears to be unused — the tokenizer calls the free function
`find_escaped` instead. Confirm that, and delete it rather than fixing it.

**Fix:** delete, or initialise to `false`.

---

## D4 — a blank line silently truncates `line_iterator`

**WRONG.** `include/util/string.hpp`, `_next()`

```cpp
if (_stride.first == _stride.second) {
    *this = line_iterator();      // becomes the end sentinel
}
```

An empty stride is treated as end-of-stream. A blank line in the middle of a file
produces an empty stride, so **iteration stops there**. An OBJ file with a blank
line between its vertex and face blocks loads only the vertices — no error, no
warning, just missing geometry.

Covered descriptively in `tests/test_string.cc`
(`"blank lines terminate iteration"`), which asserts only the weak property so it
keeps passing until this is deliberately changed.

**Fix:** distinguish "empty line" from "input exhausted" by comparing `_cur`
against `_end` rather than comparing the stride endpoints. This is a **behaviour
change** — see the risk note in [README.md](README.md).

---

## D5 — `using namespace std;` inside `namespace util`

**HYGIENE.** `include/util/string.hpp:24`

In a header, so every translation unit that includes it drags all of `std` into
`util`. This makes `util::copy`, `util::find_if` and friends resolve, invites
ambiguity with anything the project later names the same, and is why the ctype
predicates can be written as bare `ptr_fun` without qualification.

**Fix:** remove it and qualify explicitly. Mechanical but touches most of the
file.

---

## D6 — reserved identifiers as include guards

**HYGIENE.** Tree-wide.

Guards of the form `__UTIL_STR_HPP__`, `__GFX_OBJ_HPP__`, `__NOCOPY_H__`.
Identifiers containing a double underscore, or beginning with an underscore
followed by a capital, are reserved to the implementation in every scope. Working
in practice; formally UB.

**Fix:** `#pragma once` (already used by the new `include/gfx/software/` headers)
or an unreserved name. Do it opportunistically as files are touched.

---

## D7 — `Vector3::operator+` and `operator*` mutate and return a reference

**WRONG.** `include/math/vector.hpp`

```cpp
Vector3& operator+(const Vector3 & rh)
{
    x += rh.x;  // ...
    return *this;
}
```

These are spelled as arithmetic operators but behave as compound assignment.
`a + b` modifies `a`. `auto c = a + b;` copies the mutated `a`. Chained
expressions do something nobody intends.

Not currently exercised — `skratch` assigns components directly rather than using
the operators.

**Fix:** free `operator+` returning by value, plus member `operator+=` returning
`Vector3&`. Also add `operator-`, `dot`, `cross`, `length` and `normalize`, which
a renderer will need shortly. Consider whether `glm` (already in the bootstrap
script's `math` group) should replace this header entirely.

---

## D8 — `gfx::System` leaves a dangling singleton pointer

**WRONG.** `gfx/system.cc`, `include/gfx/system.hpp`

`System::get_instance()` heap-allocates on first call and stores it in the static
`_instance`. `Application::~Application()` then calls `delete _system`. The
destructor does set `_instance = NULL`, so a second `get_instance()` would
allocate afresh — but ownership is split between a static factory and an unrelated
class's destructor, which is why the ordering works only by accident.

The new `gfx::software::System` (`include/gfx/software/system.hpp`) deliberately
does not do this: it is a plain object with explicit ownership.

**Fix:** covered by the `gl_legacy` retirement in
[graphics-backends](../2026-07-25-graphics-backends/). Not worth fixing in code
that is being deleted.

---

## Already fixed

| | Where | What |
|---|---|---|
| `util::format` | `include/util/format.hpp` | Reused a consumed `va_list` (UB), never called `va_end`, and sized the buffer one byte short so every message lost its last character. On the error path of both `FileImpl` backends. |
| `if(GCC)` | `CMakeLists.txt` | Tested an undefined variable, so `-Wall -Werror` was never applied. Replaced by the `wreel::warnings` interface target. |
| `TTF_Font` tag | `include/gfx/software/context.hpp` | Forward declaration used `_TTF_Font`; SDL_ttf 2.24 uses `TTF_Font`. Hard error in any TU including both. |
