# Overview & vision

> The north-star document: *why elads exists, what it will and won't be, and how a SLADE fork grows toward Ultimate Doom Builder on a Raspberry Pi 5.* Read this first, then [architecture](01-architecture.md).

---

## 1. Problem statement

There is **no single good Doom-development environment on the Raspberry Pi 5 today.** A Doom
mapper/modder needs three things that currently live in three different, poorly-fitting places on
this hardware:

1. **Resource + graphics + code editing** — WAD/PK3 wrangling, texture/flat/sprite editing,
   ZScript/DECORATE/ACS authoring. Best tool: **SLADE3**.
2. **2D + 3D map editing** — the modern sector-draw / visual-mode workflow. Best tool:
   **Ultimate Doom Builder (UDB)**.
3. **A build/playtest loop** — node building, ACS compilation, and launching the map in
   **GZDoom**.

On the Pi 5, each of these fails for a *hardware* reason, not a taste reason:

| Tool | Blocker on Pi 5 |
|------|-----------------|
| **SLADE3** `master` | Requires **desktop GL 3.3**; map editor has **no software fallback** → falls to slow `llvmpipe`. Also wants **wxWidgets ≥ 3.3.1** (dev series); Pi OS ships wx 3.2.x. |
| **UDB** | C#/.NET on **Mono** (slow, Linux dev-unsupported); its OpenTK renderer needs **desktop GL 3.2 core** — *above* the Pi's native GL **3.1** ceiling. ARM64 untested. |
| **GZDoom** | Actually fine — builds on aarch64 and renders via its **GLES backend** (`+vid_preferbackend 3`). It's the *engine*, not the editor. |

The one fact that dominates every decision (see [rpi5-target](09-rpi5-target.md)):

> **Raspberry Pi 5 GPU = VideoCore VII, Mesa V3D 7.x.** Native caps: desktop GL **core 3.1**
> (GLSL 1.40) / compat 2.1, **GLES 3.1** (GLSL ES 3.10, *conformant* — compute, SSBO,
> `layout(location)`), **Vulkan 1.3** (V3DV, conformant). Desktop GL 3.3/4.x will **never** exist
> on this GPU. ⇒ **Primary render target = GLES 3.1 (`#version 310 es`).**

So the two best editors are each locked out by a GL-version wall that the Pi's driver will never
climb. elads exists to remove that wall by owning the renderer.

---

## 2. Vision

**elads** ("SLADE" spelled backwards) is **one native GPLv3 desktop application** that unifies
SLADE3's resource/graphics/code editing with UDB's 2D+3D map editing, running
**hardware-accelerated on the Raspberry Pi 5 (8 GB, aarch64) first**, and on Linux desktop
generally.

You open a WAD or PK3, edit textures and ZScript and geometry side by side, draw sectors and walk
them in 3D, then build nodes, compile ACS, and launch a GZDoom playtest — **without leaving the
app**, and **without the GPU falling back to software.**

---

## 3. Goals and non-goals

### Goals

- **G1 — HW-accelerated on the Pi 5.** Every render path runs on the V3D GPU via **GLES 3.1**;
  no path silently drops to `llvmpipe`. This is the whole reason the project exists.
- **G2 — One unified app.** Resource, graphics, text/script, and 2D/3D map editing in a single
  process with a shared archive/VFS and one wxAUI shell — not a launcher over separate tools.
- **G3 — Reuse SLADE aggressively (~80%).** Fork SLADE3; keep the Archive/format core, Scintilla
  editor + data-driven lexers, Lua/sol2 scripting, Graphics/SIFormat + TEXTUREx/TEXTURES editors,
  and the wxAUI shell largely intact.
- **G4 — Grow the map editor toward UDB UX/feature parity**, in C++, staged over time.
- **G5 — Orchestrate an ARM64-clean toolchain** (AJBSP/ZDBSP/acc/GZDoom) into a one-click
  author → build → playtest loop.
- **G6 — GZDoom + UDMF first** as the primary engine/format target (see
  [ADR-0005](../decisions/ADR-0005-gzdoom-udmf-first.md)).
- **G7 — Correct, GPLv3-clean licensing** from day one (see [licensing](10-licensing.md)).

### Non-goals

- **N1 — Not a game engine.** elads edits and orchestrates; **GZDoom** renders gameplay and is the
  authoritative ZScript/DECORATE validator. We do not re-implement the Doom engine.
- **N2 — No UDB code port.** UDB is a **UX/feature reference only.** Its C# is the wrong language
  and GPLv3; its `.cfg` game-config format is **not interchangeable** with SLADE's and is not
  copied. We look at UDB, we do not paste it.
- **N3 — Not Windows/macOS-first.** Linux/aarch64 is the design center; other platforms are
  incidental, later, and never at the expense of the Pi target.
- **N4 — No desktop GL 3.3/4.x dependency, ever.** Anything requiring GL > 3.1 core is off the
  table for the primary path.
- **N5 — Not day-one UDB 3D parity.** Slopes, 3D floors, dynamic lights, models, shadowmaps are
  **staged** (see [roadmap](../roadmap.md)); v1 targets SLADE-level 3D, not UDB-level.
- **N6 — Not a general image/audio editor.** Doom-relevant formats only.
- **N7 — Not a full ZScript language server** for v1. Data-driven highlighting + GZDoom-as-validator;
  tree-sitter/LSP is Phase 4.

---

## 4. SLADE-vs-UDB capability map

Where each capability lives today, and elads's plan + owning module (see
[architecture](01-architecture.md) for the module list). "◑" = partial.

| Capability | SLADE | UDB | elads plan (owning module) |
|------------|:----:|:---:|----------------------------|
| WAD/PWAD/IWAD read+write | ✅ | ◑ | **Reuse** SLADE Archive core — `archive/` |
| PK3/ZIP/folder archives (VFS) | ✅ | ◑ | **Reuse** SLADE Archive/VFS — `archive/` |
| Entry-type detection / lump ID | ✅ | ➖ | **Reuse** SLADE EntryType — `archive/` |
| Graphics editor (patches/flats/PNG) | ✅ | ➖ | **Reuse** SIFormat loaders — `graphics/` |
| Palette / COLORMAP / PLAYPAL | ✅ | ➖ | **Reuse** — `graphics/` |
| TEXTUREx / PNAMES editor | ✅ | ➖ | **Reuse** — `graphics/` |
| ZDoom **TEXTURES** editor | ✅ | ➖ | **Reuse** — `graphics/` |
| Text editor w/ Doom lexers (ZScript/ACS/DECORATE) | ✅ | ◑ | **Reuse** Scintilla + data lexers — `texteditor/` |
| Lua/sol2 scripting + plugins | ✅ | ➖ | **Reuse/extend** — `scripting/` |
| **2D map editor** | ◑ | ✅ | **Grow toward UDB** — `mapeditor/view2d/` |
| Sector **draw** mode / snapping | ◑ | ✅ | **Build** (UDB-referenced) — `mapeditor/view2d/` |
| Map **error checks** / analysis | ◑ | ✅ | **Build** — `mapeditor/model/` |
| Thing/actor **browser** from ZScript+MAPINFO | ◑ | ✅ | **Build** — `mapeditor/` + `texteditor/` |
| **3D visual mode** (walls/flats/light) | ◑ | ✅ | **Build on new renderer** — `mapeditor/view3d/` |
| Visual-mode **texture align/paint** | ◑ | ✅ | **Build** (staged) — `mapeditor/view3d/` |
| **Slopes** in 3D | ➖ | ✅ | **Build** (vertex-shader planes, staged) — `mapeditor/view3d/` |
| **3D floors**, dynamic lights, models | ➖ | ✅ | **Later** (staged) — `mapeditor/view3d/` |
| **UDMF** read/write | ✅ | ✅ | **Reuse+extend** — `mapeditor/model/` |
| Hexen / Doom (binary) map formats | ✅ | ✅ | **Reuse+extend** — `mapeditor/model/` |
| **Node building** (BSP) | ➖¹ | ✅ | **Embed AJBSP + bundle ZDBSP** — `pipeline/` |
| **ACS** compilation (acc/bcc) | ➖ | ✅ | **Shell out to acc** — `pipeline/` |
| One-click **playtest** in engine | ➖ | ✅ | **Shell out to GZDoom-GLES** — `pipeline/` |
| **Renderer** | GL 3.3, no map-editor SW fallback | GL 3.2 core | **NEW GLES 3.1 RAL** — `render/` |
| Runs HW-accelerated on **Pi 5** | ❌ (llvmpipe) | ❌ (GL wall) | ✅ **the point** — `render/` |

¹ SLADE does not build nodes; it edits maps and relies on external builders / the engine.
GZDoom rebuilds nodes at load, so external ZDBSP is for **pre-baking** / other engines
(see [build-test-pipeline](07-build-test-pipeline.md)).

**Reading the table:** almost every ✅ under SLADE is a **reuse**; almost every ✅-only-under-UDB
is a **build**, and the single ❌→✅ transition that unlocks all of it is the **renderer**.

---

## 5. Strategy

Four moves, in dependency order. Decisions are recorded as ADRs (see
[decisions/README](../decisions/README.md)).

```
                 ┌─────────────────────────────────────────────┐
                 │  Fork SLADE3  (ADR-0001)  ~80% reuse         │
                 │  Archive · Scintilla · Lua/sol2 · Graphics  │
                 │  · TEXTUREx/TEXTURES · wxAUI shell          │
                 └───────────────────┬─────────────────────────┘
                                     │ replace EXACTLY ONE subsystem
                                     ▼
                 ┌─────────────────────────────────────────────┐
                 │  Render Abstraction Layer  (ADR-0002)       │
                 │  render/backend + render/gles (primary)     │
                 │  + render/desktopgl (fallback / Zink)       │
                 └───────────────────┬─────────────────────────┘
                                     │ everything draws through the RAL
                                     ▼
                 ┌─────────────────────────────────────────────┐
                 │  Grow the map editor toward UDB             │
                 │  mapeditor/view2d → view3d (staged)         │
                 │  UDB = UX/feature REFERENCE only (no C#)    │
                 └───────────────────┬─────────────────────────┘
                                     │ author → build → playtest
                                     ▼
                 ┌─────────────────────────────────────────────┐
                 │  Orchestrate the toolchain  (ADR-0007)      │
                 │  AJBSP(lib) · ZDBSP(ext) · acc · GZDoom-GLES│
                 └─────────────────────────────────────────────┘
```

### 5.1 Fork SLADE ([ADR-0001](../decisions/ADR-0001-fork-slade3.md))

SLADE3 is C++17/wxWidgets/OpenGL and already builds on aarch64. It ships the mature ~80% we do not
want to rewrite. We fork it, keep the Archive/format/text/graphics/scripting layers, and diverge on
exactly one subsystem. SLADE is **GPLv2-or-later** (per-file headers say "version 2 … or any later
version"), which is what makes combining with GPLv3 references lawful; the combined elads work ships
**GPLv3** (see [licensing](10-licensing.md), [ADR-0004](../decisions/ADR-0004-gplv3.md)).

### 5.2 Replace the renderer ([ADR-0002](../decisions/ADR-0002-gles31-render-backend.md))

The **only** subsystem we replace is the renderer. All GL is confined behind a **Render Abstraction
Layer** with a `backend/` interface, a **`gles/` primary backend (`#version 310 es`)**, and a
`desktopgl/` fallback (desktop GL 3.1 / Zink-over-Vulkan for x86 parity). Legacy GL idioms SLADE/UDB
carry — immediate mode, display lists, `GL_QUADS`, fixed-function fog, `GL_POINT_SPRITE`,
double-precision verts — are **lowered** to shader triangles, `gl_PointCoord`, UBO fog, and floats.
GZDoom's GLES backend is the working reference. This is **the core investment**; see
[render-abstraction](02-render-abstraction.md) and risk **R2** in [risks](../risks.md).

### 5.3 Grow the map editor toward UDB

SLADE's map editor is partial; UDB's is the gold standard. We re-implement UDB's UX and 3D feature
set **in C++, on the new renderer**, staged: 2D authoring to UDB-comparable UX for v1, then 3D
visual mode to SLADE parity, then slopes, then (later) 3D floors / dynamic lights / models. **We
read UDB; we never copy its C#** (wrong language, and its `.cfg` format is not adopted). See
[map-editor](04-map-editor.md).

### 5.4 Orchestrate the toolchain ([ADR-0007](../decisions/ADR-0007-nodebuilders-toolchain.md))

We consume the build tools as **ARM64-clean binaries/libraries, not source to merge**:

- **AJBSP** — embedded as a library (node building).
- **ZDBSP** — bundled external tool. Its x86 SSE classifier files are auto-excluded on 64-bit by its
  own CMake (gated on 32-bit) ⇒ builds clean on aarch64 via the scalar path. Used for **pre-baking**
  nodes; GZDoom rebuilds nodes at load anyway.
- **acc** — bundled (~99% portable, endianness-safe C) for ACS compilation.
- **GZDoom** — shelled out to for **playtest** and as the **authoritative ZScript/DECORATE
  validator**. It renders on the Pi only via its **GLES** backend (mainlined ~4.8, run with
  `+vid_preferbackend 3`) and needs a **live GL context** — there is no headless compile-validate
  mode, so CI uses an **offscreen EGL** context and parses `-stdout` logs (risk **R7**).

See [build-test-pipeline](07-build-test-pipeline.md).

---

## 6. Target users

| User | What they need from elads |
|------|---------------------------|
| **Pi-first hobbyist mapper** | A modern sector-draw 2D editor + 3D visual mode that is *actually GPU-accelerated* on their Pi 5, not a software-rendered slideshow. The primary persona. |
| **ZDoom/GZDoom modder** | ZScript/DECORATE/ACS editing with Doom-aware lexers, a Thing browser sourced from their own actors, and GZDoom as the live validator + playtest engine. |
| **Resource/graphics editor** | SLADE-grade WAD/PK3, palette, patch/flat/sprite, TEXTUREx/TEXTURES workflows — unchanged, just accelerated. |
| **Linux desktop user (x86)** | The same app on a normal desktop via the desktop-GL/Zink fallback backend, for parity and porting. |
| **Contributors** | A clearly-layered C++17 codebase where the novel work (render layer + map editor) is isolated from the reused SLADE core. |

---

## 7. Scope summary

Brief, kept consistent with [roadmap](../roadmap.md) (Phase 0 = on-device graphics de-risk spike).

### MVP (Phase 1) — *resource + text + graphics on GLES*
- Forked SLADE, relicensed GPLv3, desktop-GL-3.3-only paths stripped/guarded.
- **Render Abstraction Layer** with a working **GLES 3.1** backend.
- Reused Archive core, EntryType detection, Scintilla + lexers, wxAUI shell.
- Open/edit/save WAD & PK3; edit textures and ZScript — **all HW-accelerated on the Pi**.
- CMake presets + GitHub Actions `ubuntu-24.04-arm` CI + headless-EGL render smoke test.

### v1 (Phases 2–3) — *full 2D authoring + pipeline, then 3D to SLADE parity*
- 2D UDMF map editor to **UDB-comparable UX** (sector draw, snapping, error checks, Thing browser).
- Embedded **AJBSP** + bundled **ZDBSP**; **acc** ACS compilation.
- **One-click GZDoom-GLES playtest** with stdout error surfacing.
- Lua/sol2 scripting console + first plugin API; earcut sector fill with cached VBOs.
- **3D visual mode** on GLES (batched walls/flats, sector light, shader fog, frustum culling),
  then **slopes** via vertex-shader plane evaluation.

### Later (Phase 4) — *UDB-parity visuals, packaging, ecosystem*
- Stacked **3D floors**, capped forward **dynamic lights**, **MODELDEF** models, sprites.
- **Zink/desktop-GL** backend for x86 Linux desktop parity.
- **CPack `.deb`** per Pi OS release (t64) + **Flatpak** on Flathub.
- DoomMake/DECOHack orchestration; optional **Obsidian** generation.
- Optional **ZScript semantic layer** (tree-sitter → diagnostics/go-to-def).

---

## 8. Glossary

Doom/ZDoom terms used across these docs. Deeper format detail in
[formats-reference](08-formats-reference.md); see also the [Doom Wiki](https://doomwiki.org) and
[ZDoom wiki](https://zdoom.org/wiki).

| Term | Meaning |
|------|---------|
| **WAD** | "Where's All the Data" — Doom's archive format; a directory of named **lumps**. |
| **IWAD** | *Internal* WAD: the base game data (e.g. `doom2.wad`, or **Freedoom**). One is required to run. |
| **PWAD** | *Patch* WAD: an add-on WAD loaded *over* an IWAD (a mod/map pack). |
| **IWAD vs Freedoom** | The commercial `doom.wad`/`doom2.wad` are non-free; **Freedoom** is a GPL-compatible, freely-distributable IWAD elads can bundle/target for testing. |
| **PK3** | A ZIP archive used as a WAD replacement (GZDoom/ZDoom), with a folder hierarchy instead of a flat lump list. Handled by the same VFS. |
| **Lump** | A single named data entry inside a WAD (a texture, map, script, sound…). elads calls these *entries*. |
| **Flat** | A 64×64 (palette-indexed) floor/ceiling texture. |
| **Patch** | A column-based sprite/wall image; the building block of composite wall **textures**. |
| **Texture** | A wall texture *composited* from one or more patches, defined in **TEXTUREx**/PNAMES (or ZDoom **TEXTURES**). |
| **Sector** | A closed region of the map floor/ceiling plane with height, light, and flat assignments. |
| **Linedef** | A line segment between two vertices; carries flags, actions/specials, and 1–2 **sidedefs**. |
| **Sidedef** | One side of a linedef; holds the upper/middle/lower wall textures and offsets, and references a sector. |
| **Thing** | A placed object/actor (monster, item, player start, light…) with position, angle, type, and flags. |
| **UDMF** | *Universal Doom Map Format* — a text-based, extensible map format (key/value blocks). elads is **UDMF-first**. |
| **Nodes / BSP** | The Binary Space Partition tree (plus SEGS/SSECTORS/etc.) a **node builder** generates so the engine can render/collide efficiently. |
| **Node builder** | A tool that computes nodes/BSP from raw geometry — here **AJBSP** (embedded) and **ZDBSP** (external). |
| **ACS** | *Action Code Script* — ZDoom's compiled scripting language for map logic. Compiled by **acc** (or bcc). |
| **DECORATE** | ZDoom's legacy text format for defining actors. Superseded by ZScript but still supported. |
| **ZScript** | GZDoom's modern class-based scripting language for actors and game logic; the primary code surface. GZDoom is the authoritative validator. |
| **MAPINFO** | ZDoom lump defining maps, episodes, and metadata; a source for the Thing/actor browser. |

---

## 9. See also

- [architecture](01-architecture.md) — the 9 modules, layering, data flow, threading.
- [render-abstraction](02-render-abstraction.md) — **the core investment**: GLES 3.1 backend, EGL.
- [map-editor](04-map-editor.md) — 2D/3D editing and the UDB UX we mirror.
- [build-test-pipeline](07-build-test-pipeline.md) — AJBSP/ZDBSP/acc/GZDoom orchestration.
- [rpi5-target](09-rpi5-target.md) — the hardware and driver facts that drive everything.
- [licensing](10-licensing.md) — GPLv2-or-later → GPLv3 analysis.
- [roadmap](../roadmap.md) · [risks](../risks.md) · [ADR index](../decisions/README.md).
