# src/ — source tree skeleton

One directory per architecture module (see `docs/design/01-architecture.md`).

The **GUI/GL-free core** now has its first real, tested code (build with
`cmake --preset core && ctest --preset core`):

- `util/` — `geometry.h` (Vec2/BBox/segment distance), `byte_io.h` (LE reader/writer), `undo.h`
- `archive/` — `wad.{h,cpp}` (lightweight WAD read/write, byte-for-byte round-trip),
  `entry_type.{h,cpp}` (ordered lump-type detection pipeline)
- `mapeditor/model/` — `map_objects.h`, `map_model.{h,cpp}` (geometry model + topology/hit-test),
  `doom_map_io.{h,cpp}` (classic Doom binary maps + map discovery), `udmf.{h,cpp}` (UDMF text, lossless),
  `sector_tri.{h,cpp}` (boundary tracing + earcut triangulation)
- `graphics/` — `image.h` (RGBA8), `palette.{h,cpp}` (PLAYPAL + nearest-color),
  `doom_gfx.{h,cpp}` (Doom picture decode/encode), `flat.{h,cpp}` (flats), `texturex.{h,cpp}`
  (PNAMES + TEXTUREx), `composite.{h,cpp}` (assemble a texture from patches)
- `render/backend/` — `render_backend.h` (the `IRenderDevice`/`IRenderContext` interface)
- `app/` — `cli_main.cpp` (the `elads` CLI: `wad-info`, `lump-types`, `map-info`, `demo-wad`)

Vendored: `third_party/earcut/earcut.hpp` (ISC) for sector triangulation.

The remaining directories hold a `README.md` describing their responsibility and the SLADE
source they will reuse or extend; the GUI/OpenGL implementations land in Phase 1.

| Dir | Module | Reuse basis |
|-----|--------|-------------|
| `app/` | Application entry, config, session | SLADE `Application` |
| `archive/` | Archive / data core (WAD/PK3/…) | SLADE `Archive`, `Formats`, `EntryType` (verbatim) |
| `render/` | **Render Abstraction Layer** (GLES 3.1 primary) | NEW — the core investment |
| `mapeditor/` | 2D + 3D map editor | extend SLADE `MapEditor` + `MapRenderer2D/3D`, UDB as UX ref |
| `texteditor/` | Scintilla code editor | SLADE `TextEditor` |
| `graphics/` | Graphics & texture editor | SLADE `Graphics` / `SIFormat` |
| `pipeline/` | Node build + ACS compile + playtest | SLADE nodebuilder integration + new |
| `scripting/` | Lua/sol2 scripting + plugins | SLADE `Scripting` |
| `ui/` | wxAUI docking shell | SLADE `UI` |
| `util/` | Shared utilities | SLADE `Utility` |
