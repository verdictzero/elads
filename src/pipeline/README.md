# pipeline — node build + compile + playtest

Orchestrates the ARM64-clean external toolchain (built by
`scripts/build-toolchain.sh`):

- **Node building** — embedded AJBSP (in-process standard/GL/XNOD) + bundled ZDBSP
  (XGL2/XGL3/UDMF for GZDoom). Config modeled on SLADE `nodebuilders.cfg`.
- **ACS** — `acc` (bcc/gdcc alternates), invoked out-of-process, only when a map uses ACS.
- **Playtest** — one-click GZDoom launch on its **GLES** backend
  (`gzdoom -iwad … -file … (-warp N|+map NAME) +vid_preferbackend 3`), capturing
  `-stdout` to surface script/asset errors. See `docs/design/07-build-test-pipeline.md`.
