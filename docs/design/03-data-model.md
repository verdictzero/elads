# Data model — archives & maps

The two in-memory data structures every other module reads and writes: the namespaced
virtual filesystem (Archive/VFS) that holds resource containers, and the format-neutral map
geometry model. Both live in the `archive` module (the DATA CORE — no GL, no wx-UI; see
[architecture](01-architecture.md)). On-disk byte layouts are specified in
[formats-reference](08-formats-reference.md); this doc is about the *shapes in memory* and the
rules that populate them.

---

## 1. Scope and layering

```
disk container (WAD/PK3/PAK/GRP/RFF/dir)
        │  loader (open)                    │  writer (save)
        ▼                                   ▲
   ┌──────────────────────────────────────────────┐
   │  Archive  = tree of ArchiveEntry (the VFS)    │   archive/ (data core)
   └──────────────────────────────────────────────┘
        │  EntryType detection assigns each entry a type
        │  a map = a lump group  ─or─  TEXTMAP/ENDMAP pair
        ▼
   ┌──────────────────────────────────────────────┐
   │  MapModel  = Vertices/Lines/Sides/Sectors/    │   mapeditor/model/
   │  Things + selection + undo stack              │
   └──────────────────────────────────────────────┘
```

Two invariants make the rest of the app simple:

- **The data core has zero GL/UI dependencies.** An `Archive` and a `MapModel` can be built,
  edited, and serialized headless (used by CI thumbnails and the pipeline; see
  [build-test-pipeline](07-build-test-pipeline.md)).
- **One in-memory map model, many disk formats.** Doom, Hexen, and UDMF differ only at the
  (de)serialization boundary. The editor ([map-editor](04-map-editor.md)) never branches on
  format except when a feature is format-gated (e.g. per-line args).

This mirrors SLADE's split: `Archive`/`ArchiveEntry`/`EntryType` are reused largely intact
(SLADE `src/Archive/`), and the map model corresponds to SLADE's `SLADEMap` plus the
`MapObject` family (SLADE `src/SLADEMap/`), extended for the editor.

---

## 2. Archive / VFS

### 2.1 What an Archive is

An `Archive` is an in-memory, namespaced virtual filesystem loaded from one container file.
It is the elads analogue of GZDoom's `FResourceFile` (GZDoom `src/common/filesystem/`), but
it is **read-write and editable**, not just a read-only mount. Multiple archives are stacked
by the app into a resource list (base resource + open archives) with later-wins lookup
semantics, exactly like a Doom engine's load order.

Every container type is loaded behind one abstract interface so callers never care whether the
bytes came from a WAD or a zip:

```cpp
// illustrative — intent + likely shape, not SLADE's exact signature
class Archive {
public:
    virtual bool open(MemChunk& data) = 0;   // parse container → entry tree
    virtual bool write(MemChunk& out)  = 0;   // serialize tree → container bytes
    ArchiveDir*   rootDir();                  // hierarchical view
    ArchiveEntry* entryAtPath(string path);   // "textures/DOOR2" etc.
    // add / remove / move / rename entries, signalled to listeners
};
```

### 2.2 Container loaders

| Format | Ext | Structure on disk | Namespaces? | Notes |
|---|---|---|---|---|
| WAD | `.wad` | 12-byte header + flat lump directory | via `_START`/`_END` markers | IWAD vs PWAD tag; the native Doom format |
| PK3 / PKE | `.pk3` `.pke` `.zip` | ZIP (deflate/store) | via **folders** | GZDoom's preferred modern format; PKE = renamed zip |
| PAK | `.pak` | Quake pack; 64-byte-name flat dir | folder-path | idTech2 |
| GRP | `.grp` | Build-engine group; flat dir | none | Duke3D et al. |
| RFF | `.rff` | Blood resource file; **XOR-encrypted directory** | type field | decrypt dir on load |
| Directory | (folder) | live filesystem folder | via subfolders | edit-in-place; watch for external changes |

Each loader is a subclass (`WadArchive`, `ZipArchive`, `PakArchive`, `GrpArchive`,
`RffArchive`, `DirArchive`) selected by sniffing magic bytes then extension. This is a direct
reuse of SLADE's `src/Archive/Formats/`. Newly written containers go back out through the same
subclass's `write()`, so a WAD stays a WAD and a PK3 stays a zip.

### 2.3 Entry model

An `ArchiveEntry` is the leaf node — one named blob plus metadata:

```cpp
// illustrative
struct ArchiveEntry {
    string      name;        // "DOOR2", "MAP01", "TEXTMAP", "sky1.png"
    MemChunk    data;        // raw bytes (see MemChunk below)
    EntryType*  type;        // detected; see §3
    string      namespace_;  // "textures","flats","sprites","global","maps",...
    ArchiveDir* parent;      // hierarchy link
    bool        modified;    // dirty flag for save/undo
    // + user-set properties (e.g. explicit type override)
};
```

- **`MemChunk`** is SLADE's owning byte buffer (`src/Utility/MemChunk`): a `uint8_t*` + size
  with load/write/reserve helpers and endian-aware readers. It is the universal currency —
  loaders fill it, EntryType reads its head/size, editors mutate it, writers concatenate it.
- **Name** in WADs is the 8-char uppercase lump name; in zip/dir it is the real filename.
- **Namespace** is where the entry semantically lives (see §2.4). It is *not* the same as the
  folder path — it is normalized so a flat is a flat whether it arrived via a WAD `F_START`
  marker or a PK3 `flats/` folder.

### 2.4 Hierarchy and PK3 folder namespaces

WADs are flat: the tree is a single root directory of lumps, with **namespace markers**
(`P_START`/`P_END`, `F_START`/`F_END`, `S_START`/`S_END`, `TX_START`/`TX_END`, `PP_`, `FF_`,
`SS_` variants) delimiting ranges. The loader collapses the markers and stamps each lump in
range with a namespace.

PK3/dir are genuinely hierarchical, and GZDoom assigns namespace by **top-level folder name**
(GZDoom `src/common/filesystem/`; zdoom.org/wiki "Using ZIPs as WAD replacement"):

| Folder | Namespace | Holds |
|---|---|---|
| `textures/` | textures | full wall textures (PNG/etc., not composited) |
| `patches/` | patches | patch graphics referenced by TEXTUREx/TEXTURES |
| `flats/` | flats | floor/ceiling flats |
| `sprites/` | sprites | actor sprite frames |
| `graphics/` | graphics | UI/menu/HUD graphics |
| `sounds/` | sounds | sound effects |
| `music/` | music | music tracks |
| `acs/` | acs | compiled ACS (`.o`) and libraries |
| `maps/` | maps | one map per file (`MAP01.wad`-style embedded, or UDMF) |
| `voxels/`, `colormaps/`, `hires/`, ... | (as named) | additional GZDoom namespaces |
| (root / other) | global | everything else (DECORATE, ZScript, TEXTURES, MAPINFO...) |

Namespace matters because texture/flat/sprite lookups are namespace-scoped: a flat named
`FLOOR4_8` and a patch named `FLOOR4_8` can coexist. The map editor's texture browser and the
graphics editor ([graphics-texture-editor](06-graphics-texture-editor.md)) query by
(namespace, name).

---

## 3. Ordered entry-type detection pipeline

Every entry gets an `EntryType` (SLADE's data-driven type system: `src/Archive/EntryType` +
the `etypes/` definition files). Type drives icon, editor selection, and export extension.
Detection is an **ordered pipeline** — the first rule that matches wins, so cheap/authoritative
signals run before expensive/heuristic ones. Intent per SLADE's `EntryType::detectEntryType`.

```
for each entry:
  1. FOLDER NAMESPACE       ── if namespace fixes the type, use it.
     e.g. anything in flats/  → gfx flat; maps/ file → map marker
  2. MAGIC BYTES            ── match a known signature at a known offset:
        "PWAD"/"IWAD", 0x89 P N G, "DDS ", "OggS", "MThd"(MIDI),
        "MUS\x1a", "RIFF"/"WAVE", "PK\x03\x04", "ACS"/"ACSE"/"ACSe"...
  3. FLAT-SIZE HEURISTIC    ── raw, headerless gfx: if size matches a known
        flat dimension → flat.  64*64 = 4096 (the classic Doom flat),
        also 8*8=64, 16*16, 32*64, 128*128=16384, 256*256=65536,
        and 320*200=64000 (fullscreen graphics / TITLEPIC raw).
  4. DOOM PATCH VALIDATION  ── structural, not magic: read the patch header
        (width,height,left,top) then verify all `width` column offsets point
        inside the lump and each column's post chain terminates with 0xFF
        before EOF.  Passes ⇒ gfx patch (doom).  This rejects random blobs
        that merely have plausible leading uint16s.
  5. TEXT-LUMP KEYWORD      ── decode as text, scan the head for section
        keywords: "ACTOR"→DECORATE, "version"/"class"→ZScript, a map name +
        "gamedefaults"→MAPINFO, "namespace"→UDMF TEXTMAP, "//"+"TEXTURE"→
        TEXTURES, "[Enemies]"→SNDINFO-ish, etc.  Last because text is the
        most ambiguous signal.
  fallback: marker / unknown (still editable as a hex/raw entry).
```

Why this order:

- **Namespace first** is authoritative and free — a lump between `F_START`/`F_END` *is* a flat
  by definition, so no heuristic can override it.
- **Magic bytes** are near-authoritative and O(constant).
- **Flat-size** must come before patch validation because a 4096-byte lump could accidentally
  pass loose patch checks; the size rule is a stronger prior for known-square raw gfx.
- **Patch validation** is structural and moderately expensive (walk every column), so it sits
  below the size shortcut but above text.
- **Text keyword** is last: decoding + scanning is the fuzziest and most costly test, and many
  binary lumps contain ASCII runs that would false-positive if tried earlier.

The result is deterministic and stable across reopen — critical for undo/redo and for CI
snapshot tests of the type assignments.

---

## 4. Map geometry model

One format-neutral in-memory model. The five object classes below are the map. All heights,
coordinates, and angles are integers in Doom/Hexen and can be floating point in UDMF; the model
stores the widest representation (see §4.6) and narrows on save.

Each object carries a stable `index` (its slot in the array — this is the on-disk reference key
for Doom/Hexen binary formats) and a bag of extra properties for UDMF round-trip (§5).

### 4.1 Vertex

```cpp
// illustrative
struct Vertex {
    double x, y;          // map units; int on binary save
    // UDMF extras (zdoom): "zfloor","zceiling" for sloped 3D-floor vtx
    PropertyList props;   // unknown/extra keys preserved verbatim
};
```

### 4.2 Linedef

The connective tissue: two vertices, one or two sidedefs, flags, and an action.

```cpp
// illustrative
struct Linedef {
    int    v1, v2;                 // vertex indices
    int    side_front, side_back;  // sidedef indices; -1 = none (back → 1-sided)
    uint32 flags;                  // BLOCKING, TWOSIDED, DONTPEGTOP, SECRET, ...
    int    special;                // line action / linedef type
    int    tag;                    // Doom: sector tag activated
    int    args[5];                // Hexen/UDMF: special arguments (arg0 often = tag)
    // UDMF: "id" (line id), activation flags as booleans, "moreids", extras
    PropertyList props;
};
```

- **Doom** stores `flags`, `special`, `tag` — no args.
- **Hexen** replaces the 2-byte tag with a 1-byte special + **5 arg bytes** and adds
  activation bits in flags (SPAC_* semantics). The single-model equivalent: `special` + `args`
  are always present; the Doom loader leaves `args` zero and maps `tag`→`args[0]` for
  tag-consuming specials as needed.
- **UDMF** names every field (`v1`, `sidefront`, `arg0`..`arg4`, `id`, plus boolean flag keys).

### 4.3 Sidedef

One face of a linedef; references exactly one sector.

```cpp
// illustrative
struct Sidedef {
    int    sector;                 // sector index
    int    tex_offset_x, tex_offset_y;
    string tex_upper, tex_middle, tex_lower;  // "-" = no texture
    // UDMF extras: per-tex offsets (offsetx_top/mid/bottom),
    //   scale/light per surface, "skew", etc.
    PropertyList props;
};
```

### 4.4 Sector

The floor/ceiling volume.

```cpp
// illustrative
struct Sector {
    int    f_height, c_height;     // floor / ceiling plane heights
    string f_flat,  c_flat;        // floor / ceiling flat names
    int    light;                  // 0..255 sector light level
    int    special;                // damage/blink/secret/etc. sector type
    int    tag;                    // Doom tag; UDMF "id"
    // UDMF extras: slope planes, per-plane light, colormap/fade,
    //   gravity, "lightfloor/lightceiling", 3D-floor references...
    PropertyList props;
};
```

### 4.5 Thing

An actor/spawn placement.

```cpp
// illustrative
struct Thing {
    double x, y;                   // position
    int    angle;                  // facing, degrees (0..359, usually /45 in Doom)
    int    type;                   // editor/DoomEd number
    uint32 flags;                  // skill1-3, ambush, multiplayer, class bits...
    // Hexen/UDMF:
    double z;                      // spawn height above floor
    int    tid;                    // thing id (Hexen "id")
    int    special;                // action executed on activation
    int    args[5];                // special args
    PropertyList props;            // UDMF extras: pitch, roll, scale, health...
};
```

- **Doom** things: `x, y, angle, type, flags` (10 bytes) — no z, no tid, no special/args.
- **Hexen** things add `tid`, `z`, `special`, `args`, extra flag bits (class/dormant) — 20
  bytes.
- **UDMF** names all of the above and permits arbitrary extra keys.

### 4.6 One model, three serializations

```
             ┌─────────────── in-memory MapModel ───────────────┐
             │  Vertex/Linedef/Sidedef/Sector/Thing (superset)   │
             └───────────────────────────────────────────────────┘
   load ▲ narrow                                    widen ▼ save
  ┌──────────┐        ┌───────────┐        ┌───────────────────────┐
  │  Doom    │        │  Hexen    │        │  UDMF (TEXTMAP)       │
  │ binary   │        │  binary   │        │  text key/value       │
  │ lumps    │        │  lumps    │        │  namespace zdoom/...  │
  └──────────┘        └───────────┘        └───────────────────────┘
```

The map *config* (which format, which game) decides the writer. The editor works on the
superset; fields a format cannot represent are either dropped on save (with a warning) or, for
UDMF, preserved as `props` (§5). Format capability is queried, never hard-coded per call site —
e.g. `mapFormat().supportsArgs()`.

---

## 5. UDMF representation and lossless round-trip

UDMF (Universal Doom Map Format; zdoom.org/wiki "UDMF") is text. A map is a single `TEXTMAP`
lump between the `MAP01` marker and `ENDMAP` (§6), shaped as:

```
namespace = "zdoom";        // or "gzdoom", "hexen", "doom", "eternity"...
vertex { x = 128.0; y = -64.0; }
linedef { v1 = 0; v2 = 1; sidefront = 0; blocking = true; special = 80; arg0 = 3; }
sidedef { sector = 0; texturemiddle = "STARTAN2"; }
sector { heightfloor = 0; heightceiling = 128; texturefloor = "FLAT1";
         textureceiling = "F_SKY1"; lightlevel = 160; }
thing  { x = 96.0; y = 96.0; angle = 90; type = 1; skill1 = true; }
```

Parsing/writing rules the model must honor:

1. **Namespace is authoritative.** `namespace` selects the dialect (which keys/specials are
   legal). elads primarily targets `zdoom` / `gzdoom`. The namespace string is stored on the
   `MapModel` and written back verbatim.
2. **Lossless round-trip, including unknown keys.** Any key elads does not model is retained on
   the object's `PropertyList` (`props`) with its original type (int/float/bool/string) and
   re-emitted on save. Opening and re-saving a UDMF map from a newer GZDoom must not silently
   drop future keys. This is the central UDMF requirement — treat the parser as *extensible by
   default*, only special-casing the keys the editor actively edits.
3. **Type fidelity.** `128` vs `128.0` vs `"128"` are distinct token types; the writer emits
   the type it read for untouched keys and a canonical form for edited ones.
4. **Ordering / formatting** is not semantically load-bearing, but elads emits a stable,
   diff-friendly order (blocks in index order, keys in a fixed canonical order) so version
   control diffs stay small. Comments are not guaranteed to survive (UDMF permits them but they
   are not attached to objects).

Practically: the UDMF codec is a tokenizer + a per-block key dispatcher. Known keys hydrate
struct fields; the default branch stashes into `props`. On write, struct fields are emitted
first, then any leftover `props` keys not shadowed by a struct field. This is how SLADE's UDMF
parser (`src/SLADEMap/MapFormat/`) achieves round-trip and what elads reuses.

---

## 6. Linking a map to its archive

A map is not a first-class file — it is a **region of an archive**. Two encodings exist and the
model must recognize both:

| Encoding | Layout | Used by |
|---|---|---|
| **Lump group** | a name marker lump (`MAP01`/`E1M1`) immediately followed, in order, by `THINGS`, `LINEDEFS`, `SIDEDEFS`, `VERTEXES`, `SEGS`, `SSECTORS`, `NODES`, `SECTORS`, `REJECT`, `BLOCKMAP` (+ Hexen `BEHAVIOR`, optional `SCRIPTS`) | Doom & Hexen binary maps |
| **TEXTMAP/ENDMAP** | name marker, then a single `TEXTMAP` text lump, optional `ZNODES`/`BEHAVIOR`/`SCRIPTS`/etc., then an `ENDMAP` marker | UDMF maps |

Detection (`MapDesc`-style scan of the archive's entry list, per SLADE
`Archive::detectMaps`):

```
walk entries in order:
  if entry is a valid map-name marker (MAPxx / ExMy / arbitrary in PK3 maps/):
      peek next entry:
        "TEXTMAP"  → UDMF map; span = marker .. "ENDMAP"        → format = UDMF
        "THINGS"   → binary map; span = marker .. last known map-lump
                       has "BEHAVIOR"? → Hexen  else → Doom
      record MapDesc{ name, format, archive, first_entry, last_entry }
```

- In a **PK3**, a map under `maps/MAP01.wad` (or `maps/MAP01` UDMF text) is a *nested* archive
  or a single file; the same detector runs on the nested entry list.
- The `MapModel` keeps a back-reference to its source `MapDesc` (archive + entry span) so
  **save** knows exactly which lumps to overwrite/replace in place. Node lumps (`SEGS`..
  `BLOCKMAP`, `ZNODES`) are treated as build artifacts, regenerated by the node builder on
  save/playtest (AJBSP/ZDBSP; see [build-test-pipeline](07-build-test-pipeline.md)), not
  hand-edited.

Byte-level lump layouts for every one of these are in
[formats-reference](08-formats-reference.md).

---

## 7. Selection sets

The editor operates on **selections** of map objects. A selection is per-object-type and
stored on the editing context, not on the objects themselves (objects stay pure data):

```cpp
// illustrative
struct Selection {
    MapObjectType type;        // VERTEX | LINE | SIDE | SECTOR | THING
    std::vector<int> indices;  // selected object indices, order = pick order
    // helpers: toggle(i), add(i), clear(), forEach(fn), isSelected(i)
};
```

- A transient **hilight** (the object under the cursor) is tracked separately from the
  committed selection.
- Selection is **derived state**: it never goes on the undo stack directly, but a command that
  needs it (e.g. "drag selected vertices") snapshots the affected indices so undo restores the
  right objects even if selection later changes.
- Cross-type derivations are computed on demand (e.g. "sectors touched by selected lines")
  rather than stored, keeping the selection minimal and consistent after edits/deletes.
- Deleting objects renumbers indices; the selection layer remaps or drops stale indices so it
  never points past the array. (This is why references are by index and mutations funnel
  through the model, not raw array pokes.)

---

## 8. Undo / redo — command pattern

All mutations go through a **command** object so they are reversible and composable. This is
SLADE's `UndoManager` / `UndoStep` pattern (`src/General/UndoRedo`), extended for map edits.

```cpp
// illustrative
class UndoStep {
public:
    virtual bool doStep()   = 0;   // apply (redo)
    virtual bool undoStep() = 0;   // revert
    virtual bool isOk() const = 0;
};

class UndoManager {
    void beginRecord(string name);          // open a transaction
    void recordStep(unique_ptr<UndoStep>);  // add to current transaction
    void endRecord(bool commit);            // push onto undo stack (or discard)
    bool undo();                            // pop → undoStep()
    bool redo();                            // → doStep()
};
```

Design points:

- **Transactions group primitives.** A single user gesture ("insert sector", "join lines")
  records many primitive steps (create vertices, create lines, set sectors) under one named
  transaction so one Ctrl-Z reverses the whole gesture.
- **Property-diff steps** for map objects: a step captures the object's changed properties
  before and after (a small delta), not a full map copy — cheap even on large maps. Structural
  steps (create/delete object) capture enough to reconstruct the object and its references.
- **Undo also covers non-map edits**: archive-level operations (add/rename/delete entry, edit
  bytes in the hex/gfx editor) push their own steps, so Ctrl-Z is uniform across the app.
- **Save does not clear undo**, but it marks a "clean" point so the title bar dirty flag is
  accurate.
- The stack is bounded (configurable depth) to cap memory on the Pi's 8 GB.

---

## 9. Consistency and cross-references

- **Referential integrity is index-based and validated.** Linedefs reference vertex and
  sidedef indices; sidedefs reference sector indices. A validation/"map check" pass
  ([map-editor](04-map-editor.md)) reports dangling refs, unclosed sectors, and orphaned
  vertices; the model does not silently auto-repair on load (it preserves what was there for a
  faithful round-trip).
- **Textures/flats are name references, not owned data.** They resolve against the stacked
  archive resource list (§2.1) by (namespace, name) at render/browse time; a missing name is a
  soft error (rendered as a "no texture" placeholder), never a load failure.
- **The map model never links directly to GL.** The renderer builds its own GPU-side buffers
  from the model each frame/dirty-region; see [render-abstraction](02-render-abstraction.md).

### Related docs

- [architecture](01-architecture.md) — where `archive/` and `mapeditor/model/` sit in the module graph.
- [map-editor](04-map-editor.md) — how the editor consumes selection/undo and the map model.
- [formats-reference](08-formats-reference.md) — exact on-disk byte layouts referenced throughout.
- [graphics-texture-editor](06-graphics-texture-editor.md) — namespace-scoped texture/flat lookup.
- [build-test-pipeline](07-build-test-pipeline.md) — node rebuild on save, headless model use.
