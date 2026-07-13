# ADR-0005 — Optimise for GZDoom + UDMF first

- **Status:** accepted
- **Date:** 2026-07

## Context

Doom maps come in several formats (classic Doom, Hexen, and text-based **UDMF**) and target many
engines (vanilla, Boom, MBF21, ZDoom/GZDoom, DSDA-Doom). Supporting every engine's node/limit
rules as a first-class path is a large compatibility surface. The Pi-native happy path uses
GZDoom (which renders on the Pi only via its GLES backend) as both playtest engine and
authoritative ZScript/DECORATE validator.

## Decision

**GZDoom + UDMF is the first-class target.** Defaults: UDMF (`gzdoom` namespace), **ZDBSP XGL3**
nodes, one-click **GZDoom-GLES** playtest. elads still **opens and edits** Doom/Hexen/Boom maps,
but broad multi-engine output (vanilla-limit-safe nodes, compressed BLOCKMAP, full REJECT,
per-engine profiles) is deferred to later phases behind engine profiles.

## Consequences

- Simplest, most reliable path on the Pi; matches the modern GZDoom modding mainstream.
- The pipeline (ADR-0007) can lean on GZDoom rebuilding nodes at load and on ZScript over ACS,
  keeping `acc` off the critical path unless a map uses ACS.
- Vanilla/Boom authors get editing today, best-in-class output later. Tracked as an open decision.

## Alternatives considered

- **Broad multi-engine from day one** — more compatibility engineering up front (nodes, limits,
  REJECT) that slows the core loop. Deferred, not rejected; revisited via engine profiles.
