# Asset provenance

Where the files in this directory came from, and what is known about their
licensing. Written 2026-07-27, when the first asset with a traceable origin was
added — everything above that line arrived with the 2016 tree and its provenance
is genuinely unknown rather than merely unrecorded.

**Why this file exists.** Three distribution channels are planned, one of which is
Steam ([planning/2026-07-25-packaging-distribution](../planning/2026-07-25-packaging-distribution/)).
An asset of unknown origin in a store build is an actual problem rather than a
theoretical one, and the way that happens is not malice — it is that nobody wrote
the origin down while it was still cheap to. So: **anything added here gets a row,
including the ones where the honest answer is "unknown".**

## Known

| File | Origin | Licence |
|---|---|---|
| `glyphs-16x16.png` | [ianhan/BitmapFonts](https://github.com/ianhan/BitmapFonts), `fonts/font-pack/5611500775_36717acd08_o.png`, fetched 2026-07-27 | **Unknown — see below** |

### `glyphs-16x16.png`

A 320×48 sheet: 60 cells of 16×16, twenty columns, three rows, ASCII 32–91
(uppercase, digits, basic punctuation; `*` is drawn as a heart). Two tones with no
alpha channel, so it is read as a 1-bit mask and recoloured in code rather than
colour-keyed — which is what lets the palette switch recolour the scroller text as
well as the bars.

Renamed from the upstream `5611500775_36717acd08_o.png`, which is a Flickr-style
identifier carrying no information; the original name is recorded above so the file
stays traceable.

**The licence is unknown and this is not shippable as it stands.** That collection
has no LICENSE file, and its README states plainly:

> "No metadata and I do not claim rights to any of these works, I just thought
> after finding a few of these sites had died that I should make it available."
>
> "I don't remember where much of this collection came from."

So it is a demoscene-archive rip whose author cannot be identified from it. Fine
for developing and measuring against, which is what it is doing now. **Before any
shipped build**, either identify the author and obtain terms, or substitute a
license-clear pixel font — a CC0 or OFL sheet, or one authored for the project.
`coppers::GlyphSheet` takes a grid geometry and a surface, so swapping the file is
a data change and not a code change, provided it is done before something depends
on this specific grid.

## Unknown

Inherited from the 2016 tree. Listed so that "unknown" is recorded rather than
assumed, and so the shipping question is asked once per file rather than never.

| File | Notes |
|---|---|
| `Speedy.fon` | Windows 3.x NE font DLL. Loads via SDL_ttf's FreeType `winfnt` driver; used for the HUD |
| `FreebooterUpdated.ttf` | TrueType. Unused so far |
| `complications.mod`, `complications ii.mod`, `her bloody weekend.mod` | Tracker modules. **Tracker authors are normally named and retain rights**, so these are a clearer attribution question than the anonymous glyph sheet, not a vaguer one. Two of the three were untracked until 2026-07-27 |
| `cavernes.png`, `darknes.png`, `test-pattern.jpg` | Images |
| `jetpackdude.xml` | Sparrow atlas. Orphaned — references a `JetPackDude.png` not in the repository |
| `ico.obj`, `ico.mtl`, `cube.obj`, `teapot.obj` | Meshes. `teapot.obj` is presumably the Newell teapot |
| `test.json`, `test.xml`, `test2.xml`, `testfile` | Test fixtures, authored for this tree |
