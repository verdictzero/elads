# ADR-0004 — License the project GPLv3

- **Status:** accepted
- **Date:** 2026-07

## Context

elads forks SLADE3 (ADR-0001). SLADE's per-file headers grant **"version 2 of the License, or
(at your option) any later version"** — i.e. **GPLv2-or-later**, not GPLv2-only. We also study
(never copy) GPLv3 projects (UDB) and bundle GPL tools (ZDBSP, `acc`, GZDoom, AJBSP).

Under FSF compatibility doctrine, GPLv2-**or-later** code may be taken under GPLv3, and a genuine
combined/linked work is then distributed as GPLv3. (Had SLADE been GPLv2-**only**, no lawful
combination with GPLv3 code would be possible — so the "or later" grant is load-bearing.)

## Decision

**License elads under GPLv3** (see [`/LICENSE`](../../LICENSE)). New source files carry a
GPLv3 header. SLADE-origin files retain their original "v2-or-later" headers; the combined work
ships as GPLv3.

## Consequences

- Lawful reuse of SLADE and combination with GPLv3-compatible components.
- **No closed-source/commercial distribution** of the combined binary — accepted; elads is a
  community open-source project.
- Bundled tools invoked as **separate processes** (GZDoom, `acc`, ZDBSP) keep clean license
  boundaries; only code we actually link is part of the combined work.
- Game-configuration/definition data is authored fresh for elads (UDB's `.cfg` format is not
  interchangeable and is not copied), avoiding provenance questions.

## Alternatives considered

- **Permissive/closed (MIT/BSD/proprietary)** — would require the from-scratch strategy with
  clean-room subsystems and permissive-only deps. Rejected given the fork decision.
