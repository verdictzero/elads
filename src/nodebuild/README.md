# nodebuild — embedded BSP node builder

Turns a `MapModel` into the "build lumps" a source port needs to run a map —
**SEGS**, **SSECTORS**, **NODES**, **BLOCKMAP**, **REJECT** — plus a **VERTEXES**
lump augmented with any split points, with no external tool. A self-contained
recursive BSP builder (the *embed* option from ADR-0007; AJBSP/ZDBSP as an
external subprocess remains the path for pathological maps). Classic 16-bit
vanilla node format, so every source port accepts the output. GUI/GL-free.

- `buildNodes(model)` → the six build lumps + stats (seg/subsector/node/split
  counts).
- `buildMapLumps(model, format)` → a complete, canonically-ordered playable map
  lump set (editable lumps + built lumps), ready to place after a map marker.

CLI: `elads build-nodes <in.wad> <MAP> <out.wad>`. See
`docs/design/07-build-test-pipeline.md` and ADR-0007.
