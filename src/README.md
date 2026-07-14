# src/ — source tree skeleton

One directory per architecture module (see `docs/design/01-architecture.md`).

The **GUI/GL-free core** now has its first real, tested code (build with
`cmake --preset core && ctest --preset core`):

- `util/` — `geometry.h` (Vec2/BBox/segment distance), `byte_io.h` (LE reader/writer), `undo.h`
- `archive/` — `wad.{h,cpp}` (lightweight WAD read/write, byte-for-byte round-trip),
  `pk3.{h,cpp}` (PK3/zip read/write via miniz, folder→namespace), `entry_type.{h,cpp}`
  (ordered lump-type detection pipeline)
- `mapeditor/model/` — `map_objects.h`, `map_model.{h,cpp}` (geometry model + topology/hit-test),
  `doom_map_io.{h,cpp}` (classic Doom binary maps + map discovery), `udmf.{h,cpp}` (UDMF text, lossless),
  `sector_tri.{h,cpp}` (boundary tracing + earcut triangulation), `map_checks.{h,cpp}` (validation),
  `planes.{h,cpp}` (UDMF sloped floor/ceiling planes from slope things + point-in-sector)
- `graphics/` — `image.h` (RGBA8), `palette.{h,cpp}` (PLAYPAL + nearest-color),
  `doom_gfx.{h,cpp}` (Doom picture decode/encode), `flat.{h,cpp}` (flats), `texturex.{h,cpp}`
  (PNAMES + TEXTUREx), `composite.{h,cpp}` (assemble a texture from patches), `png.{h,cpp}`
  (PNG output via miniz), `material_set.h` (named RGBA textures/flats), `wad_materials.{h,cpp}`
  (build a MaterialSet from a WAD: PLAYPAL + patches + TEXTUREx + flats)
- `render/backend/` — `render_backend.h` (the `IRenderDevice`/`IRenderContext` interface)

**Desktop OpenGL variant** (`ELADS_GL`, needs EGL + libepoxy):

- `render/gl/` — `egl_headless.{h,cpp}` (surfaceless GL context), `gl_backend.{h,cpp}`
  (`GLDevice`/`GLContext` — desktop GL 3.3+ implementation of the abstraction),
  `offscreen.{h,cpp}` (FBO + readback)
- `mapeditor/view2d/` — `map_view_2d.{h,cpp}` (`MapRenderer2D`, backend-agnostic 2D map view)
- `mapeditor/view3d/` — `map_view_3d.{h,cpp}` (`MapRenderer3D` + `Camera3D`, **textured** 3D
  visual mode: UV-mapped walls from sector heights, earcut floors/ceilings, per-texture batching,
  perspective + depth; falls back to flat shading when a texture is missing)
- `util/mat4.h` — column-major 4×4 matrix math for the 3D camera
- `app/` — `cli_main.cpp` (the GL-free `elads` CLI), `render_main.cpp` (the `elads-render`
  headless map→PNG tool: `render-demo`/`render-map`/`render-demo3d`/`render-map3d`)

Vendored: `third_party/earcut/` (ISC) for triangulation; `third_party/miniz/` (public domain)
for PK3/zip + PNG.

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
