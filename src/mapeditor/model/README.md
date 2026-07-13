# mapeditor/model — map geometry data model

The editable map: vertices, linedefs, sidedefs, sectors, things, plus UDMF
key/value round-tripping (namespace `zdoom`/`gzdoom`). Owns selection sets and the
undo/redo stack. Triangulates sectors via `earcut.hpp` (cached per-sector VBOs,
re-triangulated only on edit). Independent of any rendering backend.
