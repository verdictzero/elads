# Advanced UDMF features — reaching Ultimate Doom Builder parity

Purpose: catalog the **UDMF capabilities Ultimate Doom Builder (UDB) supports that SLADE3's
map editor does not** — the features that make UDB the map editor of choice — and define
elads' plan to match them. This is the "cool UDMF stuff" roadmap.

## The baseline elads already has

elads' map model **losslessly preserves every UDMF field** on every object: any key elads
does not model with a typed field is kept verbatim in that object's `extra` list and written
back on save (see [data-model](03-data-model.md), `src/mapeditor/model/udmf.cpp`). So no data
is ever dropped — the gap to UDB is not *preservation*, it is **rendering and editing** these
fields (which is exactly where SLADE3's map editor falls short). Everything below is framed as
"promote to typed/editable" + "render in visual mode".

## Feature catalog (UDB ✓ / SLADE3 ✗)

Legend for **elads**: `preserved` = round-trips today via `extra`; `model` = typed field;
`render` = drawn in a view; `edit` = interactively editable. Priority: **v1 / v2 / later**.

### 1. Slopes  — *the flagship*
Floor/ceiling planes that tilt. Sources UDB supports and renders:
- **Plane_Align** (line special 181) — hinge the sector's plane along a line to a neighbour.
- **Plane_Copy** (118) — copy another sector's slope.
- **Slope things** — Floor/Ceiling Vertex Slope (9500/9501), Vavoom Floor/Ceiling (9502/9503),
  Slope Floor/Ceiling to Things (9510/9511).
- **UDMF vertex heights** — `zfloor` / `zceiling` on vertices (triangular-sector vertex slopes).
- **UDMF sector plane fields** (GZDoom) — explicit floor/ceiling plane equations.

UDB renders all of these in 2D (slope arrows) and 3D visual mode, with drag handles.
**elads:** ✅ **done (first cut)** — slope things (9500/9501) → 3-point planes **and Plane_Align
(181)**, evaluated per-vertex for floors/ceilings and per-endpoint for walls in the 3D view
(`src/mapeditor/model/planes.*`, `test_slopes`, sloped `render-demo3d`). Remaining: Plane_Copy
(118), UDMF vertex `zfloor`/`zceiling`, 2D slope arrows, and visual-mode drag handles.

### 2. 3D floors
Extra floors inside a sector defined by a **control sector** referenced by **Sector_Set3DFloor**
(line special 160): stacked, translucent, swimmable, non-solid, fog, with their own textures and
light. UDB renders the slabs and lets you edit them in visual mode. SLADE3 does not.
**elads:** `preserved` today; `render` (synthesized slab geometry) + `model` **v2**.

### 3. Per-surface texture transforms (UDMF)
UDB edits and renders, per sidedef part and per flat:
- offsets: `offsetx_top/mid/bottom`, `offsety_top/mid/bottom`
- scale: `scalex_top/mid/bottom`, `scaley_…`; flats `xscalefloor/yscalefloor/xscaleceiling/yscaleceiling`
- rotation: `rotationfloor`, `rotationceiling`
- panning: `xpanningfloor/ypanningfloor/xpanningceiling/ypanningceiling`
SLADE3's 3D mode ignores most of these. **elads:** `preserved`; fold into the renderer's UV
computation (`model` + `render`) **v1–v2** (the textured renderer already computes UVs; these
add offset/scale/rotation terms).

### 4. Per-surface lighting & color (UDMF, GZDoom)
- sidedef: `light`, `lightabsolute`, `light_top/mid/bottom`, `lightfog`
- sector: `lightfloor`, `lightceiling`, `lightfloorabsolute`, `lightceilingabsolute`
- colors: `color_floor`, `color_ceiling`, `color_walltop`, `color_wallbottom`, `lightcolor`,
  `fadecolor` (fog), `colorization`
- glow: `floorglowcolor/height`, `ceilingglowcolor/height`
UDB previews sector color/fog/glow in visual mode. **elads:** `preserved`; `render` (tint +
fog in the shader — the shader already has a per-vertex tint) **v2**.

### 5. Thing UDMF properties
UDB edits and (many) previews: `scale/scalex/scaley`, `pitch`, `roll`, `alpha`, `renderstyle`,
`fillcolor`, `health`, `score`, `gravity`, `floatbobphase`, `conversation`, **`arg0str`**
(named ACS script), `countsecret`, and arbitrary `user_*` variables. SLADE3 exposes far fewer.
**elads:** `preserved`; `model` the common ones + a generic custom-fields editor; sprite preview
with scale/angle **v2**.

### 6. Portals
- **Line portals** — `Line_SetPortal` (156): visual, static, interactive/linked.
- **Sector portals** — `Sector_SetPortal` (1030): floor/ceiling sky/plane/linked portals.
UDB visualizes them. **elads:** `preserved`; `render` **later** (portals are their own rendering
project); at minimum flag/label them in 2D **v2**.

### 7. 3D midtextures & wall flags
`midtex3d` (walkable mid-texture platforms), `wrapmidtex`, `clipmidtex`, `midtex3dimpassible`,
`flipx`/`flipy`, `nogradient`, `noskywalls`, `nofakecontrast`, `smoothlighting`.
**elads:** `preserved`; `render` mid-texture + 3D midtex **v2**.

### 8. Sector/line/thing gameplay UDMF fields
`gravity`, `damageamount`/`damagetype`/`damageinterval`/`leakiness`, `floorterrain`/`ceilingterrain`,
`healthfloor`/`healthceiling`, `soundsequence`, `hidden` (automap), `waterzone`, `moreids`
(extra tags), linedef `arg0str`/`moreids`/`blocking flags` (many UDMF booleans), line `alpha`
+ `renderstyle` (translucent lines). **elads:** `preserved`; surface in property editors **v1–v2**.

### 9. Editor UX UDB has (beyond raw UDMF)
- **Custom fields tab** — add/edit arbitrary UDMF key/values with types on any object.
- **Action/arg browsers** — line/thing specials with named args from the game config.
- **Tag & action helpers**, tag-range management, `moreids`.
- **Visual-mode editing** — drag floor/ceiling/thing heights, paint & auto-align textures,
  copy/paste surface properties, slope handles.
- **Game configurations** — many namespaces (`zdoom`, `gzdoom`, `eternity`, `doom`, `hexen`, …)
  with per-game things/specials/flags.
- **Sky rendering** (`sky1`/`sky2`, skybox), dynamic-light (GLDEFS) and **MODELDEF model** preview.
**elads:** the custom-fields editor is straightforward given lossless `extra` (**v1**); action
browsers come from the game-config work (**v2**); visual-mode editing + sky/lights/models (**later**).

## Why SLADE3 falls short here (and elads won't)

SLADE3 is primarily an **archive/resource** editor; its map editor is secondary and its 3D mode
is a simplified preview that does not render slopes, 3D floors, per-surface transforms, sector
colors, sprites, or portals. UDB is a **dedicated map editor** built around exactly these. elads
inherits SLADE's archive strength **and** builds a UDB-class map editor on top — the render
abstraction ([02](02-render-abstraction.md)) and the textured visual mode
([04](04-map-editor.md)) are the foundation these features slot into.

## Implementation order

1. **Slopes** (v1, starting now) — plane model + slope things + 3-point planes; render sloped
   floors/ceilings/walls in the 3D view.
2. **Per-surface texture transforms** (v1/v2) — offsets/scale/rotation in the UV math.
3. **Sector color/fog/glow** (v2) — shader tint + fog.
4. **3D floors** (v2) — control-sector slab synthesis.
5. **Thing UDMF fields + sprites** (v2), **custom-fields editor** (v1), **action browsers** (v2).
6. **Portals, 3D midtex, sky, models** (later).

Each lands behind the render abstraction, so the Pi/GLES backend inherits it automatically.
