# mapeditor/edit — editing operations, selection & picking

Pure, headless-testable editor logic over `mapeditor/model`, with no rendering
dependency. See `docs/design/04-map-editor.md` and the B-track items in
`docs/implementation-plan.md`.

- `selection.{h,cpp}` — `Selection` (one object of a kind) and `pick()`, which
  resolves a world-space point to the object under it (vertex → thing → linedef →
  enclosing sector). Screen→world unprojection lives with the cameras
  (`view2d::screenToWorld`, `view3d::screenRay`); this layer stays pure model math.
- `map_edit.{h,cpp}` — undoable operations recorded through `util::UndoManager` as
  `{apply, revert}` pairs: move vertices, set sector/sidedef properties, flip and
  split linedefs, add/delete things, and create a sector from a loop. Structural ops
  append and undo by truncation (strict LIFO).
