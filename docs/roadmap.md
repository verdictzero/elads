# elads roadmap

Living plan. Dates are relative effort estimates, not commitments. Priorities:
**MVP** → **v1** → **later**. The guiding principle is *de-risk graphics first, reuse
SLADE aggressively, and phase 3D fidelity*.

> **Progress:** the design docs + scaffolding are complete, and a substantial **GUI/GL-free
> core** compiles and passes tests (`cmake --preset core && ctest --preset core`, 14 tests):
> **WAD** and **PK3/zip** archives (via vendored miniz) + **entry-type detection**
> (`src/archive`); the map data model + **classic Doom binary** and **UDMF** map
> (de)serialization with lossless round-trip + **earcut sector triangulation** + **map
> validation** (`src/mapeditor/model`); palette, **Doom picture** and **flat** decode/encode,
> **TEXTUREx/PNAMES**, **composite-texture assembly**, and **PNG output** (`src/graphics`);
> foundational utils (`src/util`); the render-abstraction **interface** (`src/render/backend`);
> and an **`elads` CLI**.
>
> **Desktop OpenGL variant** builds (Linux x86-64, `cmake --preset desktop`, 22 tests): an
> **EGL + OpenGL 3.3 backend** implementing the render abstraction (`src/render/gl`), a
> backend-agnostic **2D map renderer** and a **3D visual-mode preview** (`src/mapeditor/view2d`,
> `view3d`: perspective walls from sector heights + earcut floors/ceilings + depth), and
> **`elads-render`**, which draws both to a PNG headlessly (verified on Mesa software GL).
> The 3D view is **textured** — real wall textures + flats resolved through the
> archive→PNAMES/TEXTUREx→GL path (`src/graphics/wad_materials`, `material_set`), UV-mapped and
> batched per texture, with flat-shaded fallback. The **Pi/GLES port** swaps only the backend +
> shader `#version` behind the same interface.
>
> **Editor milestone (core: 19 tests):** **sector slopes** — floor/ceiling *planes* from slope
> things (9500/9501) and `Plane_Align` (181), evaluated per-vertex so the 3D view tilts
> (`src/mapeditor/model/planes`); **picking** — screen→world unproject (2D) + a screen ray with
> floor-plane intersection (3D) + `Selection`/`pick` precedence (`src/mapeditor/edit/selection`);
> **editing operations + undo** — move/split/flip, sector & sidedef properties, things, and
> sector authoring, all recorded through `util::UndoManager` (`src/mapeditor/edit/map_edit`);
> and **save-back** — serialize an edited model into a WAD's map lumps (binary or UDMF TEXTMAP),
> replacing them in place and preserving non-map lumps (`src/mapeditor/model/map_save`). An
> **interactive window** — `elads-view` (`src/render/gl/glfw_window`, `src/app/view_main`) —
> renders the same 2D/3D renderers into a GLFW GL 3.3 window, with an `--auto-screenshot` mode
> verified headless under Xvfb.
>
> **Live 2D editing:** the pieces are now wired into a **`MapEditor` controller**
> (`src/mapeditor/edit/editor`) — hover/click-select, drag-move vertices/things, delete, nudge,
> undo/redo, grid snap, and pan/zoom, all driven by screen-space input and unit-tested headless.
> `elads-view`'s 2D mode binds it to the mouse/keyboard (1–5 edit modes incl. a **draw-sector**
> tool that traces a new sector, LMB select/drag, RMB pan, wheel zoom, F2 save) and the 2D renderer
> draws **things** + a **hover/selection overlay** + the in-progress draw loop. Next
> visual-mode stages: **per-surface texture transforms, thing sprites, colour/fog, 3D floors, and
> dynamic lights**; then the GLES/Pi backend and the wxWidgets shell.

## Phase 0 — On-device bring-up spike (2–4 weeks)

**Goal:** de-risk the single biggest unknown (graphics) before committing architecture.

- [ ] Build SLADE `master` on a real Pi 5 (target image pinned) against **system wx 3.2.x**.
- [ ] Capture `glxinfo`/`eglinfo`/`vulkaninfo` via `scripts/probe-gl.sh`; record GL core/compat,
      GLES, and Vulkan versions + `GL_MAX_DRAW_BUFFERS`. Attach to an issue.
- [ ] Prove a **GLES 3.1 context** via wxGLCanvas EGL and render one textured triangle in a viewport.
- [ ] Build **AJBSP, ZDBSP, acc** from source via `scripts/build-toolchain.sh` (aarch64).
- [ ] Run one **author → nodes → ACS → GZDoom-GLES playtest** loop by hand.
- [ ] **Decide:** GLES-only vs Zink-supported; pin the Pi OS image + Mesa version.

**Exit criteria:** confirmed GLES 3.1 rendering on-device + a working manual toolchain loop.

## Phase 1 — MVP: resource + text + graphics on GLES (2–3 months)

**Goal:** a forked SLADE running hardware-accelerated on the Pi 5 for non-map work plus basic
2D map editing.

- [ ] Fork SLADE; relicense combined project **GPLv3**; strip/guard desktop-GL-3.3-only paths.
- [ ] **Render Abstraction Layer** with a working GLES 3.1 backend; port gfx/texture preview and
      the 2D map canvas onto it.
- [ ] Reuse **Archive core**, **EntryType** detection, **Scintilla editor + data-driven lexers** verbatim.
- [ ] **wxAUI shell** with the idle-handling fix; boots and edits WAD/PK3, textures, and text on-device.
- [ ] **CMake presets** + **GitHub Actions `ubuntu-24.04-arm`** CI + **headless EGL** render smoke test.

**Exit criteria:** open/edit/save WAD & PK3, edit textures and ZScript, all HW-accelerated on the Pi.

## Phase 2 — v1: full 2D map authoring + build/test pipeline (3–4 months)

**Goal:** complete author→compile→playtest loop for UDMF/Doom/Hexen maps.

- [ ] 2D UDMF map editor to **UDB-comparable UX** (sector draw, snapping, error checks,
      Thing browser from ZScript + MAPINFO).
- [ ] Embedded **AJBSP** + bundled **ZDBSP** node building; **acc/bcc** ACS compilation.
- [ ] **One-click GZDoom-GLES playtest** with stdout error surfacing.
- [ ] **Lua/sol2** scripting console and first plugin API surface.
- [ ] earcut-based sector fill with cached VBOs.

**Exit criteria:** build a small map end-to-end and play it, without leaving elads.

## Phase 3 — v1: 3D visual mode to SLADE parity, then slopes (2–3 months)

**Goal:** usable in-editor 3D walkthrough on Pi hardware.

- [ ] 3D visual mode on GLES: texture-batched walls/flats, sector light, shader fog, frustum culling.
- [ ] **Slope** support via plane evaluation in the vertex shader.
- [ ] Visual-mode **texture painting/alignment** and thing placement.
- [ ] Performance pass for the V3D tile-based GPU (minimise FBO switches/overdraw).

**Exit criteria:** navigate a real map in 3D at interactive framerates on the Pi 5.

## Phase 4 — later: UDB-parity visuals, packaging, ecosystem (ongoing)

- [ ] Stacked **3D floors**, capped forward **dynamic lights**, **MODELDEF** models, sprites.
- [ ] **Zink/desktop-GL** backend for x86 Linux desktop parity.
- [ ] **CPack `.deb`** per Pi OS release + **Flatpak** on Flathub; `.desktop`/AppStream assets.
- [ ] **DoomMake/DECOHack** build orchestration and optional **Obsidian** generation.
- [ ] Optional **ZScript semantic layer** (tree-sitter grammar → diagnostics/go-to-def).

## Open decisions (tracked)

See the plan's open-decisions list. Current defaults: fork-SLADE · docs+scaffolding first ·
GPLv3 · GZDoom UDMF-first · staged 3D (SLADE-level for v1) · certify against Pi OS Trixie.
