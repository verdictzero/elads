# ADR-0008 — Use earcut.hpp for sector triangulation

- **Status:** accepted
- **Date:** 2026-07

## Context

Rendering a sector's floor/ceiling fill (in both the 2D view and the 3D visual mode) requires
turning its boundary polygon — possibly with holes (islands) and messy real-world topology
(self-intersections, unclosed loops) — into triangles for the GLES backend. SLADE uses a
hand-rolled convex-decomposition tessellator. GLES 3.1 has no `GL_POLYGON`/fixed-function fill,
so triangulation is mandatory, not optional.

## Decision

Use **`earcut.hpp`** (mapbox), a header-only, ISC-licensed, hole-aware ear-clipping tessellator
that compiles cleanly on ARM64. Convention: outer ring CW, inner rings (holes) CCW. Cache the
resulting triangles in a **per-sector VBO** and re-triangulate only when that sector's geometry
changes. Keep **libtess2** as a fallback for pathological cases. Port SLADE's guard heuristics
(closure checks, degenerate-loop rejection) as validation before tessellation.

## Consequences

- A well-tested, simple tessellator replaces bespoke code; fewer correctness edge cases (risk R4).
- Header-only → trivial vendoring, no extra link deps.
- Cached VBOs keep 2D pan/zoom and 3D rendering cheap on the fill-rate-limited V3D GPU.

## Alternatives considered

- **SLADE's convex decomposition** — works but bespoke and harder to reason about on bad input.
  Replaced.
- **GLU tessellator (`gluTess`)** — legacy, GLU not available in a clean GLES/EGL setup. Rejected.
- **libtess2 as primary** — robust but heavier; kept as the fallback path.
