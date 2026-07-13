# Doom/ZDoom formats reference

The implementer's cheat-sheet: exact on-disk byte layouts for every WAD/PK3 resource elads
must read and write. This underpins the in-memory shapes in [data-model](03-data-model.md) and
the pixel/palette handling in [graphics-texture-editor](06-graphics-texture-editor.md). Where a
layout is non-obvious it is cited to the [Doom Wiki](https://doomwiki.org/) or the
[ZDoom Wiki](https://zdoom.org/wiki/). All multi-byte integers are **little-endian**; Doom was
a DOS/x86 program and every engine since preserves that. On the aarch64 Pi 5 target every
read/write must therefore byte-swap explicitly on big-endian hosts — but aarch64 runs
little-endian, so in practice the structs map 1:1 (see [rpi5-target](09-rpi5-target.md)).

Type key used in the tables: `u8/u16/u32/i16/i32` = unsigned/signed little-endian ints of that
bit width; `char[n]` = fixed field, NUL-padded, **not** guaranteed NUL-terminated.

---

## 1. WAD container

A WAD ("Where's All the Data") is: a 12-byte header, then lump payloads, then a directory. The
directory can physically sit before or after the lumps — always seek via `infotableofs`.

### 1.1 Header (12 bytes)

| Off | Type    | Field          | Notes |
|-----|---------|----------------|-------|
| 0   | char[4] | `identification` | `"IWAD"` or `"PWAD"` magic. IWAD = standalone game; PWAD = patch/add-on. GZDoom treats both nearly identically; the flag mainly affects load order and a few legacy heuristics. |
| 4   | i32     | `numlumps`     | Number of directory entries. |
| 8   | i32     | `infotableofs` | Absolute byte offset of the directory. |

### 1.2 Directory entry (16 bytes each, `numlumps` of them)

| Off | Type    | Field     | Notes |
|-----|---------|-----------|-------|
| 0   | i32     | `filepos` | Absolute offset of lump data. 0 length ⇒ `filepos` may point anywhere / be a marker. |
| 4   | i32     | `size`    | Lump size in bytes. |
| 8   | char[8] | `name`    | Uppercase ASCII, NUL-padded to 8. **Max 8 chars, no path.** Names are case-insensitive on lookup but stored uppercase. Duplicates are legal (last-wins on lookup, but map/namespace context matters). |

```
+0            +4            +8                    +16
| filepos i32 | size    i32 | name char[8]         |
```

### 1.3 Lump ordering, markers, namespaces

WAD has no folders. Ordering is meaningful, and **zero-length marker lumps** delimit namespaces:

| Namespace | Start/End markers | Contents |
|-----------|-------------------|----------|
| Sprites   | `S_START` / `S_END` (also `SS_START/SS_END`) | Sprite graphics (Doom picture format). |
| Flats     | `F_START` / `F_END` (also `FF_START/FF_END`) | 64×64 raw flats; `F1_START/F1_END`, `F2_*`, `F3_*` sub-blocks in IWADs. |
| Patches   | `P_START` / `P_END` (`PP_*`, `P1_*`, `P2_*`, `P3_*`) | Wall patches referenced by TEXTUREx/PNAMES. |
| Colormaps | `C_START` / `C_END` | Boom custom colormaps. |
| ACS       | `A_START` / `A_END` | Compiled ACS behavior (rarely used vs. `BEHAVIOR`). |
| TX (textures) | `TX_START` / `TX_END` | ZDoom stand-alone textures (any image format), each usable as a texture by lump name. |
| Voxels    | `VX_START` / `VX_END` | KVX voxel models. |
| Hi-res    | `HI_START` / `HI_END` | Legacy hires texture replacements. |
| Global    | *(none)*          | Everything else: maps, `PLAYPAL`, `TEXTURE1`, `PNAMES`, sounds, music, ZScript, etc. |

Notes for the loader ([data-model §2](03-data-model.md)):
- Doubled markers (`SS_START`) exist because some tools stripped single-letter ones. Treat
  `S_START` and `SS_START` as equivalent open/close. Nesting is not real — a second START just
  continues the namespace.
- Map data is **not** namespaced by markers; it is a *contiguous run* of specially-named lumps
  following a map-name marker lump (§3).

### 1.4 Map lump group (classic + Hexen)

A map is a header lump whose name is the map name (`E1M1`, `MAP01`, or any name in UDMF), then a
fixed sequence of child lumps until a lump appears that is not a recognized map lump:

```
MAP01            (0-length header / marker)
  THINGS
  LINEDEFS
  SIDEDEFS
  VERTEXES
  SEGS
  SSECTORS
  NODES
  SECTORS
  REJECT
  BLOCKMAP
  BEHAVIOR       (Hexen/ZDoom only — compiled ACS; its PRESENCE selects the Hexen map format)
  SCRIPTS        (optional — ACS source, informational)
```

The presence of `BEHAVIOR` immediately after the map is the on-disk signal that LINEDEFS/THINGS
use the **Hexen extended** record layout (§4), not the Doom layout (§3). UDMF maps replace this
whole group with `TEXTMAP`…`ENDMAP` (§5).

---

## 2. PK3 / PKE / PK7 (zip) and folder namespaces

PK3/PKE are ordinary ZIP archives (PK7 = 7z); GZDoom and SLADE read them as first-class
resource archives. Instead of marker lumps, the **top-level folder** carries the namespace, and
files keep their extension. elads' `archive` module maps folders → the same namespaces as §1.3.

| Folder      | WAD-namespace equivalent | Notes |
|-------------|--------------------------|-------|
| `sprites/`  | Sprites (`S_`)           | One sprite frame per file (`TROOA1.png`). |
| `flats/`    | Flats (`F_`)             | Any image format, not just 64×64 raw. |
| `patches/`  | Patches (`P_`)           | For TEXTUREx composition. |
| `textures/` | TX (`TX_`)               | Stand-alone textures, referenced by base filename. |
| `graphics/` | Global misc graphics     | UI/menu/status graphics (`TITLEPIC`, fonts…). |
| `colormaps/`| Colormaps (`C_`)         | |
| `acs/`      | ACS (`A_`)               | Compiled `.o` behavior libraries. |
| `voxels/`   | Voxels (`VX_`)           | KVX. |
| `sounds/`   | Global (sound)           | |
| `music/`    | Global (music)           | |
| `maps/`     | (each file is a map)     | Each `maps/MAP01.wad` is an **embedded WAD** holding one map lump group (or UDMF). |
| *(root)*    | Global                   | Lump-name resources: `DECORATE`, `ZSCRIPT`, `MAPINFO`, `PLAYPAL`, `TEXTURES`, etc. |

Key differences from WAD to keep the model uniform (see [data-model](03-data-model.md)):
- Names may exceed 8 chars and have extensions; lookup usually strips the extension and folder.
- `maps/` contains one WAD-wrapped map per file rather than a flat run of lumps.
- Reference: [ZDoom Wiki: Using ZIPs as WAD replacement](https://zdoom.org/wiki/Using_ZIPs_as_WAD_replacement).

### 2.1 Other native containers (PAK / GRP / RFF)

elads' `archive` module (reusing SLADE's per-format classes; see [data-model](03-data-model.md)
§2.2) also reads these. Layouts below are summaries — verify field widths against SLADE's
loaders before implementing.

**PAK** (Quake/`.pak`) — 12-byte header then a directory:

```
header:  char magic[4] = "PACK";  int32 dirOffset;  int32 dirLength   // little-endian
entry (64 bytes): char name[56] (NUL-padded path);  int32 fileOffset;  int32 fileSize
count = dirLength / 64
```

**GRP** (Build / Duke Nukem 3D / `.grp`) — 16-byte header, then `numFiles` entries, then data
laid out contiguously in entry order (offsets are computed by accumulation):

```
header:  char magic[12] = "KenSilverman";  int32 numFiles
entry (16 bytes): char name[12] (NUL-padded);  int32 size
```

**RFF** (Blood / `.rff`) — signature `"RFF\x1A"`, a header with **version**, plus the directory
**offset and count**; the directory sits elsewhere in the file and its entries (≈48 bytes each:
offset, size, name 8+3, flags, time) are **XOR-encrypted** with a key derived from the entry
offset. This is the one non-trivial container — treat SLADE's `RffArchive` as authoritative for
the exact decrypt/field layout.

---

## 3. Classic Doom map lumps (byte layouts)

All records are fixed-size arrays; `count = lump.size / recordSize`.

### 3.1 VERTEXES — 4 bytes/record

| Off | Type | Field |
|-----|------|-------|
| 0   | i16  | x |
| 2   | i16  | y |

Map units are integers; UDMF (§5) lifts this to floating point.

### 3.2 THINGS (Doom) — 10 bytes/record

| Off | Type | Field | Notes |
|-----|------|-------|-------|
| 0   | i16  | x | |
| 2   | i16  | y | |
| 4   | i16  | angle | Degrees (0=E, 90=N…). |
| 6   | u16  | type  | Editor/DoomEd number. |
| 8   | u16  | flags | See §3.2.1. |

#### 3.2.1 Thing flags (Doom)

| Bit | Mask   | Meaning |
|-----|--------|---------|
| 0   | 0x0001 | On skill 1 & 2 (easy). |
| 1   | 0x0002 | On skill 3 (medium). |
| 2   | 0x0004 | On skill 4 & 5 (hard). |
| 3   | 0x0008 | Ambush / deaf (waits for sight, not sound). |
| 4   | 0x0010 | Multiplayer only (not in single-player). |
| 5   | 0x0020 | (Boom) Not in deathmatch. |
| 6   | 0x0040 | (Boom) Not in co-op. |
| 7   | 0x0080 | (MBF) Friendly monster. |

### 3.3 LINEDEFS (Doom) — 14 bytes/record

| Off | Type | Field | Notes |
|-----|------|-------|-------|
| 0   | i16  | startVertex | Index into VERTEXES. |
| 2   | i16  | endVertex   | |
| 4   | u16  | flags       | §3.3.1. |
| 6   | u16  | special     | Line action/type (linedef "special"). |
| 8   | u16  | tag / sectorTag | Tags target sectors. |
| 10  | i16  | rightSidedef | Front side, index into SIDEDEFS. |
| 12  | i16  | leftSidedef  | Back side; `0xFFFF` (−1) = no back side (one-sided). |

#### 3.3.1 Linedef flags (Doom/Boom)

| Mask   | Name | Meaning |
|--------|------|---------|
| 0x0001 | BLOCKING | Impassable. |
| 0x0002 | BLOCKMONSTERS | Blocks monsters only. |
| 0x0004 | TWOSIDED | Two-sided (both sidedefs valid). |
| 0x0008 | DONTPEGTOP | Upper texture unpegged. |
| 0x0010 | DONTPEGBOTTOM | Lower texture unpegged. |
| 0x0020 | SECRET | Shows as one-sided on automap. |
| 0x0040 | SOUNDBLOCK | Blocks sound propagation. |
| 0x0080 | DONTDRAW | Never shown on automap. |
| 0x0100 | MAPPED | Always shown on automap. |
| 0x0200 | (Boom) PASSUSE | Use action passes through to next line. |

### 3.4 SIDEDEFS — 30 bytes/record

| Off | Type    | Field | Notes |
|-----|---------|-------|-------|
| 0   | i16     | xOffset | Texture X offset. |
| 2   | i16     | yOffset | Texture Y offset. |
| 4   | char[8] | upperTex | Name or `"-"` (none). |
| 12  | char[8] | lowerTex | |
| 20  | char[8] | midTex   | |
| 28  | i16     | sector   | Index into SECTORS. |

### 3.5 SECTORS — 26 bytes/record

| Off | Type    | Field | Notes |
|-----|---------|-------|-------|
| 0   | i16     | floorHeight   | |
| 2   | i16     | ceilingHeight | |
| 4   | char[8] | floorFlat     | Flat name. |
| 12  | char[8] | ceilingFlat   | |
| 20  | i16     | lightLevel    | 0–255. |
| 22  | u16     | special       | Sector effect (damage, blink, secret…). |
| 24  | u16     | tag           | Matched by linedef tag. |

### 3.6 Derived / build lumps (produced by the node builder)

elads regenerates these via embedded AJBSP / external ZDBSP on save; see
[build-test-pipeline](07-build-test-pipeline.md). The editor rarely reads them, but the loader
must skip them correctly.

| Lump | Record | Layout |
|------|--------|--------|
| SEGS | 12 B | i16 v1, i16 v2, i16 angle, i16 linedef, i16 side(0=front/1=back), i16 offset. |
| SSECTORS | 4 B | i16 segCount, i16 firstSeg. A subsector = convex region. |
| NODES | 28 B | i16 x, y, dx, dy (partition line); then two bounding boxes `i16[4]` (top,bottom,left,right) for right & left child; then u16 rightChild, u16 leftChild. High bit (0x8000) set ⇒ child is a subsector index, else a node index. |
| REJECT | bit matrix | `ceil(numSectors² / 8)` bytes. Bit (i·numSectors + j) set ⇒ monsters in sector i cannot see sector j (LOS/wakeup optimization). All-zero is valid. |
| BLOCKMAP | see below | Collision acceleration grid. |

**BLOCKMAP** header: i16 originX, i16 originY, u16 columns, u16 rows. Then `columns×rows` u16
offsets (in words) to per-block linedef lists. Each list: a leading `0x0000`, then u16 linedef
indices, terminated by `0xFFFF`. Blocks are 128×128 map units.

Extended/compressed node formats (`XNOD`/`XGLN`/`ZNOD`/`ZGLN`, and GL nodes `GL_*`) exist for
huge maps and are what ZDBSP/GZDoom emit; see [ZDoom Wiki: Node](https://zdoom.org/wiki/Node) and
AJBSP docs. Detect by the 4-byte magic at the start of the NODES/SSECTORS lump.

---

## 4. Hexen / ZDoom extended map format

Selected on-disk by the presence of a `BEHAVIOR` lump in the map group (§1.4). Only THINGS and
LINEDEFS change; VERTEXES/SIDEDEFS/SECTORS are identical to §3.

### 4.1 THINGS (Hexen) — 20 bytes/record

| Off | Type | Field | Notes |
|-----|------|-------|-------|
| 0   | i16  | tid    | Thing ID (for scripting references). |
| 2   | i16  | x | |
| 4   | i16  | y | |
| 6   | i16  | z      | Height above sector floor. |
| 8   | i16  | angle  | |
| 10  | u16  | type   | |
| 12  | u16  | flags  | Extends Doom flags; adds class bits (fighter/cleric/mage), DORMANT (0x0010 shifted), etc. |
| 14  | u8   | special | Action special executed by the thing. |
| 15  | u8   | arg1 | |
| 16  | u8   | arg2 | |
| 17  | u8   | arg3 | |
| 18  | u8   | arg4 | |
| 19  | u8   | arg5 | |

### 4.2 LINEDEFS (Hexen) — 16 bytes/record

| Off | Type | Field | Notes |
|-----|------|-------|-------|
| 0   | i16  | startVertex | |
| 2   | i16  | endVertex | |
| 4   | u16  | flags   | Doom flags + Hexen extras (REPEATABLE 0x0200, activation-type bits SPAC_* in 0x0400–0x1C00, MONSTERSCANACTIVATE, BLOCK_PLAYERS, BLOCKEVERYTHING…). |
| 6   | u8   | special | Action special (0–255). |
| 7   | u8   | arg1 | The **args replace the Doom `tag` field**: arg1 is often the tag. |
| 8   | u8   | arg2 | |
| 9   | u8   | arg3 | |
| 10  | u8   | arg4 | |
| 11  | u8   | arg5 | |
| 12  | i16  | rightSidedef | |
| 14  | i16  | leftSidedef | |

The 5-args + special model is the heart of the "Hexen (map format)" games and is what UDMF
generalizes. See [ZDoom Wiki: Hexen map format](https://zdoom.org/wiki/Hexen_map_format).

### 4.3 BEHAVIOR — compiled ACS object

`BEHAVIOR` is the compiled-ACS lump that the [build/test pipeline](07-build-test-pipeline.md)
produces with `acc` (its **presence in a map group selects the Hexen/UDMF binary path**, §1.4).
elads does not compile ACS itself — it reads/writes `BEHAVIOR` as an opaque blob and shells out
to `acc` — but the object layout is documented here so a validator/inspector can parse it.

```
header (12 bytes):
  char  magic[4]   // "ACS\0" (ACS0, Hexen)  |  "ACSE" (enhanced)  |  "ACSe" (compressed/GZDoom)
  int32 dirOffset  // byte offset to the info/directory
ACS0 (Hexen): at dirOffset →  int32 scriptCount;  then scriptCount × {int32 scriptNumber+type,
              int32 codeOffset, int32 argCount};  then int32 stringCount; then string offsets.
ACSE/ACSe (ZDoom): a chunk stream (FourCC-tagged: SPTR scripts, STRL strings, FUNC functions,
              ARAY/AINI arrays, MINI, MEXP/MIMP module im/exports, LOAD, …). ACSe is ACSE with
              a compressed representation. GZDoom emits ACSE/ACSe via `acc`/`bcc`.
```

Fields are little-endian. The chunked ACSE format is the one modern GZDoom projects use; treat
`acc` output as authoritative and see the [ZDoom Wiki: ACS](https://zdoom.org/wiki/ACS) and
`acc`'s `pcode.hpp` for exact chunk semantics. Cross-ref: [pipeline](07-build-test-pipeline.md) §3.

---

## 5. UDMF — Universal Doom Map Format

A **text** map. The map group is `TEXTMAP` (the whole map, an ASCII lump) plus the usual build
lumps in a wrapper, terminated by an `ENDMAP` marker. Everything else lives in `TEXTMAP`.
Spec: [UDMF 1.1](https://doomwiki.org/wiki/UDMF) / [ZDoom Wiki: UDMF](https://zdoom.org/wiki/Universal_Doom_Map_Format).

### 5.1 Grammar (EBNF-ish, C-like)

```
translation_unit := global_expr_list
global_expr      := block | assignment_expr
block            := identifier '{' expr_list '}'
assignment_expr  := identifier '=' value ';'
value            := integer | float | quoted_string | keyword   // true/false are keywords
```

- The **first** statement must be `namespace = "…";` — e.g. `"Doom"`, `"Heretic"`, `"Hexen"`,
  `"Strife"`, `"ZDoom"`, `"ZDoomTranslated"`. The namespace fixes which keys/specials are legal
  and default flag semantics.
- `//` line comments and `/* */` block comments allowed. Whitespace insignificant.
- Booleans default to `false` when a key is absent; numeric defaults are `0`; string defaults
  are empty. Implementations must ignore unknown keys (forward-compat).

### 5.2 Blocks and common keys

Blocks are position-indexed by declaration order (the Nth `vertex{}` is vertex index N).

**vertex** — `x`, `y` (float, required). ZDoom adds `zfloor`, `zceiling` for slope hints.

**linedef** — `v1`, `v2` (int vertex indices, required); `sidefront` (int, required),
`sideback` (int, default −1); `special`, `arg0`…`arg4` (int); flags as booleans:
`blocking`, `blockmonsters`, `twosided`, `dontpegtop`, `dontpegbottom`, `secret`,
`blocksound`, `dontdraw`, `mapped`, plus ZDoom `passuse`, `repeatspecial`,
`playercross`/`playeruse`/`monstercross`/… (SPAC activation as individual bools),
`blockeverything`, `id` (line's own tag/UDMF id).

**sidedef** — `sector` (int, required); `offsetx`, `offsety` (int);
`texturetop`, `texturemiddle`, `texturebottom` (string, default `"-"`); ZDoom per-texture
`offsetx_top`, `scalex_mid`, `light`, `lightabsolute`, etc.

**sector** — `heightfloor`, `heightceiling` (int); `texturefloor`, `textureceiling` (string,
required); `lightlevel` (int, default 160); `special`, `id` (int); ZDoom adds `xpanningfloor`,
`rotationfloor`, `lightfloor`, `gravity`, `fadecolor`, `lightcolor`, per-plane slope keys, etc.

**thing** — `x`, `y` (float, required); `height` (float, the Hexen z); `angle` (int);
`type` (int, required); `id` (int TID); `special`, `arg0`…`arg4` (int); skill/flag booleans
`skill1`…`skill5`, `ambush`, `single`, `dm`, `coop`, `class1`/`class2`/`class3`,
`friend`, `dormant`, `standing`, `strifeally`, etc.

### 5.3 Minimal TEXTMAP example (illustrative)

```
namespace = "ZDoom";

thing { x = 32.0; y = 32.0; type = 1; }              // player 1 start

vertex { x = 0.0;   y = 0.0;   }
vertex { x = 128.0; y = 0.0;   }
vertex { x = 128.0; y = 128.0; }
vertex { x = 0.0;   y = 128.0; }

linedef { v1 = 0; v2 = 1; sidefront = 0; blocking = true; }
// … 3 more linedefs …

sidedef { sector = 0; texturemiddle = "STONE2"; }

sector {
  heightfloor = 0; heightceiling = 128;
  texturefloor = "FLOOR4_8"; textureceiling = "CEIL3_5";
  lightlevel = 192;
}
```

elads keeps a lossless key/value bag per element so unknown-namespace keys round-trip untouched
(see [data-model](03-data-model.md)); it does **not** discard keys it doesn't model.

---

## 6. Composite wall textures: TEXTURE1/TEXTURE2 + PNAMES

Doom wall textures are **composited** from patches at load time. Three lumps cooperate.

### 6.1 PNAMES

| Off | Type | Field |
|-----|------|-------|
| 0   | i32  | numPatches |
| 4   | char[8] × N | patch lump names (index = "patch number"). |

### 6.2 TEXTUREx

```
i32  numTextures
i32  offsets[numTextures]     // byte offset from lump start to each MapTexture
```

Each **MapTexture**:

| Off | Type    | Field | Notes |
|-----|---------|-------|-------|
| 0   | char[8] | name  | Texture name (referenced by SIDEDEFS). |
| 8   | u32     | masked | Boolean (unused by vanilla; Strife stores flags here). |
| 12  | u16     | width | |
| 14  | u16     | height | |
| 16  | u32     | columnDirectory | Obsolete/ignored (kept for vanilla compat). |
| 20  | u16     | patchCount | |
| 22  | MapPatch × patchCount | patches placed into the texture. |

Each **MapPatch** (10 bytes):

| Off | Type | Field | Notes |
|-----|------|-------|-------|
| 0   | i16  | originX | X offset within texture. |
| 2   | i16  | originY | Y offset. |
| 4   | i16  | patchIndex | Index into PNAMES. |
| 6   | i16  | stepDir | Unused in vanilla. |
| 8   | i16  | colormap | Unused in vanilla. |

Composition: for each patch, draw the patch (§7) at (originX, originY); later patches overwrite
earlier ones; transparent posts leave underlying pixels. Empty (never-drawn) columns are
transparent — the classic "F_SKY" / medusa-adjacent behavior. GZDoom's `TEXTURES` lump (a text
format, [ZDoom Wiki: TEXTURES](https://zdoom.org/wiki/TEXTURES)) supersedes this with
translucency, scaling, offsets, and non-patch sources; elads' TEXTUREx editor writes both (see
[graphics-texture-editor](06-graphics-texture-editor.md)).

---

## 7. Doom picture (patch/sprite/graphic) format

The column-oriented, run-length, palettized sprite/patch format. Used for wall patches,
sprites, and most UI graphics. Pixels are **palette indices** (see §9), not RGB.

### 7.1 Header

| Off | Type | Field | Notes |
|-----|------|-------|-------|
| 0   | u16  | width  | |
| 2   | u16  | height | |
| 4   | i16  | leftOffset | Horizontal draw offset (sprite origin). |
| 6   | i16  | topOffset  | Vertical draw offset. |
| 8   | u32 × width | columnOffsets | Byte offset (from picture start) to each column's post data. |

### 7.2 Column = sequence of posts, terminated by `0xFF` topdelta

Each **post**:

```
u8  topdelta      // y of first pixel; 0xFF ends the column
u8  length        // number of pixels in this post
u8  unused        // padding byte (ignored)
u8  pixels[length]
u8  unused        // padding byte (ignored)
```

Gaps between posts (topdelta jumps) are **transparent**. This is how sprites get their
cut-out silhouette. For pictures taller than 254, GZDoom interprets successive posts with a
non-decreasing "tall patch" convention (topdelta treated as delta when it would otherwise go
backwards); vanilla capped effective height at 254.

### 7.3 `grAb` chunk

PNG sprites/patches carry offsets in a PNG ancillary chunk named `grAb`: 8-byte payload =
i32 xOffset, i32 yOffset (big-endian, per PNG rules). elads reads/writes `grAb` so PNG sprites
keep their pivot; see [graphics-texture-editor](06-graphics-texture-editor.md) and
[Doom Wiki: grAb](https://doomwiki.org/wiki/GRAB).

---

## 8. Flats

Flats are floor/ceiling textures: **raw, uncompressed, palettized, column-major-free** —
simply `width×height` palette-index bytes with no header.

| Size (bytes) | Dimensions | Use |
|--------------|------------|-----|
| 4096 | 64×64 | Standard flat. |
| 8192 | 64×128 | Tall animated (e.g. some sky/scroll). |
| 64000 | 320×200 | Raw full-screen image (e.g. Heretic/Hexen fullscreen graphics, `AUTOPAGE`); the raw fullscreen size also referenced by [data-model](03-data-model.md) §3 and [graphics](06-graphics-texture-editor.md) §3.2. |
| 65536 | 256×256 | Heretic/large. |

Because there is no header, dimensions are inferred from the lump size and namespace (§1.3
`F_START`). GZDoom/PK3 relaxes this: any image in `flats/` may be used as a flat regardless of
format or dimension.

---

## 9. PLAYPAL and COLORMAP

### 9.1 PLAYPAL — palettes

`N` palettes of 256 colors, each color = 3 bytes RGB (no alpha):

```
palette[p][i] = { u8 R, u8 G, u8 B }      // 768 bytes per palette
```

Doom's PLAYPAL holds 14 palettes (768 × 14 = 10752 bytes): palette 0 = normal; 1–8 = damage/red
tints (increasing); 9–12 = item-pickup gold flashes; 13 = radiation-suit green. The engine
swaps the active palette for full-screen tints. elads renders index→RGB through palette 0 by
default; see [graphics-texture-editor](06-graphics-texture-editor.md).

### 9.2 COLORMAP — light levels

34 maps × 256 bytes. Each map is a lookup `index → index` (into PLAYPAL) for a given light
level:

| Maps | Purpose |
|------|---------|
| 0–31 | Diminishing light: 0 = full bright, 31 = near black. |
| 32   | Invulnerability (inverted greyscale). |
| 33   | All-black (used for `F_SKY`/void). |

Total 34 × 256 = 8704 bytes. Boom adds custom named colormaps between `C_START`/`C_END` for
colored sectors. GZDoom's true-color renderer derives lighting mathematically but still honors
COLORMAP for the "classic" software look and for fog/tint definitions.

---

## 10. Sprite naming convention

Sprite lump names encode frame + rotation into the 8-char name:

```
NAME  F  R  [F2 R2]
└─4─┘ │  │
      │  └ rotation digit 0–8 (0 = "rot-independent, use for all angles")
      └ frame letter A–Z (also '[' '\' ']' for extended frames)
```

- `TROOA1` = actor `TROO`, frame `A`, rotation `1` (facing angle bucket 1 of 8).
- Rotation `0` = single sprite used for all viewing angles.
- **Mirroring**: a name may pack **two** frame/rotation pairs — `TROOB1B5` means: use this
  graphic for frame `B` rotation `1`, **and** for frame `B` rotation `5` **mirrored** (flipped
  horizontally). Saves memory for symmetric actors.
- Rotations map to viewing angles: 1 = facing viewer, going counter-clockwise; 8 rotations = 45°
  buckets. GZDoom supports 16 rotations via hex digits `9,A…G`.

Reference: [Doom Wiki: Sprite](https://doomwiki.org/wiki/Sprite). elads' sprite browser parses
this to group frames; see [graphics-texture-editor](06-graphics-texture-editor.md).

---

## 11. Sound formats

### 11.1 DMX digital sound (the classic `DS*` / `DP*` lumps)

| Off | Type | Field | Notes |
|-----|------|-------|-------|
| 0   | u16  | formatId | Always `3` for PCM. |
| 2   | u16  | sampleRate | Usually 11025 Hz (vanilla). |
| 4   | u32  | sampleCount | Number of samples (bytes; 8-bit mono). |
| 8   | u8[16] | padding | 16 leading pad samples (copies of first sample). |
| …   | u8 × sampleCount | 8-bit unsigned mono PCM. | |
| end | u8[16] | trailing pad. | |

The real audio length excludes the 16+16 padding samples. Reference:
[Doom Wiki: DMX (sound)](https://doomwiki.org/wiki/Sound). GZDoom also accepts raw headerless
PCM, WAV, FLAC, Ogg Vorbis, and MP3 in `sounds/`.

### 11.2 PC speaker (`DP*`)

Small format: u16 `0`, u16 length, then bytes = PC-speaker frequency indices. Rarely edited;
elads shows a hex/preview view.

---

## 12. Music formats

### 12.1 MUS — the id/DMX music format

A compact MIDI-like event stream. Header:

| Off | Type | Field | Notes |
|-----|------|-------|-------|
| 0   | char[4] | id | `"MUS\x1a"` (4D 55 53 1A). |
| 4   | u16 | scoreLen | Length of the event body. |
| 6   | u16 | scoreStart | Offset to first event. |
| 8   | u16 | channels | Primary channels. |
| 10  | u16 | secChannels | Secondary channels. |
| 12  | u16 | instrCount | |
| 14  | u16 | reserved | |
| 16  | u16 × instrCount | instrument (patch) numbers. |

Events: a byte with a 3-bit event type + channel, optional delay (variable-length quantity in
128s), for note-on/off, pitch bend, controller, and end-of-score. GZDoom converts MUS→MIDI at
load. Reference: [Doom Wiki: MUS](https://doomwiki.org/wiki/MUS).

### 12.2 Formats GZDoom accepts

| Family | Formats |
|--------|---------|
| Sequenced | MUS, MIDI (SMF type 0/1), HMI/HMP, XMI (Miles). Rendered via a soft-synth (FluidSynth + SoundFont, OPL emulation, or GUS patches). |
| Tracker | MOD, S3M, XM, IT, and others via libmodplug/ZMusic's dumb/openmpt. |
| Streaming | Ogg Vorbis, MP3, FLAC, WAV, Opus. |

elads does not synthesize audio itself; it identifies the format (magic bytes) and hands
playback to SLADE's audio path (SFML/OpenAL + a bundled synth) or shells to GZDoom for playtest.
See [build-test-pipeline](07-build-test-pipeline.md). Note: FluidSynth is an *optional* SLADE
dep on the Pi target (see [rpi5-target](09-rpi5-target.md)).

---

## 13. Endianness & round-trip rules (implementer notes)

- Everything binary is little-endian. On aarch64 (little-endian) the packed structs match, but
  **do not** rely on `memcpy` of native structs for portability — read fields with explicit
  little-endian accessors so a future big-endian CI host stays correct.
- `char[8]` name fields are **not** NUL-terminated when full; always bound reads to 8 and
  uppercase on write.
- Preserve unknown lumps and unknown UDMF/`TEXTURES`/ZScript keys verbatim for lossless
  round-trips (a core elads correctness goal; see [data-model](03-data-model.md)).
- Build lumps (SEGS…BLOCKMAP, extended nodes) are **regenerated** on save by the node builder,
  never hand-edited; the loader may keep them for engines that don't rebuild, but elads' source
  of truth is the geometry model. See [build-test-pipeline](07-build-test-pipeline.md).

### Related docs

- [data-model](03-data-model.md) — the in-memory shapes these bytes populate.
- [graphics-texture-editor](06-graphics-texture-editor.md) — picture/flat/palette rendering and TEXTUREx/TEXTURES editing.
- [map-editor](04-map-editor.md) — how map lumps become editable geometry.
- [build-test-pipeline](07-build-test-pipeline.md) — node/BLOCKMAP/REJECT regeneration and playtest.
- [rpi5-target](09-rpi5-target.md) — endianness and codec/dependency notes for aarch64.
