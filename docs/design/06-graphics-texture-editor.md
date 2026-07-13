# Graphics & texture editor

> How elads loads, edits, and writes back every graphic a Doom resource can hold — Doom gfx, flats,
> PNG/JPG/WebP/PCX/TGA/ILBM, SVG icons — plus the palette (PLAYPAL/COLORMAP) tools, the classic
> composite-texture editor (TEXTURE1/TEXTURE2 + PNAMES + patches), the ZDoom `TEXTURES` editor, and
> sprite-offset/batch conversion tools. This is the `graphics` module (module 6 of 9; see
> [architecture](01-architecture.md)). It reuses SLADE's `Graphics`/`SIFormat` subsystem almost
> intact. On-disk byte layouts live in [formats-reference](08-formats-reference.md); the archive it
> reads/writes is [data-model](03-data-model.md); textures it produces feed [map-editor](04-map-editor.md).

---

## 1. Scope and module layout

```
src/graphics/
  image/      SImage (the in-memory RGBA/paletted bitmap) + SIFormat loaders/writers
  palette/    Palette, PaletteManager, Translation, COLORMAP generation
  texture/    CTexture, TextureXList (TEXTURE1/2 + PNAMES), ZTextures (TEXTURES lump)
  editors/    wx panels: GfxEntryPanel, PaletteDialog, TextureXEditor, ZTexturesEditor,
              sprite-offset overlay, batch/convert dialogs
  browser/    shared texture/flat/patch browser (also used by the map editor)
```

Two hard rules, mirroring the rest of elads:

- **`image/` + `palette/` + `texture/` are pure data** — no wx, no GL. They decode bytes to
  `SImage`, composite textures, and re-encode. They run headless (CI thumbnails, batch convert,
  the pipeline; see [build-test-pipeline](07-build-test-pipeline.md)).
- **`editors/` is the only place wx/GL appears.** Panels upload an `SImage`'s RGBA to a texture via
  the [render abstraction](02-render-abstraction.md) (`IRenderContext`, GLES 3.1 primary) to preview
  it; they never touch GL directly.

This is exactly SLADE's split: `src/Graphics/` (SImage, SIFormat, Palette, CTexture, TextureXList,
ZTextures) is reused near-verbatim; the wx panels under SLADE `src/MainEditor/UI/EntryPanel/` and
`src/MainEditor/UI/TextureXEditor/` are ported to the elads UI shell.

---

## 2. The in-memory image: `SImage`

Everything decodes to one type. `SImage` holds either an 8-bit paletted buffer (with an attached
`Palette`) or a 32-bit RGBA buffer, plus alpha and metadata. This is SLADE's `SImage`
(SLADE `src/Graphics/SImage/SImage.h`); intent + likely shape below (illustrative, not the exact API):

```cpp
// illustrative
class SImage {
    enum class Type { PalMask, RGBA, AlphaMap };
    Type          type_;
    int           width_, height_;
    vector<uint8> data_;      // indices (PalMask) or RGBA quads
    vector<uint8> mask_;      // per-pixel alpha for PalMask
    Palette       palette_;   // valid when PalMask
    Vec2i         offset_;     // grAb / picture-format offset (sprite/patch origin)
    // + hasPalette(), getRGBAData(), applyTranslation(), etc.
};
```

Key fields for Doom work:

- **`offset_`** — the picture-format left/top offset (sprite handle / patch placement). Preserved
  across load → edit → save, and surfaced by the sprite-offset editor (§8) and grAb (§3.3).
- **Paletted vs RGBA** — Doom gfx and flats load paletted (index into PLAYPAL); PNG/TGA/etc. may be
  either. Conversion is lazy: `getRGBAData()` resolves indices through the current palette on demand
  (§4). Editing keeps paletted images paletted where possible so a round-trip to Doom gfx is lossless.

---

## 3. Supported formats & loaders (`SIFormat`)

`SIFormat` is a registry of format handlers, each answering *isThisFormat(bytes)*, *readImage*,
*writeImage*, and capability flags (paletted? offsets? animation?). This is SLADE's
`SIFormat`/`SIFDoomGfx`/`SIFPng`… family (SLADE `src/Graphics/SImage/Formats/`). Load is by
content sniffing first, falling back to the archive `EntryType` hint from [data-model](03-data-model.md).

| Format | Loader | Paletted? | Offsets | Notes |
|---|---|---|---|---|
| **Doom gfx** (picture format) | `SIFDoomGfx` | yes (PLAYPAL) | yes (hdr) | posts/columns, index 0 = transparent. See [08 §picture](08-formats-reference.md) |
| **Doom flat** | `SIFDoomFlat` | yes | no | raw W×H bytes, no header; size inferred (64×64, 64×65, 128×128, 8×8, 256-wide fullscreen) |
| **Doom gfx (alpha/beta), Doom snea, arah** | variants | yes | some | legacy/pre-release picture variants SLADE already handles |
| **PNG** | `SIFPng` (libpng) | either | **grAb** | reads `grAb` chunk → `offset_`; reads `tRNS`/alpha; keeps palette if indexed |
| **JPEG** | `SIFJpeg` | RGBA | no | libjpeg; lossy, no alpha |
| **WebP** | `SIFWebp` (libwebp) | RGBA | no | libwebp is already a SLADE mandatory dep (SHARED CONTEXT) |
| **PCX** | `SIFPcx` | 8-bit | no | ZSoft PCX, common for old wall textures |
| **TGA** | `SIFTga` | either | no | 16/24/32-bit + RLE |
| **ILBM/LBM** | `SIFIlbm` | 8-bit | no | Amiga IFF planar; body de-planarized on load |
| **GIF, BMP, DDS, various** | respective SIF | mixed | no | reused as-is from SLADE where present |
| **SVG icons** | lunasvg | RGBA | no | **UI icons only**, not a resource format — rasterized at load to themed/DPI sizes (§3.4) |

### 3.1 Doom gfx (picture format)

The picture (aka patch) format is column-major with transparency. A header
(`width, height, leftoffset, topoffset`) is followed by `width` 32-bit column offsets, each pointing
at a chain of *posts*: `topdelta, length, <length bytes + 2 pad>`, terminated by `topdelta == 0xFF`.
Loader reconstructs a `width×height` paletted buffer, marks every pixel not written by a post as
transparent (mask = 0), and stores `leftoffset/topoffset` in `SImage::offset_`. Exact byte layout:
[08 §picture-format](08-formats-reference.md).

### 3.2 Flats

Flats are raw paletted bytes with **no header** — dimension must be inferred. The loader tries the
canonical sizes in order (64×64 → 4096 bytes; also 64×128, 128×128, 8×8 for `TEXTURE`-namespace
misuse, 320×200/256×… fullscreen) and uses the archive namespace (`flats/`, see
[data-model §namespaces](03-data-model.md)) plus entry size to disambiguate. Ambiguous sizes are a
user-overridable guess in the gfx panel.

### 3.3 grAb / offsets

Two offset carriers must round-trip identically:

- **Doom gfx** stores offsets in its header (§3.1).
- **PNG** stores them in a `grAb` ancillary chunk (`{ int32 x, int32 y }`, the ZDoom/SLADE
  convention). `SIFPng` reads `grAb` → `offset_` on load and **re-emits `grAb` on save** so a PNG
  sprite keeps its handle. Losing offsets silently would misplace every sprite in the game — the
  writer treats `offset_ != (0,0)` as "must emit grAb".

### 3.4 SVG icons (lunasvg)

lunasvg rasterizes the app's own toolbar/tree icons to RGBA at the target DPI and light/dark theme
(the Pi OS is Wayland/HiDPI-aware; see [rpi5-target](09-rpi5-target.md)). This is a *UI asset*
pipeline, distinct from resource graphics — no `SImage`-to-archive path. It replaces shipping dozens
of pre-rendered PNG sizes, and keeps icons crisp on fractional-scale Wayland.

---

## 4. Paletted → RGBA conversion at load

The heart of Doom graphics: 8-bit indices are meaningless without a palette. Conversion happens on
demand in `getRGBAData()` / at upload time, never destructively on the stored indices.

```
index buffer (8-bit)  ──►  for each px:  if transparent(px) → RGBA(0,0,0,0)
      + Palette (256 RGB)                 else → RGBA(pal[idx].r, .g, .b, 255)
      + transparency rule
                          ──►  RGBA buffer  ──►  IRenderContext texture (preview)
```

Rules, in the order they matter:

1. **Which palette.** Default is the resource's own `PLAYPAL` (first 256-colour palette), resolved
   through the stacked archive list — base resource + open archives, later-wins — exactly like the
   map editor's texture lookup ([data-model §2.1](03-data-model.md)). Falls back to the built-in Doom
   palette if none present. The gfx panel has a palette dropdown to preview under any loaded palette
   or a translation.
2. **Index 0 as transparency (Doom gfx only).** In the *picture* format, transparency is structural
   (unwritten post pixels), **not** "colour 0". A flat has no transparency; its index 0 is an opaque
   colour. So the transparency rule is per-format, carried by `SImage::mask_`, not a global "index 0
   is clear". (A common misconception; SLADE gets this right and elads preserves it.)
3. **grAb/offset preserved.** Conversion touches colour only; `offset_` is untouched and travels with
   the RGBA the preview/map uploads, so a converted sprite still draws at the right handle.
4. **Palette translations.** A `Translation` remaps index→index (or index→colour range) *before*
   palette lookup — used for player colours (`TRANSLATION`/`translate`), Boom `COLORMAP`-style
   ranges, and the "Colourise/Tint/Translate" gfx operations. This is SLADE's `Translation` class
   (SLADE `src/Graphics/Translation.h`); it composes with §1 palette selection.

---

## 5. PLAYPAL & COLORMAP editor

### 5.1 PLAYPAL

`PLAYPAL` is 14 palettes × 256 RGB triples (14×768 bytes) in Doom — palette 0 is the base; the rest
are pain/pickup/radsuit tints the engine cycles. The palette editor:

- edits any of the N palettes (count = entry size / 768; Heretic/Hexen differ),
- picks/swaps colours, gradients between two indices, imports/exports GIMP/PAL/ACT/PNG palettes,
- previews a loaded gfx live under the selected palette (§4.1),
- writes the whole lump back (§9). This is SLADE's `PaletteDialog` + `PaletteCanvas`.

### 5.2 COLORMAP

`COLORMAP` is 34 tables × 256 bytes: maps 0–31 are the light ramp (bright→dark), map 32 is the
invulnerability/inverse ramp, map 33 is all-black. Each byte is a PLAYPAL index. The editor:

- **views** each ramp as a 256-swatch strip resolved through PLAYPAL,
- **edits** individual cells (rare) or whole ramps,
- **generates** a COLORMAP from a PLAYPAL: for light level `L`∈[0,31], each palette index is mapped
  to the nearest PLAYPAL colour of `colour × (1 − L/31)` (optionally blended toward a fade/fog colour,
  Boom-style). This is the classic "regenerate colormap after editing the palette" workflow and is a
  pure `palette/` function (headless-testable). Cross-ref exact layout: [08 §colormap](08-formats-reference.md).

```
generateColormap(playpal, fadeColor, fogRange):
  for L in 0..31:  for i in 0..255:
     c = lerp(playpal[i], fadeColor, L/31)
     colormap[L][i] = nearestIndex(playpal, c)   // nearest by weighted RGB dist
  colormap[32] = invert ramp;  colormap[33] = all-black index
```

---

## 6. Composite textures — TEXTURE1/TEXTURE2 + PNAMES (classic model)

A Doom wall *texture* is not an image; it is a **recipe**: a named W×H canvas onto which *patches*
(picture-format gfx) are stamped at (x,y) offsets. The data lives in three lumps:

| Lump | Role |
|---|---|
| `PNAMES` | flat list of patch names (8-char), indexed by number |
| `TEXTURE1` | array of `CTexture` defs; each patch entry references a `PNAMES` index |
| `TEXTURE2` | optional second table (registered/commercial split); same format |

Data model: `TextureXList` owns the `CTexture` array + a `PatchTable` (the PNAMES side). This is
SLADE `src/Graphics/CTexture/TextureXList` + `CTexture` + `PatchTable`.

```cpp
// illustrative
struct CTPatch { string name; int16 x, y; /* +Strife dup/blend, +ZDoom extras */ };
class CTexture {
    string           name; uint16 width, height; bool extended; // extended = TEXTURES-style
    vector<CTPatch>  patches;
    // scale/offset/worldpanning live here too when extended (§7)
};
class TextureXList { Format format; vector<CTexture> textures; };  // Normal, Strife11, Textures, Jaguar…
```

### 6.1 The visual composite editor (`TextureXEditor`)

Two-pane editor (SLADE's `TextureXEditor` / `TextureXPanel` + `TextureEditorPanel`):

```
┌─ texture list ─┐┌──── composite canvas ────────────────┐
│ AASHITTY       ││  W×H bounds, checkerboard = transparent│
│ BIGDOOR1  ◄────┼┼─ patches drawn at (x,y); selected has  │
│ SW1BRN1        ││   drag handles; PgUp/Dn = z-order      │
│ …              ││                                        │
└────────────────┘└────────────────────────────────────────┘
┌─ patch list (this texture) ─┐┌─ patch browser (PNAMES) ─┐
│ WALL03_1  x=0  y=0           ││  thumbnail grid, filter   │
└──────────────────────────────┘└───────────────────────────┘
```

Operations: add/remove/rename/resize textures; add patch (from the PNAMES browser, auto-adding to
`PatchTable` if new), drag to position, nudge by pixel, reorder z, duplicate, mirror/flip (extended
only). The canvas composites live: each `CTexture` renders to an `SImage` via `CTexture::toImage()`
(patches sampled through the current PLAYPAL, respecting each patch's transparency), then uploaded
through the RAL for preview. A "patches out of bounds" / "unknown patch" check flags broken defs.

### 6.2 Composite → `SImage`

`CTexture::toImage(patchProvider, palette)` allocates a transparent W×H paletted (or RGBA) buffer and
blits each patch: for each patch, load its gfx, translate to the texture palette, and copy non-masked
pixels at (x,y), clipping to bounds. Extended patches add per-patch translation/blend/alpha/flip
(ZDoom). This one function serves the editor preview, the map-editor material bake (§10), and CI
thumbnails — identical output everywhere.

---

## 7. ZDoom `TEXTURES` lump editor

`TEXTURES` is a text lump defining hi-res/override textures, sprites, flats, graphics, and
walltextures with real transforms — the modern replacement for/overlay on TEXTUREx. Parsed into the
same `CTexture` family with `extended = true`, format `Textures` (SLADE `ZTextures`/`TextureXList`
parses `TEXTURES`). Example:

```
// TEXTURES
WallTexture BIGDOOR7, 128, 128
{
    Scale 2.0, 2.0
    Offset 0, 0
    WorldPanning
    Patch DOOR3_6, 0, 0 { FlipX Alpha 0.8 Style Add Translation "0:255=%[…]" }
    Patch DOOR3_4, 64, 0
}
Sprite POSSA1, 64, 64 { Offset 32, 68  Patch POSSA0 0 0 }
```

Editable properties surfaced by the editor:

| Property | Meaning |
|---|---|
| type | `Texture`/`WallTexture`/`Flat`/`Sprite`/`Graphic` (namespace + engine treatment) |
| `Scale x, y` | render scale (hi-res: a 256×256 patch at Scale 2 covers a 128×128 texture slot) |
| `Offset x, y` | texture-space origin (sprite handle / decal anchor) |
| `WorldPanning` | offsets interpreted in world units, not texture units |
| `NoDecals`, `NullTexture`, `NoTrim` | engine flags |
| per-`Patch` | position, `FlipX/FlipY`, `Alpha`, `Style` (blend), `Translation`, `Rotate`, `Blend/Tint` |

The elads TEXTURES editor offers **both** a text view (Scintilla with the TEXTURES lexer; see
[text-script-editor](05-text-script-editor.md)) and the same visual composite canvas as §6.1,
extended with scale/offset/flip/alpha widgets. Round-trip is text-preserving where possible: unknown
or engine-future keywords are retained verbatim on rewrite rather than dropped. GZDoom is the
authoritative validator for anything ambiguous (SHARED CONTEXT: shell out to GZDoom).

---

## 8. Sprite offset editing

Sprites live or die by their offset (the pixel the engine treats as the actor's feet/centre). The gfx
panel's offset overlay (SLADE `GfxEntryPanel` offset mode):

- shows crosshair at `SImage::offset_`, draggable, with numeric X/Y spin controls,
- one-click presets: **Monster/Thing** (bottom-centre: x = w/2, y = h), **Projectile** (centre:
  x = w/2, y = h/2), **HUD/weapon** (0,0 or graphic-relative), **Custom**,
- writes back to the header (Doom gfx) or `grAb` (PNG) on save (§3.3),
- optional "auto-offset" using an alignment guide relative to a reference frame.

Batch offsetting a whole sprite set (all `POSS*`) applies one preset across selected entries in one
undo step (§9).

---

## 9. Writing changes back to the Archive

All edits ultimately re-encode an `SImage`/`CTexture`/`Palette` to bytes and replace an
`ArchiveEntry`'s data (see [data-model §Archive](03-data-model.md)). The path:

```
edit in panel ──► model object (SImage/CTexture/Palette) mutated
             ──► on Save: SIFormat::writeImage() / TextureXList::writeTEXTUREXData()
                          / Palette::write() / ZTextures serialize
             ──► ArchiveEntry::importMem(bytes)   (marks entry + archive dirty)
             ──► undo step pushed (uniform Ctrl-Z across the app, see 03 §undo)
```

Rules:

- **Format-preserving by default.** A Doom-gfx entry saves back as Doom gfx; a PNG stays PNG (grAb
  re-emitted). Converting format is explicit (§10), because it changes how the engine reads the lump.
- **TEXTUREx is a set write.** Editing one texture rewrites the whole `TEXTURE1`/`TEXTURE2` lump and
  reconciles `PNAMES` (adds new patch names, never silently removes referenced ones). The editor
  batches all texture/patch edits into one save so the two lumps never go out of sync.
- **`TEXTURES` writes text**, preserving unknown tokens (§7).
- **Palette writes** rewrite the full multi-palette lump; a COLORMAP regen is offered when PLAYPAL
  changes (§5.2), but never forced.
- **Dirty/undo integration** matches the rest of elads: archive-level entry edits push undo steps, so
  gfx/texture edits are undoable alongside map edits ([data-model §undo](03-data-model.md)).

---

## 10. Feeding the map editor's material lookup

The 3D visual mode and 2D texture browser ([map-editor §material](04-map-editor.md)) resolve a
sidedef/sector texture *name* to a GPU texture. The graphics module is the resolver:

```
name + namespace  (e.g. "BIGDOOR7" in textures/, "FLOOR4_8" in flats/)
        │  MaterialCache::get(name, ns)
        ▼
  1. TEXTURES (extended) override?  → CTexture::toImage()
  2. TEXTUREx composite?            → CTexture::toImage()
  3. flat / standalone gfx entry?   → SImage from SIFormat
        │  resolve through stacked archive list (later-wins, 03 §2.1)
        ▼
  RGBA (palette applied, §4)  ──►  IRenderContext texture, cached by (name,ns)
```

Details:

- **Lookup precedence** follows Doom engine order: a `TEXTURES` def overrides same-named TEXTUREx
  which overrides a raw entry; resolved against the resource stack (base + open archives, later-wins).
- **One bake, many consumers.** `CTexture::toImage()` (§6.2) is the single compositor; the map
  editor, the texture browser, and CI thumbnails all upload its output. No second code path.
- **Cache invalidation.** Editing a texture, patch, palette, or archive entry bumps a generation
  counter on the affected `(name, ns)`; the map editor's `MaterialCache` drops and re-bakes lazily, so
  a texture edit is visible in 3D mode without reload. Palette edits invalidate *all* paletted
  materials (they all re-resolve colour).
- **Missing name = placeholder**, never a hard failure — the map model treats unresolved textures as a
  soft error rendered as a "no texture" checkerboard ([data-model §9](03-data-model.md)).
- **Colour management on the Pi.** RGBA bakes are plain `GL_RGBA8`; paletted source stays 8-bit in RAM
  and is expanded only into the GPU upload, keeping VRAM/bandwidth low on the VideoCore VII
  (see [rpi5-target](09-rpi5-target.md)).

---

## 11. Batch & convert gfx tools

Headless-capable operations in `image/` + `texture/`, exposed as dialogs and as scriptable Lua
functions ([scripting], reached from the archive tree context menu). All run as one undo step per batch.

| Tool | What it does |
|---|---|
| **Import PNG → Doom gfx** | RGBA/indexed PNG → picture format: quantise to target PLAYPAL (nearest, optional dithering), derive transparency from alpha (α<threshold → masked), carry grAb → header offset |
| **Import PNG → flat** | quantise to PLAYPAL, assert a legal flat size (64×64 …), write raw bytes, place in `flats/` namespace |
| **Convert flat ⇄ gfx** | flat → picture (whole image opaque, no offset) or gfx → flat (must be rectangular & opaque) |
| **Convert any → PNG** | export with palette (indexed PNG for paletted sources) + grAb for offsets |
| **Set offsets (batch)** | apply a sprite-offset preset (§8) across a selection |
| **Colourise / tint / translate** | apply a `Translation`/colour op across a selection |
| **Add to TEXTUREx / PNAMES** | wrap selected patches as auto-generated 1-patch textures; register PNAMES |
| **Palette remap** | re-index images from palette A to palette B by nearest colour |

Conversion correctness notes: quantisation uses a weighted-RGB nearest-colour search against the
chosen palette (same primitive as COLORMAP gen, §5.2); transparency mapping is explicit so a hard-edged
sprite doesn't get a fringe. Because these are pure functions, they are the primary target of the
graphics unit tests and CI golden-image checks ([build-test-pipeline](07-build-test-pipeline.md)).

---

## 12. Reuse ledger & risks

| Reused from SLADE (near-intact) | elads changes |
|---|---|
| `SImage`, `SIFormat` + all format handlers | preview uploads go through RAL/GLES 3.1, not direct GL |
| `Palette`, `Translation`, palette dialog | unchanged logic; ported wx panel |
| `CTexture`, `TextureXList`, `PatchTable`, `ZTextures` | unchanged; `toImage()` feeds map `MaterialCache` |
| `TextureXEditor` UI | reparented into elads wxAUI shell (see [architecture](01-architecture.md)) |
| lunasvg icon rasterization | DPI/theme aware for Wayland HiDPI (see [rpi5-target](09-rpi5-target.md)) |

Risks / open items (see [risks](../risks.md)):

- **Palette selection ambiguity** — which PLAYPAL applies to a given entry across a deep resource
  stack is heuristic; the panel exposes an override so authors aren't surprised.
- **Flat size inference** — non-standard flat sizes need a user hint; document the guessed sizes.
- **TEXTURES round-trip fidelity** — preserve unknown/engine-future tokens verbatim; defer validation
  to GZDoom rather than re-implementing its parser (SHARED CONTEXT).
- **GLES texture limits** — very large hi-res `TEXTURES` overrides can exceed `GL_MAX_TEXTURE_SIZE` on
  V3D; the material bake clamps/reports rather than failing the frame.

### Related docs

- [architecture](01-architecture.md) — where `graphics/` sits in the 9-module graph.
- [render-abstraction](02-render-abstraction.md) — the `IRenderContext` all previews/bakes upload through.
- [data-model](03-data-model.md) — the Archive/`ArchiveEntry` this editor reads and writes, namespaces, undo.
- [map-editor](04-map-editor.md) — the material lookup that consumes composited textures.
- [text-script-editor](05-text-script-editor.md) — the Scintilla `TEXTURES` text view + lexer.
- [formats-reference](08-formats-reference.md) — exact byte layouts for picture format, flats, PLAYPAL, COLORMAP, TEXTUREx, PNAMES.
- [rpi5-target](09-rpi5-target.md) — HiDPI icons, GLES texture limits, VRAM budgeting.
