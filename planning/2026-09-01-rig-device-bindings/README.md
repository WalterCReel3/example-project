# Per-device input bindings in `rig`

**Status:** `blocked` — deferred, for the two reasons in decision 4. The decision
is recorded here; the mapping work needs device logs this machine cannot produce
**Written:** 2026-09-01
**Blocked by:** real hardware, for the mapping route. The alias table for a
device is keyed on what that device reports, and no Anbernic or TrimUI pad has
ever been enumerated by this project. Per the standing arrangement, device runs
are the user's. Handling axis input is on the task list, needs no hardware, and
can be started without any of this
**Serves:** [game-layer-and-demo](../2026-08-10-game-layer-and-demo/) decision 14,
which pushes this row out of `game/` and onto `rig`;
[target-validation](../2026-07-25-target-validation/) § 4, which owns collecting
the enumeration this depends on

> **What this document is and is not.** It is a decision record for what to do
> with a device's enumeration once we have one. It is **not** the place the
> enumeration gets collected. **This snapshot supersedes nothing.**

## Scope: which document owns what, since four now touch one question

Checked line by line on 2026-09-01, because "what does the gamepad enumerate as"
is already claimed as a deliverable in three places and a fourth claimant with no
stated relationship to the others would leave the question tracked everywhere and
owned nowhere.

| Document | What it claims | Relationship |
|---|---|---|
| [target-validation](../2026-07-25-target-validation/) § 4 "On-device" | lists "what the gamepad enumerates as, which `skratch/input.cc`'s hard-coded Xbox 360 axis mapping certainly gets wrong" as one of four on-device deliverables | **owns collection.** This document depends on it. Device runs are recorded in its `results.md`, and that stays true |
| [planning/README.md](../README.md), the open-questions list | "**What the pad enumerates as** […] which `rig::Pad` now logs in full" | the index's restatement of the row above. Not a separate owner |
| [coppers-cracktro](../2026-07-26-coppers-cracktro/) | that its warning line, naming the device, its GUID and its counts, "**is** the target-validation deliverable" | **owns the instrument**, and the claim holds — see below. Not superseded |
| this document | what to *do* with the answer: mapping string vs. keysym table, where it ships, who calls it before `Pad` is constructed | **new and previously unowned.** A decision, not a collection step |

So the split is: target-validation collects, `coppers` instruments, this decides.
Nothing above is withdrawn or replaced, and the Blocked-by line names the real
dependency rather than duplicating someone else's deliverable.

## Motivation

The architecture call, from the user, on 2026-09-01: **nothing specific to a game
belongs in the shared library layer.** `rig` knows the inputs. The binding layer
is a shorthand for different *devices* that alias — maybe even 1:1 — to the
specific inputs available on that device. An Anbernic H700 may map a LEFT
differently than a Miyoo, and a Miyoo differently from a TrimUI. The game supplies
what the input *means*; it does not supply what the hardware *is*.

That is device normalisation. It is `rig`'s job, and
[game-layer-and-demo](../2026-08-10-game-layer-and-demo/) decision 14 records the
consequence for the game layer. This document records the `rig` side.

`include/rig/input.hpp` already states the same job in its own words:

> Named actions rather than scancodes and axis indices, because the same demo
> runs on a dev box with a keyboard and on a handheld whose pad nobody has
> enumerated yet.

So the charter is not in dispute and never was. What is at issue is that the
charter is only *half* discharged, and the half that is missing is exactly the
handhelds this project ships to.

## What exists today

Read from `include/rig/input.hpp`, `rig/input.cc` and `probe/main.cc` on
2026-09-01. Nothing in this section was built or run.

`rig::Button` is the canonical named-switch vocabulary — `Up`, `Down`, `Left`,
`Right`, `A`, `B`, `X`, `Y`, `L`, `R`, `Start`, `Select`, plus `Count`. Every
input path funnels into it through `Pad::set()`, and `Pad::mapped()` reports which
path won.

`rig::Pad` takes three paths in preference order:

| | Path | State |
|---|---|---|
| 1 | `SDL_GameController`, when SDL recognises the device — `from_controller` | works; the aliasing is SDL's own database, not ours |
| 2 | raw `SDL_Joystick` otherwise, buttons by index and dpad from hat 0 — `from_joystick_index` | **a guess**, says so in the log, never seen a device |
| 3 | the keyboard, always, in parallel — `from_key` | works, and carries one real device alias — see below |

### The device-alias row is not unstarted

**A device alias already exists in `rig`, for one device, and it is verified
rather than guessed.** On the Miyoo Mini the pad *is* the keyboard: the vendor
SDL2 video driver reads `/dev/input` and translates Linux `KEY_*` codes straight
to keysyms, so no joystick is ever enumerated — the demo logs "no pad attached,
keyboard only" on a handheld covered in buttons. The second keysym group in
`from_key` is Onion's own
[`src/common/system/keymap_sw.h`](https://github.com/OnionUI/Onion/blob/main/src/common/system/keymap_sw.h)
(A=`SPACE`, B=`LCTRL`, X=`LSHIFT`, Y=`LALT`, L1=`e`, R1=`t`, L2=`TAB`,
R2=`BACKSPACE`, Select=`RCTRL`, Start=`RETURN`), and the comment above it records
the device run that proved it:
the first run took the D-pad and Escape and ignored every face button, because
arrows and Escape happened to coincide and nothing else did. Full anatomy in
[docs/MIYOO-MINI.md § 6.3](../../docs/MIYOO-MINI.md).

So the correct framing is not "device aliasing is missing". It is: **device
aliasing is done for one device in one key space, and absent in the other.**

### Two key spaces, which is the structural fact this document turns on

| Target | What SDL enumerates | Alias key space | State |
|---|---|---|---|
| `miyoomini` — Mini, Mini Plus, Mini Flip | **no joystick at all**; the pad arrives as ordinary key events | SDL keysym | **done for the D-pad, the face buttons, L1/R1 and Start/Select**, from Onion's `keymap_sw.h`, confirmed by one device run on two firmwares. L2 and R2 have no `rig::Button` to reach — [D31](../2026-07-25-cxx17-modernization/defects.md) |
| `rk3326` — RG351/RG353 | evdev joystick, expected without an SDL mapping | joystick GUID → button index / hat | **guessed**, never enumerated |
| `h700` — RG35XX Plus/H/SP, RG40XX | evdev joystick, expected without an SDL mapping | joystick GUID → button index / hat | **guessed**, never enumerated |
| TrimUI | — | presumably button indices | **not a configured target** — no preset, no toolchain, no entry in [docs/TARGETS.md](../../docs/TARGETS.md) |

**The Miyoo row is established; the two Mali rows are an expectation.** The Miyoo
half rests on `rig/input.cc`'s own documentation and a device run. The claim that
an `h700` or `rk3326` device *does* enumerate an evdev joystick — and so has a
GUID to key on at all — has never been checked on hardware by this project;
neither Mali target has been run on a device in any capacity
([docs/DEVELOPMENT.md § Status](../../docs/DEVELOPMENT.md#status)). It is the
reasonable expectation for a Linux handheld running Knulli or muOS, and it is the
premise the whole mapping-string route rests on. *Check:* the first line of the
first device log — `input: pad 0 …` versus `input: no pad attached, keyboard
only`. If those devices also turn out to present their pad as a keyboard, decision
1 does not apply to them either and this document needs revisiting rather than
extending.

The two key spaces do not interoperate, and the reason is not stylistic. **A GUID
is a property of a joystick.** On the Miyoo there is no joystick to have one, so
neither an SDL mapping string nor any GUID-keyed table this project could write is
capable of naming that device at all. Any design that specifies one mechanism
silently excludes the only device this project has ever run on.

Worth recording precisely, because the obvious explanation is wrong: the
`miyoomini` preset's generated `SDL_config.h` has `SDL_JOYSTICK_LINUX 1` and
`SDL_JOYSTICK_DISABLED` undefined, so SDL's evdev joystick backend **is compiled
in** on that target. The pad does not appear as a joystick because the kernel
input driver presents it as a keyboard, not because support was configured out.
There is no build option to flip.

### `rig` handles no axis input of any kind

Checked two ways. `grep -i axis` over `rig/` and `include/rig/` returns only two
hits, both in prose comments. `Pad::handle_event` switches on exactly `SDL_QUIT`,
`SDL_KEYDOWN`/`SDL_KEYUP`, `SDL_CONTROLLERBUTTONDOWN`/`UP`,
`SDL_JOYBUTTONDOWN`/`UP` and `SDL_JOYHATMOTION` — and the hat case additionally
requires `event.jhat.hat != 0` to be false, so **hat 0 only**. There is no
`SDL_JOYAXISMOTION` or `SDL_CONTROLLERAXISMOTION` case, and `rig::Button` has no
axis member.

The user's statement of the charter — "rig knows the inputs: keyboard, gamepad,
stick" — is therefore the charter and not the current state. The stick is not
there. This matters beyond analogue sticks; see the Risks section.

### Both instruments now collect the field the remedy needs

The remedy in decision 1 is keyed on the device GUID. Stating precisely which
tools emit it matters, because the loose version of this ("the instrument cannot
collect the field") overstates the blockage and would misroute the task order
below.

**`rig::Pad` does, and always has.** `Pad::open_first_device` logs, for every
attached device:

```
input: pad 0 '<name>' guid <32 hex chars>, sdl mapping <yes|NO>
```

and `coppers` holds a `rig::Pad` as a member — declared in `coppers/demo.hpp`,
constructed in `Demo`'s initialiser list — so **any device run of `coppers`
already emits the GUID of every pad attached to it.** `coppers` ships in the
OnionOS bundle as `App/Coppers/` and its launcher redirects the whole script to a
log on the SD card. `sprites` holds one too. So GUID collection was never blocked
on a code change; it is blocked on hardware, which is a different and less
tractable thing.

[coppers-cracktro](../2026-07-26-coppers-cracktro/)'s claim that "that log line
**is** the target-validation deliverable" is therefore accurate as written, and
this document does not contradict it.

**`wreel-probe` now does too, as of 2026-09-01.** It previously printed the
joystick count, name, `(gamepad mapping present)` or `(raw joystick)` and the
axis/button/hat counts, and no GUID — an inconsistency worth closing, because the
probe is the tool [docs/TARGETS.md](../../docs/TARGETS.md) and target-validation
§ 4 both name as the thing you SSH onto a device and run, and it is the one that
works headless where `coppers` needs a display. `report_input` in `probe/main.cc`
now prints a `guid` line, taken from the device index so it still reports when
`SDL_JoystickOpen` fails. It is in the working tree and compiles clean on
`desktop-software`; **it has not been run on a device.**

**Neither prints "you just pressed button 3".** Authoring a mapping string needs a
press-to-identify pass, and `Pad::set()` logs nothing. This is the instrumentation
gap that remains, and it is the one that is not three lines.

## Decisions

### 1. The remedy for path 2 is an SDL mapping string, and this is adopted rather than decided

[target-validation/results.md](../2026-07-25-target-validation/results.md),
2026-07-27:

> The fallback's button order is the conventional retro-handheld one and is very
> likely wrong somewhere. It is deliberately not presented as correct […] Once a
> device reports its GUID, the right fix is an `SDL_GameControllerAddMapping`
> string rather than more guessing.

`rig/input.cc` says the same thing to the operator at runtime, in
`Pad::open_first_device`: "Button order is a GUESS; verify on hardware and add a
mapping."

**This snapshot adopts that and is not the first word on it.** The reasoning
holds up on re-reading, and it is stronger than a `rig`-side per-device table:

- A mapping string **promotes a device from path 2 to path 1**. It deletes the
  guess rather than layering a second table over it, so there is one aliasing
  mechanism on that device rather than two disagreeing ones.
- SDL's own controller database is the thing being extended, so the fix is
  correct for every SDL program on that device, not only ours.
- No new `rig` API, no new file format of our own, and `from_joystick_index`
  becomes dead weight for any device we have actually mapped — reachable only for
  devices nobody has enumerated yet, which is exactly what it was written for.

The API is verified, not recalled — see decision 3.

### 2. The keysym space stays where it is, and is not folded into the mapping route

Because a GUID cannot name the Miyoo (see What exists today), the keysym table in
`from_key` cannot be expressed as a mapping string and must not be moved behind
one. It stays as a compiled-in per-device table in `rig`.

That leaves `rig` with two aliasing mechanisms rather than one, which is worth
being explicit about rather than discovering later: the shape of the hardware
question is genuinely different in the two cases, and a single mechanism spanning
both would be the same category error
[game-layer-and-demo](../2026-08-10-game-layer-and-demo/) decision 12 objected to
— a rename standing in for an abstraction.

Open: whether the Miyoo keysym table should be *named* as a device alias in the
source rather than sitting as an unlabelled second group in a keyboard switch. It
is currently discoverable only by reading the comment. That is a legibility
question, not a behaviour one, and it is not urgent.

### 3. The SDL spellings below are read, not recalled

Per [CLAUDE.md](../../CLAUDE.md) — this project has already shipped a wrong
forward declaration written from memory of an older SDL_ttf, and
`include/rig/input.hpp` carries a comment about that trap.

Read on 2026-09-01 from the pinned tree at
`build/desktop-software/_deps/sdl2-src/include/`, confirmed to be **2.32.10** from
`SDL_version.h` (`SDL_MAJOR_VERSION 2`, `SDL_MINOR_VERSION 32`, `SDL_PATCHLEVEL
10`):

| Declaration | Header |
|---|---|
| `int SDL_GameControllerAddMapping(const char* mappingString)` | `SDL_gamecontroller.h` |
| `int SDL_GameControllerAddMappingsFromRW(SDL_RWops* rw, int freerw)` | `SDL_gamecontroller.h` |
| `SDL_GameControllerAddMappingsFromFile(file)` — a **macro** over the above, not a function | `SDL_gamecontroller.h` |
| `char* SDL_GameControllerMappingForGUID(SDL_JoystickGUID guid)` | `SDL_gamecontroller.h` |
| `char* SDL_GameControllerMappingForDeviceIndex(int joystick_index)` | `SDL_gamecontroller.h` |
| `int SDL_GameControllerNumMappings(void)` | `SDL_gamecontroller.h` |
| `SDL_HINT_GAMECONTROLLERCONFIG` → `"SDL_GAMECONTROLLERCONFIG"` | `SDL_hints.h` |
| `SDL_HINT_GAMECONTROLLERCONFIG_FILE` → `"SDL_GAMECONTROLLERCONFIG_FILE"` | `SDL_hints.h` |

The mapping-string format, quoted from `SDL_gamecontroller.h`'s own block comment
so it does not have to be recalled either — `guid,name,mappings`, where a joystick
element is `bX` for button index X, `hX.Y` for hat X with value Y, and `aX` for
axis X, and the controller-side names are `a`, `b`, `x`, `y`, `start`, `back`,
`guide`, `dpup`/`dpdown`/`dpleft`/`dpright`, `leftshoulder`/`rightshoulder`,
`leftstick`/`rightstick`, `leftx`/`lefty`/`rightx`/`righty`,
`lefttrigger`/`righttrigger`. Buttons may be used as an axis and vice versa. The
header's own worked example:

```
03000000341a00003608000000000000,PS3 Controller,a:b1,b:b2,y:b3,x:b0,start:b9,guide:b12,back:b8,dpup:h0.1,dpleft:h0.8,dpdown:h0.4,dpright:h0.2,leftshoulder:b4,rightshoulder:b5,leftstick:b10,rightstick:b11,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:b6,righttrigger:b7
```

Two behavioural notes also read from the header rather than assumed:
`AddMappingsFromRW` loads the whole database into memory before processing it,
which the header itself flags for memory-constrained environments — 128 MB
total on the Mini; and a later mapping for a known GUID **overwrites** the one
already loaded.

What is **not** verified and must not be written down as if it were: the
precedence between the two hints and the two calls when more than one supplies a
mapping for the same GUID, and whether the grafted SSD202D drivers in
`platform/miyoomini/sdl2/` touch any of this. Both are Open questions below.

### 4. Deferred, for two independent reasons

Neither reason alone would be decisive; both hold.

**It cannot be done from this machine.** The table for a device is keyed on what
that device reports, and no Anbernic or TrimUI pad has ever been enumerated here.
Anything written before those logs arrive is a second guess stacked on the first,
which is the failure `from_joystick_index` is already in. `Pad` logs the GUID for
precisely this reason.

**Scope.** [game-layer-and-demo](../2026-08-10-game-layer-and-demo/) decision 13
already ruled that "pulling a `rig/` API change into a `game/` scope cleanup is
creep by this project's own standard". The cleanup got larger; the standard did
not change.

### 5. `rig::Pad` needs `released()` before any consumer can bind one verb to two buttons

This was first argued in
[game-layer-and-demo](../2026-08-10-game-layer-and-demo/) decision 13, where the
scope was a `game/` class and this was the `rig` change being kept out of it.
That scope is gone and this is a `rig` finding, so it is carried here in full
rather than left as a pointer. It is **not re-argued and not re-checked** — the
reasoning below is decision 13's, and its conclusion is inherited rather than
independently established.

**The finding.** `pressed` carries information only about buttons that are
currently down. A button released this frame reads `down=false, pressed=false`,
which is indistinguishable from one that has been up for a minute. So a verb's
previous-frame state is not reconstructible from the current frame, and
stateless multi-bind edge detection is **impossible** under the `Source` concept
as documented. That is a hole in the interface's information content, not a
missing predicate.

**The counterexample, which is the load-bearing part.** The plausible predicate
`(any bound button pressed) && !(any bound button down-but-not-pressed)`
survives casual inspection and is wrong. Bind one verb to `A` and `Up`. Hold `A`
on frame *f-1*. On frame *f*, release `A` and press `Up`. The verb was down in
both frames, so no edge is owed — but that predicate reports one. A spurious
edge is exactly the one-shot cue retrigger that the "use `pressed()`, not
`down()`" rule exists to prevent, so the failure is silent and lands in audio
and animation rather than in a crash.

**The remedy.** Widen `rig::Pad` with `released()`, after which
`was_down(b) = (down(b) && !pressed(b)) || released(b)` and the consumer stays
stateless. The alternative — one previous-frame bit per verb plus a mandatory
per-frame tick in the consumer — buys the same thing by giving the binding layer
a lifecycle, which is the shape decision 12 objected to.

**The cost, as checked there.** `rig::Pad` keeps `_down[]` and `_pressed[]` and
no previous-frame array; edges are recorded at the transition in `Pad::set()`,
the single funnel for all three input paths. `_released` is the exact mirror of
`_pressed` — `if (!is_down && _down[index])` — plus a `memset` in
`begin_frame()` and an accessor. Purely additive, so nothing using `rig::Pad`
today breaks, and `down`/`pressed`/`released` is the standard triple, so it
completes an existing idiom rather than inventing generality.

Deferred, but not for the reason the mapping work is: nothing in the tree binds
a verb to anything yet, so there is no consumer to break. No device log would
change that.

## Tasks

**The dependency chain, which has one fewer link than it first appears.** The
obvious reading is that the probe must report GUIDs before GUIDs can be collected
before mapping strings can be written. The middle link does not hold: `coppers`
already logs the GUID of every attached pad on any device run (see "Both
instruments now collect the field the remedy needs" above), so collection is
gated on **hardware only**. The real chain
is:

```
hardware run (user)  ->  GUIDs in results.md  ->  mapping strings  ->  shipping route
                              ^
   probe GUID fix ------------+   done; improved the instrument, gated nothing
   press-to-identify ---------+   needed to author a NON-trivial mapping
```

The one item that needed no hardware has been done, and it was a quality fix
rather than the head of a chain. What waits on a device is the **mapping-string
chain** the diagram draws: the GUIDs, the strings keyed on them, and the route
that ships them. The last two items on the list below sit outside that chain and
are **deferred rather than blocked** — `released()` waits on a consumer that
wants multi-bind, and handling axis input needs no hardware at all. Axis input
is implementable today and is arguably ahead of the mapping work in value.

- [x] **Print the GUID from `wreel-probe`.** Done 2026-09-01, in the working
      tree. `report_input` in `probe/main.cc` now emits a `guid` line via
      `SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(i), …)`, the spelling
      `Pad::open_first_device` already used, read from the device index so it
      survives a failed `SDL_JoystickOpen`. Closes an inconsistency between the
      two instruments; it did **not** unblock anything below it and should not be
      recorded as if it did. Compiles on `desktop-software`; not run on a device
      — see Verification state.
- [ ] **Decide where a press-to-identify pass lives.** Authoring a mapping string
      needs "you just pressed button 3", and nothing prints it. Two candidates,
      and the second is cheaper than it looks: a `--input` mode on `wreel-probe`,
      or SDL's own upstream `controllermap`, whose source **is** in the pinned
      tree at `_deps/sdl2-src/test/controllermap.c` with its two BMP assets. It is
      not currently built — `cmake/Dependencies.cmake` sets `SDL_TEST OFF ... FORCE`
      — and it wants a window and a framebuffer, which on the Miyoo is the thing
      that has historically been hard. Designable without hardware, useless
      without it.
- [ ] **Collect GUIDs and button/hat/axis reports from an `h700` and an `rk3326`
      device.** **The head of the chain, and it needs no code change first** —
      running `coppers` on the device is sufficient today, because it constructs a
      `rig::Pad` and the launcher already redirects its output to a log. User-run;
      logs come back to the repository root and are recorded in
      [target-validation/results.md](../2026-07-25-target-validation/results.md),
      which owns that record. Neither Mali target has ever been run on hardware
      at all ([docs/DEVELOPMENT.md § Status](../../docs/DEVELOPMENT.md#status)),
      so this is a first device run and not only an input question.
- [ ] **Write the mapping strings.** One per device GUID, format per decision 3.
      Blocked on the two above.
- [ ] **Decide how a mapping ships and who installs it.** Candidates, all
      spellings verified in decision 3, none of them chosen: a compiled-in string
      passed to `SDL_GameControllerAddMapping`; a text file beside the bundle
      loaded through `SDL_GameControllerAddMappingsFromFile` and located with
      `rig::asset_path()`; or `SDL_GAMECONTROLLERCONFIG` exported from the
      launcher script. The first needs no asset and cannot go missing; the last
      needs no rebuild to fix a device in the field. **Ordering constraint,
      independent of which is chosen:** `Pad::open_first_device` evaluates
      `SDL_IsGameController(0)` in `Pad`'s constructor, so a mapping added after a
      `Pad` exists does not affect that `Pad`. Whichever route wins has to run
      before construction.
- [ ] **Handle axis input.** No `SDL_JOYAXISMOTION` or `SDL_CONTROLLERAXISMOTION`
      case exists. Needed for analogue sticks, and needed for the dpad-on-an-axis
      case in Risks, which no per-device button table can fix. Independent of the
      mapping work and arguably ahead of it in value.
- [ ] **Add `released()` to `rig::Pad`** when a consumer wants multi-bind.
      Decision 5 above; deferred with it.

## Risks

**A dpad reported on an axis has no Left/Right at all, and no binding table fixes
it.** `Pad::handle_event` takes the dpad from `SDL_JOYHATMOTION` on hat 0 only.
A device that reports its dpad as two axes — common enough on cheap pads — raises
`SDL_JOYAXISMOTION`, which `rig` does not handle, so the events never arrive and
there is nothing for any index table to remap. This is the concrete failure the
"an H700 may map LEFT differently than a Miyoo" example was reaching for, and it
is the reason the axis task above is not merely a stick feature. A mapping string
*does* cover it (`dpup:a1` and similar are legal), which is a further point for
decision 1 over a `rig`-side index table.

**Hat 0 only.** A device with its dpad on hat 1 is silently dead in the same way,
for a smaller reason. Unverified whether any target does this.

**The Miyoo keysym table rests on one device run.** It is sourced from Onion's
`keymap_sw.h`, but the mechanism is the kernel input driver rather than Onion, and
[docs/MIYOO-MINI.md § 6.3](../../docs/MIYOO-MINI.md) records the same binary
taking input correctly on stock firmware with the same bindings compiled in. That
is two firmwares and one table, which is good evidence and not proof. If a third
firmware differs there is no fallback path — the keyboard *is* the pad, so a wrong
keysym table means an uncontrollable device rather than a degraded one.

**A device-specific mapping we author can go stale.** SDL's own database gains
entries upstream, and a pinned SDL that later recognises a device would find our
mapping overriding its own. Since a later mapping overwrites an earlier one for
the same GUID (decision 3), the load order decides who wins, and the shipping
route chosen determines the load order. Worth settling when that task is taken,
not before.

**Adding a mapping makes `mapped()` report `true` where it used to report
`false`.** That is the intent, but `Pad::mapped()` is currently also the thing
that suppresses double-handling of `SDL_JOYBUTTONDOWN` alongside
`SDL_CONTROLLERBUTTONDOWN`, and it feeds `description()`, which reaches the
scrolling message in the demos. The behaviour change is correct and the reporting
change is cosmetic, but both should be looked at rather than assumed harmless.

**TrimUI is named in the motivating call and is not a target.** There is no
preset, no toolchain entry and no row in [docs/TARGETS.md](../../docs/TARGETS.md).
Designing the mechanism around three device families when two are configured is
fine; committing to ship a third is not, and nothing here does.

## Open questions

Each with the check that would answer it, so it does not have to be re-derived.

- **Which shipping route for a mapping?** Compiled-in string, sidecar file via
  `SDL_GameControllerAddMappingsFromFile` and `rig::asset_path()`, or
  `SDL_GAMECONTROLLERCONFIG` in the launcher. *Check:* the declarations are
  confirmed (decision 3); what is not is the **precedence** when more than one
  supplies a mapping for the same GUID. Read `SDL_gamecontroller.c` in the pinned
  tree, not the headers — the headers state the overwrite rule for repeated
  `AddMappings` calls but not the hint-versus-call ordering.
- **Does the vendored Miyoo SDL2 change any of this?** *Partially checked:* the
  `miyoomini` preset's generated `SDL_config.h` has `SDL_JOYSTICK_LINUX 1`, so the
  evdev backend is compiled in and the absent joystick is a device fact rather
  than a build-configuration one. *Not checked:* whether the grafted SSD202D
  video/render/audio drivers in `platform/miyoomini/sdl2/` touch the event source
  in a way that matters. See
  [miyoo-sdl2-fork](../2026-07-31-miyoo-sdl2-fork/) and
  [platform/miyoomini/sdl2/PROVENANCE.md](../../platform/miyoomini/sdl2/PROVENANCE.md).
- **Is the Miyoo keysym table identical on the Mini Plus and the Mini Flip?** All
  three are the same SSD202D with different panels
  ([docs/TARGETS.md](../../docs/TARGETS.md)), so the same kernel input driver is
  likely, but "likely" is what this project's Status discipline exists to refuse.
  *Check:* run the bundle on a Plus or a Flip and read the log.
- **Should the Miyoo alias be named as one in the source?** Decision 2 leaves this
  open. It is currently the second group of a `switch` in `from_key`, identified
  only by a comment.
- **What to do about `TAB`, which the two keysym groups disagree on.** The
  desktop group binds it to Select; Onion's `keymap_sw.h` binds it to L2, and
  `rig::Button` has no L2 to route it to — so on a Miyoo an L2 press reads as
  Select. Recorded with its full reasoning as
  [D31](../2026-07-25-cxx17-modernization/defects.md); the three candidate
  remedies are there and none is picked. It sits here because two of the three
  change `rig`'s vocabulary, which is this document's row. *Check:* it is latent
  only while `Button::Select` has no reader — grep for it before assuming the
  cost is still zero. Onion's R2, `BACKSPACE`, is unbound in `from_key` and is
  the same gap without the collision.
- ~~**Does upstream's `controllermap` utility build from the pinned tree?**~~
  **Checked 2026-09-01: the source is there and the build is off.**
  `_deps/sdl2-src/test/controllermap.c` exists, alongside `controllermap.bmp` and
  `controllermap_back.bmp`; `cmake/Dependencies.cmake` sets `SDL_TEST OFF ... FORCE`
  in both places it configures SDL, and no test binary is present in
  `_deps/sdl2-build/`. So it is an option rather than a dead end, at the cost of
  turning that flag on for one preset and shipping two BMPs. What is still
  unchecked is whether it runs usefully on a handheld panel at all — it draws a
  controller diagram and asks you to press each control, which assumes a display
  this project has had to fight for. Left open for whoever takes the task.
- **Is `from_joystick_index` worth keeping once real mappings exist?** It stays
  useful for a device nobody has enumerated, which is what it was written for.
  Not a question to answer before there is at least one real mapping to compare
  it against.

## Verification state

The analysis here is read-derived, on 2026-09-01, from: `include/rig/input.hpp`,
`rig/input.cc`, `probe/main.cc`,
`coppers/demo.hpp` and `coppers/demo.cc`, `sprites/demo.hpp`,
[planning/README.md](../README.md) and
[coppers-cracktro](../2026-07-26-coppers-cracktro/),
`build/desktop-software/_deps/sdl2-src/include/SDL_gamecontroller.h`,
`SDL_hints.h` and `SDL_version.h`,
`build/miyoomini/_deps/sdl2-build/include-config-release/SDL2/SDL_config.h`,
`cmake/Dependencies.cmake`, a directory listing of
`build/desktop-software/_deps/sdl2-src/test/`,
[docs/MIYOO-MINI.md](../../docs/MIYOO-MINI.md),
[docs/TARGETS.md](../../docs/TARGETS.md),
[docs/DEVELOPMENT.md § Status](../../docs/DEVELOPMENT.md#status),
[target-validation](../2026-07-25-target-validation/) and its `results.md`, and
[game-layer-and-demo](../2026-08-10-game-layer-and-demo/) decisions 12 and 13.

**No pad has ever been enumerated on a handheld by this project.** The Miyoo Mini
keysym path is confirmed by device runs; the raw joystick path has never seen a
device and its button order remains the guess it says it is.

The one code change referenced above — the `guid` line in `probe/main.cc` —
compiles clean on `desktop-software` with `WREEL_WERROR` on, and that is the
whole of what is established about it. **It has never been executed**, on a
device or anywhere else, so the line it prints has never actually been seen. No
other preset was configured or built, and nothing this document says about
`rig`'s runtime behaviour has been tested.

## References

- [game-layer-and-demo](../2026-08-10-game-layer-and-demo/) — decision 14 for the
  architecture call that sends this row to `rig`, decision 12 for why a
  middle-layer class in `game/` was the wrong shape, decision 13 for the
  `released()` analysis relocated above
- [target-validation](../2026-07-25-target-validation/) — § 4 owns collecting the
  device enumeration; [results.md](../2026-07-25-target-validation/results.md)
  2026-07-27 is where the mapping-string remedy was first recorded
- [docs/MIYOO-MINI.md § 6.3](../../docs/MIYOO-MINI.md) — "The pad is a keyboard",
  and the `keymap_sw.h` table
- [miyoo-sdl2-fork](../2026-07-31-miyoo-sdl2-fork/) — what this project's own
  SDL2 for that device is, and what was grafted into it
- [OnionUI/Onion `src/common/system/keymap_sw.h`](https://github.com/OnionUI/Onion/blob/main/src/common/system/keymap_sw.h)
  — the source of the Miyoo keysym bindings
- SDL 2.32.10 `SDL_gamecontroller.h` and `SDL_hints.h`, in the pinned tree under
  `build/*/_deps/sdl2-src/include/` — the authority for every spelling in
  decision 3
