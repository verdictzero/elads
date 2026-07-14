# mapeditor — 2D + 3D map editor

Extends SLADE's map editor toward Ultimate Doom Builder parity. UDB is a **UX and
feature reference only** (its C# is not copied). See `docs/design/04-map-editor.md`.

- `model/` — geometry data model (vertices/linedefs/sidedefs/sectors/things), UDMF
  in/out, sector triangulation, slope planes, and save-back into archives.
- `edit/` — undoable editing operations + selection/picking (GUI/GL-free).
- `view2d/` — the 2D grid editor (extends SLADE `MapRenderer2D`) via `src/render`.
- `view3d/` — the 3D "visual mode" walkthrough (extends SLADE `MapRenderer3D`).
