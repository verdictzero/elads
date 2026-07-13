# mapeditor/view3d — 3D visual mode

Free-fly in-editor walkthrough rendering the map with real textures. Staged toward
UDB parity: textures+light+fog → slopes (plane eval in vertex shader) → stacked 3D
floors (synthesized slabs) → capped forward dynamic lights (UBO) → sprites/MODELDEF.
Real-time shadowmaps stay off by default (V3D fill-rate). Draws through `src/render`.
