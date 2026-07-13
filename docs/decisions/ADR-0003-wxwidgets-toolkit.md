# ADR-0003 — Keep wxWidgets as the GUI toolkit

- **Status:** accepted
- **Date:** 2026-07

## Context

Forking SLADE (ADR-0001) means inheriting its toolkit. SLADE uses **wxWidgets**: wxAUI for
docking, `wxStyledTextCtrl` (Scintilla) for the code editor, `wxGLCanvas` for GL viewports,
plus propgrid/richtext. Switching to Qt6 would mean rewriting the entire UI and the GL viewport
hosting, discarding most of SLADE's reusable value.

Relevant fact: **wxWidgets 3.2.9** (Dec 2025) added an **EGL 1.4** path and HiDPI-under-Wayland
fixes to `wxGLCanvas` — exactly what the Pi 5's labwc/Wayland session needs. Raspberry Pi OS
ships wx **3.2.x** via apt; SLADE `master` wants wx ≥ 3.3.1 (a dev series not packaged).

## Decision

**Keep wxWidgets**, targeting **system wx 3.2.9+** on the Pi (`BUILD_WX=OFF`). Use the
`wxGLCanvas` **EGL** path for on-screen contexts and an EGL/GBM offscreen context for headless
render (CI, thumbnails). Backport only the specific wx 3.3 APIs elads genuinely needs rather
than building wx from source on-device.

## Consequences

- ~80% of SLADE's UI/editor code is preserved.
- We must reconcile wxAUI's idle-driven docking with an always-rendering GL canvas (risk R5) —
  SLADE already solves a version of this; we adopt its explicit idle handling.
- We are constrained to wx 3.2 APIs on the Pi until 3.3 is packaged; manageable.

## Alternatives considered

- **Qt6 + Qt-Advanced-Docking-System** — better IDE-grade docking/HiDPI, but a full UI rewrite and
  loss of SLADE reuse. Rejected.
- **Dear ImGui** — great for tools/viewports, weak for rich native panels/dialogs; would still
  need a windowing layer. Rejected as the shell.
- **FLTK** (Eureka's toolkit) — lightweight and low-GL, but a downgrade in widget richness and no
  SLADE reuse. Rejected; kept as a reference.
