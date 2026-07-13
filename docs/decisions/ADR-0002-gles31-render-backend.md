# ADR-0002 — Target OpenGL ES 3.1 via a render-abstraction layer

- **Status:** accepted
- **Date:** 2026-07

## Context

The Raspberry Pi 5's VideoCore VII GPU (Mesa V3D 7.x) exposes, natively:

| API | Level |
|-----|-------|
| Desktop GL core | **3.1** (GLSL 1.40) — no 3.2/3.3/4.x, ever |
| Desktop GL compat | 2.1 (legacy immediate-mode cap) |
| **GLES 3.1** (GLSL ES 3.10) | conformant — has compute, SSBO, explicit `layout(location=)` |
| Vulkan (V3DV) | 1.3 conformant |

SLADE `master` requires desktop **GL 3.3** and its map editor has **no software fallback**;
on the Pi it would fall to `llvmpipe` (slow). This is the project's biggest technical risk (R1).

## Decision

Introduce a thin **Render Abstraction Layer** (`src/render`) — device/context/viewport/shader/
buffer/texture — behind which all rendering happens. The **primary backend is OpenGL ES 3.1**
(`#version 310 es`), the only conformant hardware-accelerated path on V3D. Provide **desktop
GL 3.1** and **Zink-over-Vulkan** backends behind the same interface as fallbacks for other
desktops. No module may call GL directly or depend on GL 3.3+ features.

All legacy GL idioms inherited from SLADE (`glBegin`/`GL_QUADS`, display lists, fixed-function
fog, `GL_POINT_SPRITE`, double-precision verts) are **lowered** in the backend to shader
triangles / `gl_PointCoord` / UBO fog / floats. GZDoom's `src/common/rendering/gles` is the
concrete lowering reference. Contexts are created via **EGL** (no GLX under Wayland).

## Consequences

- The renderer is the core new investment; everything visual routes through it.
- Shaders are authored once as GLSL ES 3.10; no geometry/tessellation stages (absent on V3D).
- Portability to other GPUs comes "for free" through the abstraction.
- Extra indirection vs calling GL directly — acceptable for the portability guarantee.

## Alternatives considered

- **Fork SLADE 3.2.x** (legacy immediate-mode GL, runs under compat 2.1 on the Pi) — rejected as
  the base but informs fallback behaviour; immediate mode is a dead end for the 3D visual mode.
- **Vulkan-only** — rejected: far more code for an editor; Zink gives Vulkan's reach when needed.
- **Depend on `llvmpipe`** — rejected: too slow for a 3D map view on a Pi-class CPU.
