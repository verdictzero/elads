# Build & test pipeline

The author → build → test loop: how the `pipeline` module turns an in-memory map + resource
archive into node-built maps, compiled ACS, and a running GZDoom playtest — and how engine
errors flow back into the editor. Everything here is orchestrated from `src/pipeline/`. The
node/ACS/engine tools are consumed as ARM64-clean external tools, not vendored source (see
[ADR-0007](../decisions/ADR-0007-nodebuilders-toolchain.md)); Pi-specific GLES runtime concerns
live in [rpi5-target](09-rpi5-target.md).

---

## 1. Scope and the loop

```
   map model (mapeditor/model)          resource archive (archive/)
        │                                     │
        │  serialize dirty map               │  ZScript/DECORATE/textures/ACS src
        ▼                                     ▼
  ┌──────────────────────── src/pipeline ─────────────────────────┐
  │                                                               │
  │  1. NODE BUILD   AJBSP (embedded lib)  |  ZDBSP (external)     │
  │  2. ACS COMPILE  acc (external)  [bcc / gdcc alternates]       │
  │  3. STAGE        assemble a temp WAD/PK3 (project + built map) │
  │  4. LAUNCH       build GZDoom command line, spawn, capture     │
  │  5. SURFACE      parse -stdout/-stderr → editor diagnostics    │
  │                                                               │
  └───────────────────────────────────────────────────────────────┘
        │                                     ▲
        ▼                                     │  jump to file:line
   GZDoom-GLES process  ───── stdout/stderr ──┘
```

Design invariants:

- **The pipeline is orchestration, not implementation.** We never re-implement node building or
  ACS; we drive tools ([ADR-0007](../decisions/ADR-0007-nodebuilders-toolchain.md)). One tool is
  linked in-process (AJBSP); the rest are child processes.
- **GZDoom is the authority.** GZDoom is both the playtest engine and the authoritative
  ZScript/DECORATE validator. The editor tokenizes but does not semantically validate (see
  [text-script-editor](05-text-script-editor.md) §7); the pipeline delegates truth to GZDoom.
- **Nodes are conditional.** GZDoom rebuilds nodes at load, so for GZDoom-only iteration node
  building can be skipped entirely. External nodes are for pre-baking releases and for
  vanilla/Boom/other engines that need on-disk nodes.
- **ACS is conditional.** `acc` runs only when a map (or the project) actually contains ACS
  source; a pure-ZScript project never invokes it.

---

## 2. Node building

Two builders, chosen per job. The decision to embed one and bundle the other is
[ADR-0007](../decisions/ADR-0007-nodebuilders-toolchain.md).

### 2.1 AJBSP — embedded, in-process

- Linked as a **library** into `pipeline` (source from the AJBSP/Eureka lineage). No process
  spawn, no temp-file round-trip for the common case; fast enough to run on every save or before
  every quick playtest.
- Produces **standard** nodes, **GL** nodes, and **XNOD** (ZDoom uncompressed extended nodes).
  Reads **UDMF** input directly, which matters because our maps are UDMF-native
  (see [data-model](03-data-model.md)).
- Does **not** produce the XGL2/XGL3 compressed-GL extended formats that GZDoom prefers for large
  UDMF maps — that is ZDBSP's job.
- Illustrative wrapper (intent, not a real AJBSP signature):

  ```cpp
  // pipeline/nodes/AjbspBuilder — illustrative
  struct NodeJob {
      MapData        map;           // UDMF vertices/linedefs/sectors
      NodeFormat     format;        // Standard | GL | XNOD
      bool           build_blockmap = true;
      bool           build_reject   = false;   // zero-fill unless engine profile asks
  };
  Result<NodeLumps> AjbspBuilder::build(const NodeJob&);   // in-process, no fork
  ```

### 2.2 ZDBSP — bundled external binary

- Invoked out-of-process (`ZDBSP_bin -o <out.wad> -X<fmt> <in.wad>`-style CLI). Built by
  `scripts/build-toolchain.sh`.
- Produces the full GZDoom set: standard, GL, **XGL** / **XGL2** / **XGL3**, and can emit
  **UDMF** output. This is the builder to use when pre-baking nodes that must match GZDoom's
  in-engine expectations exactly, or when targeting the widest engine coverage.
- **aarch64-clean:** ZDBSP's x86 SSE classifier files are gated on 32-bit in its own CMake and
  auto-excluded on 64-bit, so the Pi build compiles the scalar path unchanged
  ([ADR-0007](../decisions/ADR-0007-nodebuilders-toolchain.md); ZDBSP `CMakeLists.txt`).
- Slower (fork + WAD I/O), so it is used for release pre-bake and non-GZDoom profiles, not for
  every keystroke.

### 2.3 Node-format matrix

Which format each engine wants, and which builder can produce it.

| Format            | Purpose / who consumes it                    | Needed by engine                      | AJBSP | ZDBSP |
|-------------------|----------------------------------------------|---------------------------------------|:-----:|:-----:|
| **Standard** (NODES/SEGS/SSECTORS) | classic BSP, ≤32767 segs   | vanilla, Boom, DSDA, MBF21            |  ✅   |  ✅   |
| **GL nodes** (GL_VERT/GL_SEGS/...) | precise GL rendering, sub-px  | (G)ZDoom GL, PrBoom+/DSDA GL, Eternity |  ✅   |  ✅   |
| **XNOD** (ZDoom extended, uncompressed) | >32767 seg limit-break | (G)ZDoom                              |  ✅   |  ✅   |
| **ZNOD** (XNOD, zlib-compressed)   | compact extended nodes        | (G)ZDoom                              |  ❌   |  ✅   |
| **XGL** (extended GL, uncompressed) | extended + GL combined       | (G)ZDoom, Eternity                    |  ❌   |  ✅   |
| **XGL2**          | extended GL, v2 (UDMF-oriented)              | GZDoom (large UDMF)                   |  ❌   |  ✅   |
| **XGL3**          | extended GL, v3 (current GZDoom default pref)| GZDoom (UDMF)                         |  ❌   |  ✅   |
| **UDMF-native** (nodes as UDMF lump/output) | round-trip UDMF     | GZDoom (rebuilds at load anyway)      |  ✅¹  |  ✅   |

¹ AJBSP reads UDMF and emits nodes for the UDMF map; it does not emit the compressed XGL2/XGL3
variants. Format taxonomy: zdoom.org wiki "Node" / "ZDBSP"; doomwiki.org "Node builder".

### 2.4 When to build nodes at all

```
GZDoom-only quick playtest ......... skip node build (engine rebuilds at load)  → fastest
GZDoom playtest, large UDMF map .... optional XGL3 pre-bake to shave load time  (ZDBSP)
Release / distributable WAD ........ pre-bake XGL3 (GZDoom) + standard+GL (compat) (ZDBSP)
Vanilla / Boom / DSDA profile ...... MUST pre-bake standard (+GL) nodes          (AJBSP/ZDBSP)
Every-save background build ......... AJBSP GL/XNOD in-process (cheap)
```

The default GZDoom profile therefore *defers* nodes: quick-test spawns GZDoom on the raw staged
map and lets the engine build. External builders exist for the other rows.

---

## 3. ACS compilation

Runs only when the map's `SCRIPTS`/`ACS` source or a project ACS lump is present.

- **`acc`** (default): the ZDoom ACS compiler. Invoked as
  `acc -i <include-dir> <source.acs> <out.o>`; the include dir must contain `zcommon.acs`
  (which pulls `zdefs.acs`, `zspecial.acs`, `zwvars.acs`). Output is the compiled object placed
  as the map's **`BEHAVIOR`** lump (and any `SCRIPTS` source retained for reference). `acc` is
  ~99% portable C and endianness-safe, so it builds and runs on aarch64 unchanged
  ([ADR-0007](../decisions/ADR-0007-nodebuilders-toolchain.md)).
- **Alternates:** `bcc` (Brad Carney's compiler, extra language features / Zandronum) and `gdcc`
  (GDCC, C-like → ACS/ACS0) are selectable per project. Same contract: source in, `BEHAVIOR`/`.o`
  out.
- **Placement:** for a WAD map, `BEHAVIOR` sits in the map's marker lump group; for UDMF, it is a
  member lump between the map's `TEXTMAP` and `ENDMAP` markers (see
  [formats-reference](08-formats-reference.md)).
- Illustrative:

  ```cpp
  // pipeline/acs/AcsCompiler — illustrative
  struct AcsJob {
      Path        source;                 // extracted .acs
      Path        include_dir;            // dir containing zcommon.acs
      AcsBackend  backend = AcsBackend::Acc;   // Acc | Bcc | Gdcc
  };
  Result<Lump> AcsCompiler::compile(const AcsJob&);   // → BEHAVIOR lump
  ```

- **Errors:** `acc` prints `file:line: message` diagnostics to stdout/stderr; the pipeline parses
  these with the same diagnostic pathway as GZDoom errors (§5) so an ACS syntax error jumps to
  the offending `.acs` line in the text editor.

---

## 4. Playtest launcher

### 4.1 Command-line assembly

The launcher builds a GZDoom invocation from the resolved IWAD, the staged project file, and the
target map:

```
gzdoom -iwad <IWAD>
       -file <project.pk3|.wad> [<extra resources>…]
       (-warp <E> <M>  |  +map MAPxx)
       +vid_preferbackend 3          # force the GLES/Vulkan-capable backend on the Pi
       [+skill <n>] [+notarget] [-nomonsters] [+sv_cheats 1] [+freeze 1]
       -stdout -stderr               # capture logs (see §5)
```

- **`+vid_preferbackend 3`** selects GZDoom's GLES backend, which is the only backend that
  renders on the Pi's VideoCore VII / Mesa V3D (GLES 3.1); the desktop-GL backend would fall to
  llvmpipe. Backend rationale and values are in [rpi5-target](09-rpi5-target.md) and GZDoom's
  `gl_backend`/`vid_preferbackend` handling (GZDoom "gles").
- **Warp vs map:** classic `ExMy`/`MAPxx` IWADs use `-warp E M`; named/UDMF maps use `+map <name>`.
  The launcher picks based on the map's lump name and the game config.
- **Skill / flags:** playtest presets (e.g. "test from here, no monsters") set `+skill`,
  `+notarget`, `-nomonsters`. "Test from current 3D-editor position" additionally passes a
  `+warp`-to-position via a generated console script if enabled.

Illustrative:

```cpp
// pipeline/launch/GZDoomLauncher — illustrative
struct LaunchSpec {
    Path              iwad;
    Path              project;      // staged pk3/wad
    std::string       map;          // "MAP01" or ExMy
    std::optional<int> skill;
    bool              notarget = false;
    bool              nomonsters = false;
    EngineProfile     profile;      // §6
};
std::vector<std::string> GZDoomLauncher::argv(const LaunchSpec&);
```

### 4.2 IWAD selection & config

- IWAD paths are resolved from a user-configured search list (SLADE-style base-resource archive
  list) plus GZDoom's own IWAD auto-detection as fallback. The chosen IWAD is validated by
  header/lump signature before launch so a missing/renamed IWAD fails fast with a clear message.
- The GZDoom binary path, extra `-file` resources (e.g. a widescreen/HUD mod), and default flags
  are stored per engine profile (§6). Config layout is modeled on SLADE's `nodebuilders.cfg`
  (§6.2).

### 4.3 Staging

Before launch the pipeline assembles a temp file combining the project resources with the freshly
built map (nodes/`BEHAVIOR` included when built). For iterative testing this can be a directory or
an uncompressed WAD to avoid recompressing a PK3 on every run. The staged file is what `-file`
points at; the original project archive is never mutated by a playtest.

---

## 5. Surfacing engine errors back into the editor

GZDoom has **no headless compile-validate mode** — it needs a live GL context
([ADR-0007](../decisions/ADR-0007-nodebuilders-toolchain.md)). So validation = launch + parse.

```
spawn gzdoom (-stdout -stderr)
        │
        │  line-buffered capture (async reader thread)
        ▼
  DiagnosticParser  ── regex/state machine ──►  Diagnostic{ file, line, col, severity, msg }
        │
        ▼
  Diagnostics panel (wxAUI)  ──click──►  open lump in text editor, jump to file:line
```

- **What we parse:** GZDoom's script compiler prints errors in the shape
  `Script error, "<lump>:<file>" line <N>: <message>` (and `Warning,` variants); DECORATE/ZScript
  parse failures, missing textures/actors, and MAPINFO errors all follow recognizable prefixes.
  ACS (`acc`) and node-builder diagnostics are folded into the same `Diagnostic` stream (§3).
- **Mapping to source:** the `<lump>`/`<file>:line` is resolved back to the archive entry the
  editor opened, so a click focuses the exact line (reusing the text editor's navigation, see
  [text-script-editor](05-text-script-editor.md)). Where GZDoom reports a lump name rather than a
  path, we map lump → archive entry via the staging manifest.
- **Fatal vs recoverable:** a fatal script error makes GZDoom exit non-zero before the window
  opens; the launcher detects the early exit, keeps the captured log, and shows diagnostics
  instead of a "crashed" dialog. A clean launch streams warnings live while the game runs.
- **CI note:** automated validation (no display) runs GZDoom against an offscreen EGL/GBM context
  and scrapes the same stdout — this is risk R7 in [risks](../risks.md); see also
  [rpi5-target](09-rpi5-target.md) for the EGL/GBM headless path.

---

## 6. Engine profiles

A profile bundles: engine binary, node-build policy, ACS backend, IWAD/base resources, and launch
flags. **GZDoom is the default and the only fully wired profile today**; the others are planned.

### 6.1 Profile matrix

| Profile        | Node policy                                   | Blockmap / Reject          | ACS  | Launch backend        | Status  |
|----------------|-----------------------------------------------|----------------------------|------|-----------------------|---------|
| **GZDoom** (default) | defer (engine rebuilds); XGL3 pre-bake opt. | engine-built               | acc  | GLES (`+vid_preferbackend 3`) | now |
| **Vanilla**    | limit-safe **standard** nodes (≤32767 segs)   | compressed BLOCKMAP; REJECT built | acc | n/a (chocolate-style) | later |
| **Boom**       | standard + GL                                 | compressed BLOCKMAP; REJECT | acc | PrBoom+/DSDA GL       | later |
| **DSDA / MBF21** | standard + GL, limit-safe                    | compressed BLOCKMAP; REJECT | acc | DSDA-Doom GL          | later |

The later profiles are where AJBSP/ZDBSP node building becomes *mandatory* (those engines do not
rebuild nodes at load) and where extra artifacts — a real (not zero-filled) **REJECT** table and a
**compressed BLOCKMAP** — start mattering for correctness and size.

### 6.2 Config format (modeled on SLADE `nodebuilders.cfg`)

Node builders and their command templates are defined in data, following SLADE's
`nodebuilders.cfg` schema (name, path, options, command line with `%i`/`%o`-style substitutions).
Illustrative:

```
# res/config/nodebuilders.cfg — illustrative, SLADE-style
nodebuilder ZDBSP {
    name    = "ZDBSP";
    command = "-o %o -X %i";        # %i = input, %o = output
    exe     = "zdbsp";
    formats = "standard,gl,xnod,znod,xgl,xgl2,xgl3,udmf";
}
nodebuilder AJBSP {           # embedded, exe empty → in-process
    name    = "AJBSP (embedded)";
    exe     = "";
    formats = "standard,gl,xnod,udmf";
}
```

Engine profiles layer on top with their own small config (engine exe, iwad, default flags, chosen
nodebuilder + format). Keeping this data-driven means adding DSDA-Doom later is a config edit, not
a recompile — the same philosophy as the language definitions in
[text-script-editor](05-text-script-editor.md).

---

## 7. Toolchain build — `scripts/build-toolchain.sh`

The external tools are compiled from source on-device (or in CI) rather than downloaded as
binaries, guaranteeing aarch64-clean builds:

```
scripts/build-toolchain.sh
  ├─ ZDBSP   → cmake + build   (SSE files auto-excluded on 64-bit → scalar path)
  ├─ acc     → make            (portable C; produces acc + zcommon.acs include set)
  ├─ (bcc / gdcc)  optional alternates
  └─ GZDoom  → cmake + build   (GLES backend; run with +vid_preferbackend 3)
     AJBSP    is NOT here — it is vendored + linked into pipeline at app build time
```

Notes:
- AJBSP is the exception: it is compiled *into* the elads binary by the main CMake build, not
  produced as a standalone tool.
- GZDoom builds on aarch64 but only *renders* on the Pi via GLES; the build produces the engine,
  runtime capability is the Pi concern in [rpi5-target](09-rpi5-target.md).
- The script records tool versions into a manifest so the diagnostic parser can adapt to
  known error-format changes across GZDoom releases.

---

## 8. Cross-references

- [ADR-0007](../decisions/ADR-0007-nodebuilders-toolchain.md) — decision to consume AJBSP/ZDBSP/
  acc/GZDoom as tools.
- [rpi5-target](09-rpi5-target.md) — GZDoom GLES backend, `+vid_preferbackend 3`, EGL/GBM headless.
- [formats-reference](08-formats-reference.md) — byte layout of BEHAVIOR, node lumps, UDMF markers.
- [text-script-editor](05-text-script-editor.md) — where diagnostics land; GZDoom as validator.
- [data-model](03-data-model.md) — UDMF map model the node builders consume.
- [risks](../risks.md) — R7 (headless GZDoom validation).
