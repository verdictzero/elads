# ADR-0001 — Fork & extend SLADE3

- **Status:** accepted
- **Date:** 2026-07

## Context

We need one native environment combining SLADE3's resource/graphics/code editing with
Ultimate Doom Builder's 2D+3D map editing, running hardware-accelerated on a Raspberry Pi 5
(aarch64). Three strategies were considered: (A) build from scratch, (B) fork/extend SLADE3,
(C) bundle existing tools.

Findings:
- **SLADE3** is C++17/wxWidgets/OpenGL, builds on aarch64, and already ships ~80% of the
  surface: WAD/PK3 archives, graphics/texture editor, a Scintilla editor with data-driven
  ZScript/DECORATE/ACS lexers, a Lua/sol2 engine, and a UDMF-capable 2D+3D map editor.
- **UDB** is C#/.NET Framework on Mono (slow, dev-unsupported on Linux) and needs desktop
  **OpenGL 3.2 core** — above the Pi 5's native GL 3.1 ceiling; ARM64/Mono is untested. It is
  a poor runtime target on the Pi.
- Building from scratch would re-implement SLADE's mature, battle-tested subsystems for no gain.

## Decision

**Fork and extend SLADE3.** Reuse its Archive/format/text/graphics/scripting layers largely
unchanged; replace exactly one subsystem — the OpenGL renderer — with a Pi-appropriate backend
(see ADR-0002); extend its map editor toward UDB parity, using **UDB as a UX/feature reference
only** (no C# is copied).

## Consequences

- Massive reuse; the hard, novel work is confined to the render abstraction and the map editor.
- The project inherits SLADE's license: GPLv2-**or-later**, so the combined work is GPLv3 (ADR-0004).
- We track SLADE upstream where practical, but diverge on rendering. Merges from upstream will
  need care around the render paths.
- UDB's map-editing UX must be re-implemented in C++, not ported.

## Alternatives considered

- **From scratch** — rejected: years of duplicated effort before parity.
- **Bundle SLADE + Eureka + tools** — rejected as the product: two toolkits, no unified app.
  Eureka remains a useful reference for lightweight, low-GL map-editor techniques.
