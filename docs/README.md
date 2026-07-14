# elads design documentation

This is the internal design/architecture reference for **elads**, a unified
Doom/ZDoom/GZDoom development environment for Linux with the **Raspberry Pi 5** as the
first-class target. Start with the overview, then the architecture.

## Design docs

| # | Doc | What it covers |
|---|-----|----------------|
| 00 | [overview](design/00-overview.md) | Vision, goals/non-goals, SLADE-vs-UDB capability map, the strategy |
| 01 | [architecture](design/01-architecture.md) | The 9 modules, layering, data flow, threading |
| 02 | [render-abstraction](design/02-render-abstraction.md) | **The core investment** — GLES 3.1 backend, legacy-GL→GLES lowering, EGL/context, fallbacks |
| 03 | [data-model](design/03-data-model.md) | Archive/VFS, entry model + type detection, map geometry model, UDMF, undo |
| 04 | [map-editor](design/04-map-editor.md) | 2D editing (render/hit-test/tools), 3D visual mode staging, UDB UX to mirror |
| 05 | [text-script-editor](design/05-text-script-editor.md) | Scintilla lexers, Thing/actor browser, Lua scripting + plugin API |
| 06 | [graphics-texture-editor](design/06-graphics-texture-editor.md) | SIFormat loaders, palette, TEXTUREx & ZDoom TEXTURES |
| 07 | [build-test-pipeline](design/07-build-test-pipeline.md) | AJBSP/ZDBSP/acc, node formats, GZDoom-GLES playtest |
| 08 | [formats-reference](design/08-formats-reference.md) | WAD/PK3/UDMF/TEXTUREx/PNAMES/flats/sprites/sound/music reference |
| 09 | [rpi5-target](design/09-rpi5-target.md) | BCM2712/VideoCore VII, Mesa V3D GL/GLES/Vulkan, Wayland/EGL, thermal, packaging |
| 10 | [licensing](design/10-licensing.md) | GPLv3 rationale, SLADE v2+→v3 analysis, reuse policy, third-party inventory |
| 11 | [udmf-advanced](design/11-udmf-advanced.md) | UDMF features UDB can do that SLADE3 can't (slopes, 3D floors, per-surface transforms, colors, portals…) + elads plan |

## Decisions

Architecture Decision Records live in [`decisions/`](decisions/). See the
[ADR index](decisions/README.md). Key decisions:

- [ADR-0001](decisions/ADR-0001-fork-slade3.md) — Fork & extend SLADE3
- [ADR-0002](decisions/ADR-0002-gles31-render-backend.md) — GLES 3.1 render backend
- [ADR-0003](decisions/ADR-0003-wxwidgets-toolkit.md) — Keep wxWidgets
- [ADR-0004](decisions/ADR-0004-gplv3.md) — GPLv3
- [ADR-0005](decisions/ADR-0005-gzdoom-udmf-first.md) — GZDoom UDMF-first
- [ADR-0006](decisions/ADR-0006-lua-sol2-plugins.md) — Lua/sol2 plugins
- [ADR-0007](decisions/ADR-0007-nodebuilders-toolchain.md) — AJBSP + ZDBSP + acc as tools
- [ADR-0008](decisions/ADR-0008-earcut-triangulation.md) — earcut.hpp sector triangulation

## Planning

- [implementation-plan](implementation-plan.md) — **detailed, referenceable plan for the next
  set of work**: advanced UDMF visual features (slopes, 3D floors, texture transforms, colors,
  sprites…), the interactive window + editing + save-back, the GLES/Pi backend, the wxWidgets
  shell, and packaging — with per-item design, tests, deps, and a recommended sequence
- [roadmap](roadmap.md) — phased delivery plan (Phase 0–4)
- [risks](risks.md) — living risk register

## The one fact that drives everything

The Raspberry Pi 5 GPU (VideoCore VII, Mesa V3D) caps **desktop OpenGL at 3.1**, but offers
**conformant GLES 3.1** and **conformant Vulkan 1.3**. So elads targets **GLES 3.1** as the
primary render path and confines all GL to one abstraction layer. Every design decision flows
from this. See [design/02](design/02-render-abstraction.md) and [design/09](design/09-rpi5-target.md).
