# Defect inventory

Found while reading the 2016 sources and writing `tests/test_string.cc`. Each is
real and reachable; none is currently asserted against, because several are
undefined behaviour rather than merely surprising output.

Severity: **UB** = undefined behaviour, **WRONG** = defined but incorrect,
**HYGIENE** = correct today, fragile tomorrow.

D10 and D11 were added 2026-07-25 while researching the character-classification
replacement, D12 and D13 while clearing warnings from the same files, and D14–D16
while replacing the logger. D3's resolution changed at the same time. D6 carried a
factual error and **D4 turned out not to be a defect at all** — both corrected in
place. Everything else is as originally written.

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

`operator=` has the same shape of bug: it assigns four of the five members,
omitting `_stop_sequence`. Harmless where it is used, but assigning between two
iterators with different stop sequences would produce a mixed object.

**Fixed** 2026-07-25 by deleting both hand-rolled functions rather than repairing
them. The members are a `StringType`, a `bool`, two iterators and a pair of them —
nothing owns a resource, so the implicitly generated copy, move and destroy are
correct and complete. Rule of zero.

`tests/test_string.cc` covers it with `"a copied line_iterator carries its whole
state"` and `"line_iterator assignment carries its whole state"`, both of which
iterate the copy through to the end — that is what actually exercises `_cur`,
`_end`, `_valid` and `_stop_sequence` rather than just the stride.

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

**Fixed** 2026-07-25: returns by value, which is correct now that D1 has given the
type a working copy constructor. Kept rather than deleted — it is the standard
iterator idiom and it costs nothing once the copy is sound.

Covered by `"post-increment returns the previous position by value"`, which also
advances the returned object afterwards to confirm it outlives the expression.

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

**Confirmed unused** (2026-07-25): no consumer anywhere in the tree; the only
other mention is a comment in `tests/test_string.cc:281`.

**Fix:** initialise to `false`. *Resolution changed* — the original proposal was
to delete it, but the decision recorded in [README.md](README.md) is to modernize
the unreachable parts of `string.hpp` rather than remove them, so it stays with
`escaped_character_(false)` added to the member initialiser list.

---

## D4 — ~~a blank line silently truncates `line_iterator`~~ NOT A DEFECT

**Withdrawn 2026-07-25 — misdiagnosis.** `include/util/string.hpp`, `_next()`

The original claim was that this line treats a blank line as end-of-stream, so an
OBJ file with a blank line between its vertex and face blocks would load only the
vertices:

```cpp
if (_stride.first == _stride.second) {
    *this = line_iterator();      // becomes the end sentinel
}
```

**It does not.** The stride is `[_cur, pos)`, and it is empty only when
`pos == _cur`. When the stop sequence is found *at* `_cur` — which is exactly what
a blank line means — `pos` becomes `_cur + _stop_sequence.size()`, so the stride
is the non-empty one-character `"\n"`. An empty stride therefore arises **only**
when `_cur == _end`, which is genuine exhaustion. The condition is the correct
termination test, if oddly spelled.

Verified two ways:

- Direct iteration over `"a\n\nb\n"` yields three strides — `"a\n"`, `"\n"`,
  `"b\n"`. Also checked: consecutive blanks, leading blank, trailing blank,
  no trailing newline, empty input, and a lone newline. All correct.
- End-to-end through the loader. A copy of `data/ico.obj` with a blank line
  inserted between `s off` and the first `f` — the precise scenario claimed to
  truncate — parses to **42 vertices / 240 indices**, identical to the original.

`tests/test_string.cc` now asserts the real behaviour exactly, in
`"blank lines are yielded and do not terminate iteration"`. The previous version
of that test carried the wrong claim in its comment and asserted only
`lines.size() >= 1`, which passed either way and so never contradicted it.

**No fix required.** The knock-on corrections — the risk note in
[README.md](README.md) and the Landmines entry in the repository's `CLAUDE.md` —
have been made.

Worth noting what the real defect in this area is: `operator=` assigns four of the
five members, omitting `_stop_sequence`. Harmless where it is used (`_next()`
assigns a default-constructed sentinel, after which the stop sequence is never
read again) but it is the same class of bug as D1.

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

Surveyed 2026-07-25 — 28 headers, three groups:

| Style | Count | Where |
|---|---|---|
| Reserved `__NAME__` | 20 | the 2016 sources |
| `WREEL_<PATH>_HPP` | 6 | `audio/`, `gfx/software/`, `util/format.hpp` |
| `#pragma once` | 2 | `util/ascii.hpp`, `util/format.hpp` |

**Correction:** this entry originally claimed `#pragma once` was "already used by
the new `include/gfx/software/` headers". It was not — those use
`WREEL_GFX_SOFTWARE_CONTEXT_HPP` and `WREEL_GFX_SOFTWARE_SYSTEM_HPP`. No header
in the tree used `#pragma once` when this was written.

**Fix:** `#pragma once`, opportunistically as files are touched. Settled in
favour of the modern spelling over matching the `WREEL_*` guards the other
authored headers happen to use.

**Partly resolved by attrition** rather than by a sweep. Counted again 2026-07-26:
**11 reserved-name guards remain of the original 20.** Seven headers were deleted
outright by the renderer rework — `gfx/context.hpp`, `gfx/obj.hpp`,
`gfx/system.hpp`, `gfx/utils.hpp`, `gfx/primitives.hpp`, `math/vector.hpp` and
`skratch/globals.hpp` — and `gfx/types.hpp`, `gfx/spritesheet.hpp` and
`loaders/obj.hpp` were rewritten with `#pragma once` as they were touched.

Both wrong-module guards this entry called out went with the deletions:
`gfx/context.hpp` declared `__GFX_VIEW_HPP__` and `gfx/primitives.hpp` declared
`__MATH_PRIMITIVES_HPP`.

What is left, all in files nothing has needed to touch yet:

```
include/loaders/image.hpp        include/util/filetypes.hpp
include/loaders/sparrow.hpp      include/util/io.hpp
include/posix/errors.hpp         include/util/mswin/fileimpl.hpp
include/util/deleter.hpp         include/util/nocopy.hpp
include/util/file.hpp            include/util/posix/fileimpl.hpp
include/util/string.hpp
```

Two of those are D13's colliding pair — `util/posix/fileimpl.hpp` and
`util/mswin/fileimpl.hpp` both declare `__UTIL_FILEIMPL_HPP__` — so converting
them fixes that entry as well. `loaders/sparrow.hpp` is due a rewrite anyway when
`load_sparrow` is revived. Every header authored since the survey uses
`#pragma once`.

Three of the 2016 guards are also simply wrong about their own file, which is
worth fixing in the same pass: `gfx/context.hpp` declares `__GFX_VIEW_HPP__`
(leftover from a rename) and `gfx/primitives.hpp` declares
`__MATH_PRIMITIVES_HPP` (wrong module).

---

## D13 — both `fileimpl.hpp` headers declare the same include guard

**WRONG.** `include/util/posix/fileimpl.hpp`, `include/util/mswin/fileimpl.hpp`

Both open with:

```cpp
#ifndef __UTIL_FILEIMPL_HPP__
#define __UTIL_FILEIMPL_HPP__
```

The only pair of colliding guards in the tree. Harmless today purely because
`include/util/file.hpp` selects one or the other by `#ifdef` and never includes
both — the compile-time backend selection described in
[docs/TARGETS.md](../../docs/TARGETS.md) is what keeps it safe.

It is a trap rather than a live bug: anything that includes both — a test
exercising both backends' shared surface, or a future `#include` added for an
unrelated reason — silently gets an empty second header and then fails with
confusing "incomplete type" errors far from the cause, rather than a redefinition
error at the include.

**Fix:** distinct names, or `#pragma once` per D6, which removes the class of
problem entirely.

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

**Fixed** 2026-07-26 **by deletion.** `include/math/vector.hpp` is gone and glm
`1.0.3` is pinned in its place, so the operators are not repaired — they no longer
exist. The alternative was writing `operator+`/`operator+=` correctly plus
`operator-`, `dot`, `cross`, `length`, `normalize` and a `Matrix4`, and the tests
to trust all of it.

It cost five lines outside the OBJ loader, because every consumer —
`gfx/obj.hpp`, `gfx/utils.hpp`, `gfx/types.hpp`, `skratch/application.cc` — was
already a file the renderer rework deleted or rewrote. Reasoning for taking a
vendor type into module signatures is in
[2026-07-26-gfx-renderer-and-gles2](../2026-07-26-gfx-renderer-and-gles2/) and
docs/TARGETS.md.

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

**Fixed** 2026-07-26 **by deletion**, as predicted. `gfx/system.cc` and its static
`_instance` are gone with the rest of the 2016 backend.

There is a `gfx::System` again, and it is a different thing: RAII over
SDL_Init/TTF_Init/IMG_Init with no singleton, no factory and no context ownership.
The old one also heap-allocated contexts into a vector and deleted them in its
destructor — a raw owning pointer plus a lifetime split across two classes. A
context is now owned by whatever created it, which for `skratch` is a
`unique_ptr` member.

Note that `gfx::renderer::System` briefly carried the same context-owning vector
before being promoted to the renderer-neutral `gfx::System`; nothing ever called its
`create_context`, so it went with the promotion.

---

## D9 — `gfx::Context` computes window flags and then discards them

**WRONG.** `gfx/context.cc`

```cpp
Uint32 flags = SDL_WINDOW_OPENGL;
if (fullscreen) {
    flags |= SDL_WINDOW_FULLSCREEN;
    SDL_ShowCursor(SDL_DISABLE);
    SDL_SetRelativeMouseMode(SDL_TRUE);
}
// ...
_sdl_window = SDL_CreateWindow(
    title.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
    dm.w, dm.h, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);   // flags unused
```

`flags` is built up and never passed. The window is **always** fullscreen
regardless of the `fullscreen` argument, so there is no way to run `skratch`
windowed — which makes the demo needlessly hostile to debug, since it grabs the
display and hides the cursor on every run.

Two further problems in the same call:

- `SDL_WINDOW_FULLSCREEN` requests a real display mode change rather than
  `SDL_WINDOW_FULLSCREEN_DESKTOP`, which takes the panel's native mode. On a
  handheld the requested mode may not exist; on Wayland a mode change is not the
  native concept. `gfx::software::Context` already uses `_DESKTOP` for this reason.
- The side effects (`SDL_ShowCursor`, `SDL_SetRelativeMouseMode`) *do* respect the
  parameter, so passing `fullscreen=false` yields a fullscreen window with a
  visible, ungrabbed cursor — the worst of both.

**Fixed** 2026-07-25. `flags` is passed, and `SDL_WINDOW_FULLSCREEN_DESKTOP`
replaces `SDL_WINDOW_FULLSCREEN`.

Two supporting changes, both about making the windowed path *work* rather than
making it the default:

- Windowed runs get three quarters of the desktop, centred, since passing `flags`
  alone would still have produced a desktop-sized window. Safe because `set_3d()`
  takes the viewport and aspect from `SDL_GetWindowSize` *after* creation, so any
  size is handled.
- `System::create_context()` gained a `fullscreen` parameter, defaulting to `true`
  to match `Context`, so a windowed context is reachable without editing either
  default.

**Fullscreen remains the default, deliberately.** An earlier pass here flipped it
to windowed on the grounds that the demo was hostile to debug. That was the wrong
call: this is a game and demoscene codebase, where taking over the display is the
intended presentation, not a misfeature. The defect was that the `fullscreen`
parameter did nothing — not that its default was wrong.

`docs/DEVELOPMENT.md § Running the skratch demo` updated: it previously said there
was no windowed mode without a code change, which is no longer true.

Left alone deliberately: the five `-Wold-style-cast` in this file. It is in the
deferred set per [README.md](README.md), and D9 is a functional fix, not a
cosmetic one.

---

## D10 — the ctype predicates are UB on the dev box and locale-dependent everywhere

**UB.** `include/util/string.hpp:26-38`

```cpp
static pointer_to_unary_function<int, int> is_space = ptr_fun(::isspace);
```

Three separate problems in one line, found while researching the replacement.

**The `char` → `int` conversion is UB for high bytes, but only on some targets.**
C99 7.4p1 permits only values representable as `unsigned char`, or `EOF`. Measured
signedness across the matrix:

```
g++ (x86-64)              char is SIGNED
aarch64-linux-gnu-g++     char is UNSIGNED
arm-linux-gnueabihf-g++   char is UNSIGNED
```

So for an asset byte ≥ 0x80 the dev box passes a negative `int` — UB — while both
ARM targets pass 128–255, which is defined but consults the locale table. The same
OBJ byte therefore takes a different path on the dev box than on every shipping
target. `token_break` reaches this on every non-quote character of every token
`loaders/obj.cc` parses.

**The locale dependency is latent, not dormant.** Nothing in the tree calls
`setlocale`, so everything currently runs in the `"C"` locale and these behave as
ASCII. But `util::to_wstring` / `util::to_string` are built on `mbsrtowcs` /
`wcrtomb`, which do nothing useful until someone calls
`setlocale(LC_CTYPE, "")`. Whoever makes the wide-char path work will silently
change what the OBJ tokenizer considers whitespace — glibc's ISO-8859-1 tables
classify 0xA0, a non-breaking space, as space.

**Thirteen mutable namespace-scope `static`s in a header.** Internal linkage, so
one copy per translation unit, and they are non-`const` so nothing stops a
consumer reassigning `util::is_space`.

**Fix:** `include/util/ascii.hpp`, per the decision recorded in
[README.md](README.md). Note that `SDL_isspace` is *not* a shortcut — SDL gates
two implementations on `HAVE_CTYPE_H`, which is `1` on both `desktop-software` and
the armv7 `miyoomini` build, so it forwards to `::isspace` and inherits all of the
above.

---

## D11 — `mbtowc_iterator` is declared but not implemented

**HYGIENE.** `include/util/string.hpp:407`

```cpp
class mbtowc_iterator
{
protected:
    string::iterator i_;
public:
    typedef input_iterator_tag  iterator_category;
    // ... four more typedefs
    explicit mbtowc_iterator() {}
};
```

It advertises itself as an input iterator through the five iterator typedefs and
then provides no `operator*`, `operator++` or `operator==`. Any attempt to use it
as one fails to compile. It is the counterpart to `wctomb_insert_iterator`, which
*is* implemented, so this looks like abandoned work rather than a deliberate stub.

Compiles today only because nothing instantiates it, and because `string::iterator`
resolves through the `using namespace std;` of D5.

Kept rather than deleted per the decision in [README.md](README.md), so it needs
`std::string::iterator` spelled explicitly once D5 is fixed. Worth a comment
saying it is incomplete, since the typedefs currently imply otherwise.

**Fix:** qualify `std::string::iterator`; document it as unimplemented. Implement
or delete when something actually needs a multibyte-to-wide input iterator.

---

## D12 — `to_wstring`'s error check is unreachable, and the error path is UB

**UB.** `include/util/string.hpp`, `util::to_wstring`

```cpp
size_t n = ::mbsrtowcs(0, &base, 0, &ps) + 1;
if (n == (size_t)-1) {
    throw runtime_error("Invalid multi-byte sequence");
}
vector<wstring::value_type> buffer(n);
::mbsrtowcs(&(*buffer.begin()), &base, n, &ps);
return wstring(buffer.begin(), buffer.end() - 1);
```

`mbsrtowcs(3)` reports failure by returning `(size_t)-1`, but the `+ 1` is
applied first, so on failure `n` is `0` and the comparison against `(size_t)-1`
can never be true. The throw is dead code.

An invalid multi-byte sequence therefore falls straight through to `buffer(0)`,
after which `&(*buffer.begin())` dereferences the end iterator of an empty vector
and `buffer.end() - 1` is out of range. Both are undefined.

Same shape as the `util::format` defect already listed as fixed below: an error
path that nobody exercised, wrong in a way that only fires when something else
has already gone wrong.

Not currently reachable, because nothing calls `to_wstring` — but it is kept per
the decision in [README.md](README.md), so it is worth being correct.

**Fixed** 2026-07-25: check the raw return value before adding the terminator
slot. Found while clearing `-Wold-style-cast` from the same two lines.

---

## D14 — `posix::FileImpl::seek` ignores its offset

**WRONG.** `util/posix/fileimpl.cc`

```cpp
off_t FileImpl::seek(off_t offset, Whence whence)
{
    off_t res = posix::wrap(::lseek(_fd, 0, make_posix_whence(whence)));
    return res;   // ^ passes 0, not offset
}
```

`offset` is accepted and discarded; `lseek` always gets `0`. So
`file.seek(5, SeekSet)` seeks to the start, and `file.seek(2, SeekCur)` is a
no-op that reports the current position — which looks like success.

The `mswin` backend does it correctly (`SetFilePointer(_handle, offset, ...)`), so
the two compile-time backends of the same interface disagree. That is the worst
shape for this kind of bug: it cannot be caught by swapping backends, because only
one is ever compiled.

Invisible until now because **every caller passes 0** — `util::read_all` uses
`seek(0, SeekEnd)` and `seek(0, SeekSet)`, and all three `seek` assertions in
`tests/test_file.cc` used offset 0. A test suite that exercises a parameter only
at its identity value cannot detect the parameter being ignored.

Found by `-Wunused-parameter`, which is the concrete argument for the `-Werror`
gate: the compiler pointed straight at it, and the warning had been sitting in the
build output unread.

**Fixed** 2026-07-25: pass `offset`. `tests/test_file.cc` gains
`"seek honours a non-zero offset"`, covering `SeekSet`, `SeekCur` and a negative
`SeekEnd`.

---

## D15 — `loaders::load_image` leaks the source surface on conversion failure

**WRONG.** `loaders/image.cc`

```cpp
SDL_Surface* image = SDL_ConvertSurfaceFormat(original, ...);
if (image == NULL) {
    util::logging.error() << "ERROR: Couldn't convert image ";
    throw runtime_error("Couldn't convert image");   // `original` never freed
}
SDL_FreeSurface(original);
```

`original` is freed on the success path only. If `SDL_ConvertSurfaceFormat` fails,
the function throws and the source surface leaks — a whole decoded image, which on
a 128 MB device is not a rounding error.

The same block also discarded the reason for both failures: the log line was a
bare `"ERROR: Couldn't load image "` with no `IMG_GetError()`, and the exception
message carried no detail either, so an asset problem produced no diagnosable
output at all.

**Fixed** 2026-07-25 while migrating this file off stream logging: free `original`
before throwing, and include `IMG_GetError()` / `SDL_GetError()` in both messages.

---

## D16 — `gfx/utils.cc` had a dead `using util::logging;`

**HYGIENE.** `gfx/utils.cc`

The file pulled in `util/logging.hpp` and hoisted `util::logging` into namespace
scope without ever logging anything. Harmless in itself, but it made the file a
consumer of the logging header — and therefore of iostreams — for no reason, which
is exactly the sort of accidental dependency that made the iostream cost in
[docs/TARGETS.md § 1a](../../docs/TARGETS.md) so widespread.

`include/util/string.hpp` had the same problem: it included `util/logging.hpp` and
never used it, so every consumer of the tokenizers — including `loaders/obj.cc`
and the test suite — inherited iostreams from a string header.

**Fixed** 2026-07-25: both includes removed, along with the dead
using-declaration.

---

## D17 — `long_of_string`'s error check can never fire

**WRONG.** `include/util/string.hpp`

```cpp
long int long_of_string(const string &s)
{
    char *end;
    long int result = strtol(s.c_str(), &end, 10);
    if (end == 0) {
        throw domain_error("Invalid integer representation");
    }
    return result;
}
```

`strtol(3)` sets `end` to the first unconverted character, so it is null only if
the input pointer was — which `c_str()` never is. The throw is unreachable, and
the function has no way to report failure:

| Input | Returns | Should |
|---|---|---|
| `"abc"` | `0` | throw |
| `"12px"` | `12` | throw |
| `""` | `0` | throw |
| `" 12"` | `12` | throw (leading whitespace) |
| `"99999999999999999999"` | `LONG_MAX`, `errno == ERANGE` unchecked | throw |

The same shape as [D12](#d12) and the `util::format` defect: an error path that
nobody exercised, wrong in a way that only fires once something else has already
gone wrong. `0` is also a legitimate value, so a caller cannot distinguish
failure by inspecting the result.

Found 2026-07-26 while looking for the right home for the strict conversion
`util::xml` needs — this is the utility that should already have been it.

Unreachable today: no consumers anywhere in the tree, which is why it has never
been noticed. It is kept and fixed rather than deleted, per the decision in
[README.md](README.md).

**Fixed** 2026-07-26 by reimplementing it on the new `util::from_string` in
`include/util/number.hpp`, so there is one strict conversion rather than two
spellings of it.

This is a **behaviour change**, unlike the rest of this inventory — the four
rows above now throw where they previously returned a plausible-looking number.
Safe only because the function has no callers; had it any, this would need to be
a separate decision rather than a fix.

Worth noting what the same read turned up in the vendor library, since it is the
identical mistake one layer down: **pugixml's `as_int(def)` does not honour its
own documented contract.** The header says it returns the default "if conversion
did not succeed or attribute is empty", but measured against v1.16 the default
applies only when the attribute is *absent* — a present but non-numeric value
yields `0`, and `"12px"` yields `12`. `util::xml`'s defaulted accessors therefore
route through `util::from_string` rather than re-exporting that behaviour, so
malformed and absent both mean "fallback". Asserted in `tests/test_xml.cc`.

---

## D18 — both Mali handhelds are built as though they had no GPU

**WRONG.** `cmake/toolchains/aarch64-handheld.cmake:29`

```cmake
# Mali GPUs: GLES 2.0 / 3.x is available. The gles2 backend is not written yet,
# so these presets currently build with the software backend; the flag records
# device capability, not backend readiness.
set(WREEL_TARGET_HAS_GPU OFF)
```

The comment states the intent precisely and the code does the opposite: the flag
is set from backend readiness, which is the one thing it says it does not record.

It matters because `WREEL_TARGET_HAS_GPU` is not confined to backend selection.
[`cmake/Dependencies.cmake`](../../cmake/Dependencies.cmake) consumes it to decide
whether SDL2 is built with GL, GLES and EGL at all, so a flag meant to say "which
of our backends is ready" silently configures the *dependency*. Read out of each
preset's generated `SDL_config.h` on 2026-07-26:

| Preset | `OPENGL` | `OPENGL_ES2` | `OPENGL_EGL` | `RENDER_OGL_ES2` |
|---|---|---|---|---|
| `desktop-software` | off | off | off | off |
| `desktop-debug` | on | on | on | on |
| `rk3326` | off | off | off | off |
| `h700` | off | off | off | off |
| `miyoomini` | off | off | off | off |

`miyoomini` and `desktop-software` are correct. `rk3326` and `h700` are not: SDL's
`GLES2_RenderDriver` is compiled out, so even the accelerated 2D path that needs
no code of ours is unavailable on both Mali devices. The build succeeds and
produces a CPU-blitting binary for hardware with a GPU, with no diagnostic.

Invisible because nothing has run on the devices and because the software driver
is a correct fallback — the failure mode is a performance one, on the targets
whose fill rate is already the main risk in
[software-2d-sprites-tiling](../2026-07-25-software-2d-sprites-tiling/).

**Fixed** 2026-07-26: the flag is set from device capability, per its own comment,
and renderer *readiness* is gated separately by `WREEL_ENABLE_GLES2` —
[2026-07-26-gfx-renderer-and-gles2](../2026-07-26-gfx-renderer-and-gles2/)
decision 1. All five presets now report the intended GL support:
`rk3326`/`h700` on, `miyoomini`/`desktop-software` off, and both aarch64 presets
still pass 8/8 under qemu.

A second defect in the same area, found by the fix not taking effect at first:
`Dependencies.cmake` forced `SDL_OPENGL`/`SDL_OPENGLES` **off** for a GPU-less
target but left the GPU case at whatever the cache already held. So a build
directory configured while a target was believed to have no GPU kept GL disabled
after that belief was corrected — the flag changed and nothing happened. Both
directions are now forced, so a reconfigure does not depend on cache history.

Expected to be needed and was not: `libgles-dev:arm64`. The GLES/EGL headers are
architecture-independent and already installed for the host, and SDL `dlopen`s
`libEGL.so`/`libGLESv2.so` rather than linking them — every cross-built binary
still lists only `libm.so.6` and `libc.so.6` as `NEEDED`. No bootstrap change.

---

## Already fixed

| | Where | What |
|---|---|---|
| `util::format` | `include/util/format.hpp` | Reused a consumed `va_list` (UB), never called `va_end`, and sized the buffer one byte short so every message lost its last character. On the error path of both `FileImpl` backends. |
| `if(GCC)` | `CMakeLists.txt` | Tested an undefined variable, so `-Wall -Werror` was never applied. Replaced by the `wreel::warnings` interface target. |
| `TTF_Font` tag | `include/gfx/software/context.hpp` | Forward declaration used `_TTF_Font`; SDL_ttf 2.24 uses `TTF_Font`. Hard error in any TU including both. |

## D19 — `WREEL_WERROR=ON` took effect in no existing build directory

**WRONG.** [`cmake/ProjectOptions.cmake:51`](../../cmake/ProjectOptions.cmake)

```cmake
option(WREEL_WERROR "Treat warnings as errors" ON)
```

The default was flipped from `OFF` to `ON` on 2026-07-26 when the 2016 sources were
deleted, and `docs/DEVELOPMENT.md` records it as on. It was not on anywhere.

`option()` does not overwrite an existing cache entry. Every build directory
configured before the flip keeps the value it was first given, so changing the
default reaches a fresh clone and nothing else. Read out of the caches on
2026-07-27, after a reconfigure of all five:

| Preset | `WREEL_WERROR` in cache |
|---|---|
| `desktop-debug` | `OFF` |
| `desktop-software` | `OFF` |
| `miyoomini` | `OFF` |
| `rk3326` | `OFF` |
| `h700` | `OFF` |

So for a day the gate the tree relies on was off in every directory anyone builds
in, while the configure summary printed `warnings as errors . OFF` and nobody read
it as a contradiction of the documented default.

**This is D18's twin, and the renderer snapshot already wrote the lesson down**:
"only forcing the OFF case meant an existing build directory kept GL disabled
after the flag was corrected — the flag would change and nothing would happen".
The same trap, one option over. A changed default is not a changed setting.

Harmless in outcome, this time: rebuilding all five presets with
`-DWREEL_WERROR=ON` produced **zero warnings and 12/12 tests** everywhere, so the
tree really was as clean as claimed. The defect is that this was not *verified* by
the builds that claimed to verify it — every "zero warnings under `-Werror`" line
in the status table was produced by a build with `-Werror` off.

**Not yet fixed.** The options are to `FORCE` the cache value like
`Dependencies.cmake` does for the SDL GL flags, to set it in `CMakePresets.json`
where a preset's `cacheVariables` are applied on every configure, or to leave the
default alone and accept that flipping one is a per-developer reconfigure. Worth
deciding rather than patching, because the general question — which of these
options are settings and which are defaults — applies to all of them.

## D20 — `~Music` halted whichever track was playing, not its own

**WRONG.** `audio/music.cc`

```cpp
Music::~Music()
{
    if (_music) {
        // Halting first avoids freeing a track the mixer callback is reading.
        if (Mix_PlayingMusic()) {
            Mix_HaltMusic();
        }
        Mix_FreeMusic(_music);
    }
}
```

`Mix_PlayingMusic()` answers "is **any** music playing". SDL_mixer has a single
music channel and exposes no way to ask which `Mix_Music` currently owns it, so
this condition is not about `_music` at all. Destroying any `Music` therefore
stopped whatever track was current.

Invisible for as long as one `Music` existed at a time, which was the whole life
of the class until `coppers::Playlist` became its first two-track consumer.
Changing track constructs the successor, plays it, and then releases the
predecessor — deliberately in that order, so that a failed load leaves the
current track playing instead of dropping into silence. The predecessor's
destructor then halted the successor a fraction of a second after it started.

**Reported from use, not found by the tests.** `test_playlist` covered exactly
this sequence and passed, because it asserted that `current()` returned the new
filename. It did: the bookkeeping was correct and the mixer was silent. A test
that checks the record of what happened rather than what happened is worth very
little, and this is the cleanest example of it in the tree.

**Fixed** 2026-07-27 by deleting the halt outright. `Mix_FreeMusic` already does
the right thing and does it better — from `music.c` in the pinned SDL2_mixer
2.8.2:

```c
Mix_LockAudio();
if (music == music_playing) {
    ...
    music_internal_halt();
}
Mix_UnlockAudio();
```

It compares against its own `music_playing` pointer, which is the comparison we
could not make from outside, and it does so under the audio lock — so the race
the original comment was guarding against was already handled, and handled more
correctly than by our unlocked check.

Regression tests added in both places, and both were confirmed to fail against
the old destructor before the fix was kept: `test_audio` holds two `Music`
objects and asserts the survivor is still playing after the other is released,
and `test_playlist` now asserts `playing()` at every step rather than only the
track name.
