# src/ — source tree skeleton

One directory per architecture module (see `docs/design/01-architecture.md`).
**No feature code exists yet** — each directory currently holds a `README.md`
describing its responsibility and the SLADE source it will reuse or extend.

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
