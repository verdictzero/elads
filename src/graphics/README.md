# graphics — graphics & texture editor

Reuses SLADE `Graphics`/`SIFormat`: Doom gfx & flats, PCX/TGA/PNG/WebP (libpng,
libwebp, lunasvg), PLAYPAL/COLORMAP palette editing, the TEXTUREx (+PNAMES) editor
and the ZDoom `TEXTURES` editor. Paletted assets convert to RGBA at load (index-0
transparency, `grAb` offsets preserved). See `docs/design/06-graphics-texture-editor.md`.
