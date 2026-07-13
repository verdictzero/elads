# Contributing to elads

Thanks for your interest! `elads` is in the **design phase** — the most valuable
contributions right now are review of the design docs and on-device validation on real
Raspberry Pi 5 hardware.

## Ground rules

- **License:** by contributing you agree your work is licensed under **GPLv3** (see
  [`LICENSE`](LICENSE) and [`docs/design/10-licensing.md`](docs/design/10-licensing.md)).
- **Reuse posture:** we fork/extend **SLADE3** (GPLv2-or-later) and consume ARM64-clean
  tools (AJBSP, ZDBSP, `acc`, GZDoom) as libraries or separate processes. **Ultimate Doom
  Builder is a UX/behaviour reference only** — do not copy its C# source (wrong language,
  GPLv3, and it muddies provenance). When you port an idea, note the source in the commit.
- **Target discipline:** the Raspberry Pi 5 is the first-class target. Any rendering code
  must go through the **Render Abstraction Layer** (`src/render/`) and must work on
  **OpenGL ES 3.1**. Never introduce a hard dependency on desktop GL 3.3+ features. See
  [`docs/design/02-render-abstraction.md`](docs/design/02-render-abstraction.md).

## What we need now (Phase 0)

See [`docs/roadmap.md`](docs/roadmap.md). The immediate de-risking work:

1. Run [`scripts/probe-gl.sh`](scripts/probe-gl.sh) on a real Pi 5 and attach the output
   (`glxinfo`/`eglinfo`/`vulkaninfo`) to an issue — we need confirmed GL/GLES/Vulkan levels
   per Pi OS image (Bookworm vs Trixie).
2. Run [`scripts/build-toolchain.sh`](scripts/build-toolchain.sh) on aarch64 and report
   build results for AJBSP / ZDBSP / `acc`.
3. Review the design docs and open issues for gaps or corrections.

## Code style (for when code lands)

- **C++17.** Follow the surrounding SLADE-derived style in a given file. New modules use
  `lower_snake_case` filenames and 4-space indent (see [`.editorconfig`](.editorconfig)).
- All GLSL is authored as **`#version 310 es`** and lives under `res/shaders/gles/`.
- Prefer data-driven config (lexer keyword lists, game profiles) over hard-coded tables.

## Commits & PRs

- Small, focused commits with descriptive messages; reference the affected module/doc.
- CI (`.github/workflows/ci.yml`) must pass: markdown link check, script `bash -n`, and the
  stub CMake configure on `ubuntu-24.04-arm`.
- Do not open a PR that adds a rendering path outside `src/render/`.
