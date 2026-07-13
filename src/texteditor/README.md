# texteditor — code editor

Reuses SLADE's Scintilla-based editor (`wxStyledTextCtrl` + `SCLEX_CONTAINER`) with
**data-driven** language definitions in `res/config/languages/` for ZScript,
DECORATE, ACS, and the definition lumps (MAPINFO, GLDEFS, SNDINFO, …). Hosts the
Thing/actor browser join and the Lua console. See `docs/design/05-text-script-editor.md`.
