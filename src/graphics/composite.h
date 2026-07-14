// SPDX-License-Identifier: GPL-3.0-or-later
// elads — composite a TEXTUREx definition into a single RGBA image.
//
// Assembles a wall texture by blitting its named patches at their offsets. The map editor's
// material lookup consumes these bitmaps. Patch lookup is injected (a resolver) so this stays
// decoupled from the archive. See docs/design/06-graphics-texture-editor.md.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "graphics/image.h"
#include "graphics/texturex.h"

namespace elads::gfx {

// Resolve a PNAMES patch name to its decoded RGBA image, or nullptr if unavailable.
using PatchLookup = std::function<const Image*(const std::string& patchName)>;

// Copy the opaque pixels of `src` into `dst` at (atX, atY), clipped to dst bounds.
void blit(Image& dst, const Image& src, int atX, int atY);

// Build the composite image for `def`, resolving patches via `pnames` + `lookup`.
// Missing patches are skipped (leaving transparency).
Image assembleTexture(const TextureDef& def, const std::vector<std::string>& pnames,
                      const PatchLookup& lookup);

} // namespace elads::gfx
