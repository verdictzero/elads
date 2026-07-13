# ADR-0007 — Consume AJBSP/ZDBSP/acc/GZDoom as tools, not code

- **Status:** accepted
- **Date:** 2026-07

## Context

Compiling and testing a map needs a **node builder** (BSP/GL/extended nodes), an **ACS
compiler**, and a **playtest engine**. All the standard ones are portable C/C++ that build on
aarch64:

- **ZDBSP** — ZDoom's node builder made external; full standard/GL/XGL2/XGL3/UDMF. Its x86 SSE
  classifier files are auto-excluded on 64-bit by its own CMake, so aarch64 builds unchanged.
- **AJBSP** — small, embeddable builder (standard/GL/XNOD), reads UDMF; used by Eureka.
- **acc** — the ACS compiler (~99% portable C, endianness-safe); needed only when a map uses ACS.
- **GZDoom** — builds on aarch64; renders on the Pi only via its **GLES** backend
  (`+vid_preferbackend 3`). It has no headless compile-validate mode.

## Decision

**Embed AJBSP as a library** for fast in-process standard/GL/XNOD building, and **bundle ZDBSP as
an external binary** for full XGL2/XGL3/UDMF coverage matching GZDoom. Bundle **`acc`** (with
`bcc`/`gdcc` as alternates) invoked out-of-process. **Shell out to GZDoom-GLES** for playtesting
and as the authoritative ZScript/DECORATE validator, capturing `-stdout` to surface errors.
`scripts/build-toolchain.sh` builds these from source on-device. Integration config is modeled on
SLADE's `nodebuilders.cfg`.

## Consequences

- No re-implementation of node building or ACS compilation; we track upstream tools.
- Clean license boundaries (ADR-0004): tools run as separate processes.
- On-device "testing" means interactive playtest on a display; automated CI validation uses an
  offscreen EGL context + stdout log parsing (risk R7).
- For GZDoom-only targets, nodes can even be deferred to load time; external node building matters
  for pre-baking and for other engines.

## Alternatives considered

- **Re-implement a node builder** — pointless duplication. Rejected.
- **glBSP / BSP-W32 / ZokumBSP** — kept as optional alternates; ZDBSP+AJBSP cover the defaults.
