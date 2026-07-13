# Architecture Decision Records

Each ADR captures one significant, hard-to-reverse decision: its context, the choice, and
the consequences. Format is lightweight (Michael Nygard style). Status is one of
`proposed` / `accepted` / `superseded`.

| ADR | Decision | Status |
|-----|----------|--------|
| [0001](ADR-0001-fork-slade3.md) | Fork & extend SLADE3 rather than build from scratch or port UDB | accepted |
| [0002](ADR-0002-gles31-render-backend.md) | Target OpenGL ES 3.1 via a render-abstraction layer | accepted |
| [0003](ADR-0003-wxwidgets-toolkit.md) | Keep wxWidgets as the GUI toolkit | accepted |
| [0004](ADR-0004-gplv3.md) | License the project GPLv3 | accepted |
| [0005](ADR-0005-gzdoom-udmf-first.md) | Optimise for GZDoom + UDMF first | accepted |
| [0006](ADR-0006-lua-sol2-plugins.md) | Lua/sol2 as the primary plugin surface | accepted |
| [0007](ADR-0007-nodebuilders-toolchain.md) | Consume AJBSP/ZDBSP/acc/GZDoom as tools, not code | accepted |
| [0008](ADR-0008-earcut-triangulation.md) | Use earcut.hpp for sector triangulation | accepted |
