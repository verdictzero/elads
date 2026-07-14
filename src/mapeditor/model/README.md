# mapeditor/model — map geometry data model

The editable map: vertices, linedefs, sidedefs, sectors, things, plus UDMF
key/value round-tripping (namespace `zdoom`/`gzdoom`). Triangulates sectors via
`earcut.hpp`; derives floor/ceiling **slope planes** (`planes.{h,cpp}`) from slope
things and `Plane_Align`; validates geometry; and **saves** an edited model back into
a WAD (`map_save.{h,cpp}`). Selection/picking and undoable edits live in `../edit`;
undo/redo itself is `util::UndoManager`. Independent of any rendering backend.
