# elads — Implementation Plan (next set of work)

A detailed, referenceable plan for the work after the textured 3D milestone. Each item has a
**goal**, a **concrete design** (files, data structures, algorithms, signatures), a **test**
strategy (headless-verifiable wherever possible), **dependencies**, **effort** (S ≤ ~1 day of
focused work, M ~2–4, L ~a week+), and **priority** (v1 / v2 / later). Read alongside
[design docs](README.md), especially [04-map-editor](design/04-map-editor.md),
[02-render-abstraction](design/02-render-abstraction.md), and
[11-udmf-advanced](design/11-udmf-advanced.md).

---

## 0. Current state (snapshot)

Built + tested (`cmake --preset core|desktop`; core 19 tests, desktop 22):
- **Archives:** WAD + PK3/zip (`src/archive/{wad,pk3}.cpp`), entry-type detection (`entry_type.cpp`).
- **Map model:** `src/mapeditor/model/{map_objects.h,map_model.cpp}`; Doom-binary + UDMF I/O
  (`doom_map_io.cpp`, `udmf.cpp`, **lossless** `extra` key/values); `sector_tri.cpp` (earcut);
  `map_checks.cpp` (validation); **slope planes** `planes.cpp` (slope things + `Plane_Align`,
  `sectorAt`); **save-back** `map_save.cpp` (serialize an edited model into a WAD, in place).
- **Editing:** `src/mapeditor/edit/{selection,map_edit,editor}.cpp` — `Selection`/`pick`, undoable
  operations (move/split/flip, sector & sidedef properties, things, sector authoring) through
  `util::UndoManager`, and a `MapEditor` controller (hover/select/drag/delete/nudge/undo + camera)
  bound into `elads-view`'s 2D mode.
- **Graphics:** palette, Doom picture, flats, TEXTUREx, composite, PNG, `material_set.h`,
  `wad_materials.cpp` (WAD → RGBA textures).
- **Render abstraction:** `src/render/backend/render_backend.h` (`IRenderDevice`/`IRenderContext`,
  `VertexLayout`, depth); **desktop GL backend** `src/render/gl/{egl_headless,gl_backend,offscreen}.cpp`;
  **GLFW window** `glfw_window.cpp`.
- **Renderers:** `src/mapeditor/view2d/map_view_2d.cpp` (2D, with screen↔world unproject),
  `src/mapeditor/view3d/map_view_3d.cpp` (textured, slope-aware 3D, with a screen ray).
- **Tools:** `src/app/render_main.cpp` (`elads-render …` headless PNG), `src/app/view_main.cpp`
  (`elads-view` interactive window + `--auto-screenshot` under Xvfb).
- **Deps available in dev env:** EGL/GL/epoxy + **GLFW 3.3 + Xvfb** (desktop), miniz, earcut.

Not started: GLES/Pi backend, wxWidgets shell, Hexen maps, Lua bindings, node-build/playtest
pipeline wiring, wiring picking/editing into the window, and the remaining
[advanced UDMF](design/11-udmf-advanced.md) visual features (texture transforms, sprites,
colour/fog, 3D floors, dynamic lights).

## Guiding constraints (apply to every item)

- **Everything routes through the render abstraction** — no GL outside `src/render/gl`. New draws
  add to the batched, per-material path in the renderers.
- **Headless-testable first** — features get a CTest that renders to an FBO and asserts pixels, or
  a pure-data test. Interactive bits are screenshot-verified under Xvfb.
- **Pi/GLES port = backend swap** — keep shaders portable (GL 3.3 now; a `#version 310 es` variant
  later); no desktop-GL-only features leak into renderers.
- **Lossless first, typed second** — UDMF already round-trips via `extra`; promoting a field to a
  typed model member must keep writing it back identically.
- **GPLv3**, keep third-party as vendored/subprocess.

---

## Track A — Visual-mode fidelity (UDB parity)

### A1. Sector slopes  — **v1, M** — ✅ done
**Status:** `src/mapeditor/model/planes.{h,cpp}` (`Plane`, `SectorPlanes`, `computeSectorPlanes`,
`sectorAt`) with slope things (9500/9501) + `Plane_Align` (181); the 3D renderer evaluates
per-vertex heights for floors/ceilings and per-endpoint heights for walls. Tests: `test_slopes`;
demo: `elads-render render-demo-slope3d`. Remaining sources (UDMF vertex `zfloor/zceiling`,
`Plane_Copy` 118) and 2D slope arrows are still open.

**Goal:** floor/ceiling planes tilt; 3D view renders sloped floors/ceilings/walls.
**Design:**
- New `src/mapeditor/model/planes.{h,cpp}`:
  ```cpp
  struct Plane { double a,b,c,d;           // a*x+b*y+c*z+d = 0
    double heightAt(double x,double y) const { return -(a*x+b*y+d)/c; }
    static Plane flat(double z){ return {0,0,1,-z}; }
    static Plane fromPoints(double x0,y0,z0, x1,y1,z1, x2,y2,z2); // normal = cross; orient c>0 for floors
  };
  struct SectorPlanes { Plane floor, ceil; };
  std::vector<SectorPlanes> computeSectorPlanes(const MapModel&);  // flat by default
  int sectorAt(const MapModel&, double x, double y);               // point-in-sector via triangulation
  ```
- Slope sources (implement in this order): **slope things** 9500/9501 (collect ≥3 points per sector
  → `fromPoints`); then **Plane_Align (181)** (hinge along the line to the neighbour's height);
  then **UDMF vertex `zfloor`/`zceiling`** for triangular sectors; then **Plane_Copy (118)**.
- Renderer (`map_view_3d.cpp`): compute planes once; replace constant `floorHeight`/`ceilHeight`
  with `plane.heightAt(x,y)` for (a) each triangulated floor/ceiling vertex and (b) each wall
  endpoint (generalize `wallQuad` to per-endpoint bottom/top heights `zBotA,zTopA,zBotB,zTopB`).
- 2D view: draw slope arrows (a short line from sector centroid down-gradient) — optional.
**Test:** `test_slopes` — `fromPoints` correctness; `computeSectorPlanes` on a 3-slope-thing map
gives the expected `heightAt` at the 3 points and interpolates between; headless render of a
sloped demo (add 3× type-9500 things to `demoMap`) asserts a height gradient in the frame.
**Deps:** none new. **Acceptance:** `render-demo3d` shows a visibly tilted floor.

### A2. Per-surface texture transforms  — **v1/v2, M** — ✅ walls (first cut)
**Status:** `src/mapeditor/model/tex_align.h` — wall UVs from sidedef X/Y offsets and the classic
`dontpegtop`/`dontpegbottom` peg rules (one-sided middle, upper, lower), evaluated per endpoint so
sloped walls stay aligned; wired into `map_view_3d`. Anchoring is unit-tested (`test_tex_align`).
Remaining: UDMF per-surface scale/rotation and flat panning/rotation/scale (need typed fields
promoted from `extra`), and back-sidedef textures.

**Goal:** honor UDMF `offsetx_*`, `scalex_*`, `rotation*`, panning, plus sidedef `offsetx/offsety`.
**Design:** promote these from `extra` to typed fields on `Sidedef`/`Sector` (keep writing back).
Fold into UV computation in `map_view_2d`/`map_view_3d`: `uv = rotate(θ) * (worldUV * scale) + offset`.
Walls: base U from distance-along + `offsetx` (+ peg flags for V origin). Flats: `uv = (x,y)/64 * scale`
rotated by `rotation*` + panning. Add pegging flags (`dontpegtop`/`dontpegbottom`) to V origin.
**Test:** unit test the UV transform math; headless render compares two offsets differ.
**Deps:** A-textured renderer (done). **Acceptance:** a WAD map's aligned textures look right.

### A3. Sector color / fog / glow  — **v2, M**
**Goal:** per-sector light color, fog (`fadecolor`), per-plane light (`lightfloor`/`lightceiling`),
glow. **Design:** extend the 3D shader with a fog term (UBO: fog color + density) and multiply the
per-vertex tint by sector `lightcolor`; per-plane light adjusts the tint. Add `color_floor/ceiling`.
Keep the fog cheap (V3D fill-rate). **Test:** headless render asserts fogged distance darkening +
colored tint. **Deps:** A1 optional. **Acceptance:** colored/foggy sectors preview like GZDoom.

### A4. 3D floors  — **v2, L**
**Goal:** render control-sector 3D floors as slabs. **Design:** parse **Sector_Set3DFloor (160)**:
map control-sector (the tagged dummy) → target sectors via the line's tag/args; synthesize slab
geometry (top+bottom flats at the control sector's floor/ceil heights, side walls with the control
sector's textures), respecting translucency/type flags. New `src/mapeditor/model/threed_floors.{h,cpp}`
producing a list of `{targetSector, topZ, botZ, texTop, texBot, texSide, flags}`. Renderer draws
them inside the target sector with depth + blending. **Test:** synthetic map with one 3D floor →
render asserts an inner slab. **Deps:** A1 (planes) helps but not required. **Acceptance:** stacked
floors visible in `render-map3d`.

### A5. Things: sprites + UDMF fields  — **v2, M**
**Goal:** draw things as sprites in 2D (icons) and 3D (billboards), honoring `scale`, `angle`,
`alpha`, `renderstyle`. **Design:** a `ThingCatalog` (from ZScript/DECORATE `//$` editor keys +
MAPINFO DoomEdNums + `gzdoom.pk3` — see [05](design/05-text-script-editor.md)); map thing `type` →
sprite name → `MaterialSet` image; billboard quad facing the camera in 3D. Promote thing UDMF
fields (`scale*`, `pitch`, `roll`, `alpha`, `renderstyle`, `health`, `arg0str`, `user_*`) to typed +
custom-fields. **Test:** render a thing → sprite quad present; catalog join unit test. **Deps:** A5
needs a resolver; start with category icons (no catalog) then real sprites. **Acceptance:** monsters/
items visible in both views.

### A6. 3D midtextures, sky, portals, dynamic lights, models  — **later, L each**
Mid-textures with `wrapmidtex`/`clipmidtex`/`midtex3d`; sky (`sky1/sky2`, skybox) as a background
pass; portals (`Line_SetPortal 156`, `Sector_SetPortal 1030`) as a rendering sub-project; GLDEFS
dynamic lights (capped forward list in a UBO — V3D-friendly); MODELDEF model preview. Each is its own
milestone; documented in [11-udmf-advanced](design/11-udmf-advanced.md).

---

## Track B — Interactivity & editing

### B1. Interactive window `elads-view`  — **v1, M** — ✅ done
**Status:** `src/render/gl/glfw_window.{h,cpp}` + `src/app/view_main.cpp` — a GLFW GL 3.3 window
rendering the same 2D/3D renderers into the default framebuffer; WASD + mouse-look (3D),
drag-pan + wheel-zoom (2D), `Tab` toggles, `F12` screenshots, `R` resets, `Esc` quits. The
`--auto-screenshot`/`--frames` mode is CI-verified under Xvfb (`test_view_window` self-skips with
no display). Picking/editing are wired in-model (B2/B3) but not yet bound to window input.

**Goal:** a real window with live 2D pan/zoom and 3D fly. **Design:** new `ELADS_WINDOW` option +
`src/app/view_main.cpp` + `src/render/gl/glfw_window.{h,cpp}` (GLFW 3.3 already installed). GLFW
creates a GL 3.3 context (GLX/EGL); the **same `GLDevice`/`GLContext`** render into the window's
default framebuffer (no offscreen FBO). Input: mouse-drag pan + scroll-zoom (2D), WASD + mouse-look
(3D), `Tab` toggles 2D/3D, `F12` screenshots via `glReadPixels`→`encodePng`. Load a WAD+map or the
demo. **Test:** run under `xvfb-run` with a `--auto-screenshot out.png --frames 3` mode; assert a
non-empty frame (this is the CI-able path). Real interactivity is user-verified on a desktop.
**Deps:** GL backend (done). **Acceptance:** `xvfb-run elads-view --demo --auto-screenshot` yields a
correct window render; on a real desktop you can pan/fly.

### B2. Picking & selection  — **v1, M** — ✅ done
**Status:** `MapModel::nearestThing`, `view2d::screenToWorld`/`worldToScreen`,
`view3d::screenRay`/`rayHitHeight`, and `edit::{Selection,pick,pickOfType}`
(`src/mapeditor/edit/selection.{h,cpp}`), with precedence vertex → thing → linedef → sector.
Pick math is unit-tested headless (`test_pick`). Interactive highlight/hover awaits the window
binding.

**Goal:** click to select vertices/linedefs/sectors/things. **Design:** reuse the model's
`nearestVertex`/`nearestLinedef` (already in `map_model.cpp`) + point-in-sector (`sectorAt`, A1);
add `nearestThing`. Screen→world unproject for 2D (inverse of `cameraOrtho`); for 3D, ray-pick
(unproject through the view-proj, intersect floor plane / geometry). A `Selection` struct (type +
indices) held by the editor state. Highlight in the renderers (a bright color batch). **Test:**
unit-test pick math (screen point → expected object) headless. **Deps:** B1 for interactive; pick
math testable without a window. **Acceptance:** hovering highlights, clicking selects.

### B3. Editing operations + undo  — **v1, L** — ✅ (first cut)
**Status:** `src/mapeditor/edit/map_edit.{h,cpp}` — `moveVertex(s)`, `setSectorHeights`,
`setSectorTexture`, `setSidedefTexture`, `setSidedefOffset`, `flipLinedef`, `splitLinedef`,
`addThing`/`deleteThing`, and `createSector`, each recorded through `util::UndoManager` and
unit-tested with undo/redo (`test_map_edit`). Still open: `drawSector` with auto-split/merge
against existing geometry, `joinSectors`, `mergeVertices`, and higher-level trace tools.

**Goal:** the actual editor. **Design:** new `src/mapeditor/edit/map_edit.{h,cpp}` — pure operations
on `MapModel` recorded through the existing `util::UndoManager`:
`moveVertices`, `insertVertexOnLine`, `splitLinedef`, `drawSector` (trace a new loop, auto-split/merge),
`join/mergeSectors`, `flipLinedef`, `setTexture`, `setHeights`, `deleteSelection`, `mergeVertices`.
Each is a function taking the model + params, pushing `{apply,revert}` closures. Geometry helpers:
line-line intersection, point-on-line split, sector-boundary re-trace after edits (reuse
`sector_tri` boundary logic). **Test:** each op has a unit test (before/after model state + undo/redo)
— fully headless, high value. **Deps:** none. **Acceptance:** build a square by drawing, move a
vertex, undo — all via tested ops.

### B4. Save-back  — **v1, S/M** — ✅ done (WAD)
**Status:** `src/mapeditor/model/map_save.{h,cpp}` — `saveMapToWad` serializes the model into a
map's lumps (binary via `writeDoomMap`, or a `TEXTMAP`+`ENDMAP` for UDMF), replacing the map's
data lumps in place while preserving the marker and all non-map lumps; `loadMapFromWad` reads
either format. Load→edit→save→reload round-trips are tested (`test_map_save`). PK3 save and an
editor-facing "save file" command remain.

**Goal:** write edits back to disk. **Design:** already have `writeDoomMap` + `writeUdmf` +
`Wad::write` + `Pk3::write`; add an editor "save" that serializes the current `MapModel` into the
map's lumps (binary) or TEXTMAP (UDMF), replaces them in the `Wad`/`Pk3`, and writes the file.
Preserve non-map lumps. **Test:** load→edit→save→reload round-trip equals expected. **Deps:** B3.
**Acceptance:** edit a map in `elads-view`, save, reopen — changes persist.

### B5. Property editors, custom UDMF fields, action browsers  — **v1/v2, M**
**Goal:** edit object properties incl. arbitrary UDMF keys. **Design:** with lossless `extra`, a
**custom-fields editor** (key/value/type table) is straightforward (v1). Typed editors for common
fields. **Action/arg browsers** (line/thing specials with named args) need the **game
configuration** data (below) — v2. UI lives in the eventual app shell (Track C2) or a minimal
GLFW/ImGui panel interim. **Test:** field round-trip through the editor model. **Acceptance:** add a
custom UDMF key in-editor; it saves.

---

## Track C — Platform & packaging

### C1. GLES 3.1 backend (Raspberry Pi)  — **v1 for Pi, M**
**Goal:** run on the Pi 5. **Design:** `src/render/gl/gles_backend.{h,cpp}` implementing the same
`IRenderDevice`/`IRenderContext` against GLES 3.1; shaders get a `#version 310 es` + precision
variant (parameterize the shader strings, or a tiny preamble swap). Context via the EGL path already
in `egl_headless` (works for GLES too) and the GLFW window (GLES profile). Select via
`ELADS_RENDER_BACKEND=gles`. **Test:** the existing render tests, compiled GLES, on the Pi (or an
arm64 runner with a GLES-capable Mesa). **Deps:** run `scripts/probe-gl.sh` on the Pi first to pin
versions. **Acceptance:** `render-demo3d` on the Pi 5 renders identically.

### C2. wxWidgets application shell  — **v1 for the "real app", L**
**Goal:** the docked GUI from [ADR-0003](decisions/ADR-0003-wxwidgets-toolkit.md) — archive
tree, entry editors, text editor, map editor viewport, panels. **Design:** fork/port SLADE's wxAUI
shell; host the `GLDevice`/`GLContext` in a `wxGLCanvas` (EGL path); embed the 2D/3D renderers as
viewports; reuse SLADE's Scintilla editor + archive UI. This is the big integration; `elads-view`
(B1) is the interim standalone viewport that de-risks the GL-in-a-window path. **Test:** launch
under Xvfb, screenshot. **Deps:** B1, and the SLADE fork groundwork. **Acceptance:** one window with
archive + text + map editing.

### C3. Packaging & CI  — **v1, S/M**
**Goal:** installable. **Design:** CPack `.deb` (per Pi OS release), Flatpak manifest; extend CI
with the `desktop` job (done) + a Pi/arm64 GLES job + packaging jobs. **Deps:** C1/C2. **Acceptance:**
`.deb` + Flatpak build in CI.

---

## Track D — Remaining core / pipeline

### D1. Node-build + ACS + playtest pipeline wiring  — **v1, M**
**Goal:** author → build → play. **Design:** `src/pipeline/*` invoking the tools from
`scripts/build-toolchain.sh` (AJBSP embedded or ZDBSP external, `acc`), then a one-click GZDoom-GLES
launch capturing `-stdout` for error surfacing (see [07](design/07-build-test-pipeline.md)). **Test:**
build a small map's nodes; on the Pi, a manual playtest loop. **Acceptance:** "play map" works.

### D2. Hexen-format maps  — **v2, S/M**
Extend `doom_map_io.cpp` with Hexen THINGS (20B) / LINEDEFS (16B) records + a format flag; round-trip
test. **D3. Full PK3 VFS** — mount PK3 folders as namespaces in one archive tree (extend `archive`).
**D4. Lua scripting** — vendor Lua 5.4 + sol2; expose Archive/Map/Graphics to scripts (see
[05](design/05-text-script-editor.md)); a `ScriptManager`; sandboxed. **D5. Textured 2D mode** —
flats/textures in the top-down view (reuse `MaterialSet` + a textured 2D shader).

---

## Recommended sequence (when we resume)

1. ✅ **A1 Slopes** (flagship UDMF; small, visual, testable) → 2. ✅ **B1 elads-view** (makes it
usable) → 3. ✅ **B2 Picking** + **B3 Editing ops** + **B4 Save-back** (the model is now an
*editor*: pick, mutate with undo, save back) →
4. ✅ **Bind B2/B3 into the B1 window** (`MapEditor` controller: hover-highlight, click-select,
   drag-move, keyboard edits → live 2D authoring) → **next:**
5. **A2 texture transforms** + **A5 things/sprites** (visual polish) →
6. **C1 GLES/Pi backend** (ship the first target) →
7. **A3 colors/fog**, **A4 3D floors**, **D1 pipeline** →
8. **C2 wxWidgets shell** (the full app) → **C3 packaging** →
9. **A6 sky/portals/lights/models**, **B5 action browsers/game configs**, **D2–D5**.

Rationale: get to a *usable, shippable Pi editor* (slopes + window + edit + save + GLES) before the
heavier fidelity and full-shell work. Each step stays headless-testable and behind the abstraction.
Steps 1–3 landed as pure, unit-tested model/edit layers plus the standalone window; step 4 (below)
is now done — a `MapEditor` controller wires picking + editing into `elads-view`'s 2D mode
(hover/select, drag-move vertices/things, delete, undo/redo, pan/zoom, grid snap, save), with a
things + hover/selection overlay in the 2D renderer. Step 5 (visual polish) is next.

## Risks & notes
- **Slope/plane math** and **sector re-tracing after edits** are the fiddly correctness areas — lean
  on unit tests. **3D floors** and **portals** are the largest render items — stage them.
- **Interactivity** can only be screenshot-tested here; keep logic in tested pure functions (B2/B3)
  and keep the window a thin shell.
- **V3D fill-rate** on the Pi — keep fog/lights/overdraw cheap; measure on-device (C1).
- **wxWidgets shell (C2)** is the biggest single effort; `elads-view` intentionally de-risks it first.
