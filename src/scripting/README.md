# scripting — Lua/sol2 automation & plugins

Reuses SLADE's Lua 5.4 + sol2 engine. A `ScriptManager` exposes per-domain bindings
(Archive, MapEditor, Game, Graphics, UI) to sandboxed `sol::environment`s. This is
the **primary** plugin surface (automation, custom actions, UI panels, batch asset
ops), avoiding a brittle native-plugin ABI on aarch64.
