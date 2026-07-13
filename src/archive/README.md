# archive — archive / data core

Reused **~verbatim** from SLADE `src/Archive` (+ `Formats`, `EntryType`). Provides
the in-memory, namespaced virtual filesystem that everything else reads:

- Loaders/writers: WAD, PK3/PKE (zip), PAK, GRP, RFF, and directory-as-archive.
- `MemChunk` entry model; hierarchical entries; namespace tagging
  (`textures/ patches/ flats/ sprites/ sounds/ music/ graphics/ …`).
- Ordered **entry-type detection** (folder → magic → flat-size → Doom-patch
  structure → text match).

This is the most self-contained, architecture-neutral subsystem — no GL, no UI.
See `docs/design/03-data-model.md`.
