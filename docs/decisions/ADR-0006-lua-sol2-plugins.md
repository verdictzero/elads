# ADR-0006 — Lua/sol2 as the primary plugin surface

- **Status:** accepted
- **Date:** 2026-07

## Context

SLADE already embeds **PUC Lua 5.4 + sol2** with per-domain bindings (Archive, MapEditor, Game,
Graphics, UI) exposed to scripts. Native C++ plugins would require a stable ABI, which is brittle
across compilers/versions on aarch64 and complicates distribution.

## Decision

Make **Lua/sol2 the primary extension surface** for automation, custom actions, UI panels, and
batch asset operations, managed by a `ScriptManager` handing each script a sandboxed
`sol::environment`. Reuse SLADE's binding layout, split per module. A narrow, versioned **C ABI**
for native plugins is added only if a concrete need arises.

## Consequences

- No native-plugin ABI headaches; plugins are portable across builds and arches.
- The scripting API doubles as the automation/testing surface.
- CPU-heavy plugins pay Lua's cost; acceptable for editor automation. `LuaBridge3` is a fallback
  if sol2's template compile times become painful on-device.

## Alternatives considered

- **Native C++ plugins** — best performance, worst portability/ABI stability. Deferred behind a
  future narrow C ABI.
- **Python** — heavier runtime/packaging on the Pi; not SLADE's existing model. Rejected.
- **AngelScript** — no existing SLADE integration; no advantage over Lua here. Rejected.
