// SPDX-License-Identifier: GPL-3.0-or-later
// elads — build a MaterialSet (textures + flats as RGBA) from a WAD.
//
// Resolves PLAYPAL, decodes PNAMES patches, assembles TEXTURE1/TEXTURE2 composites, and
// decodes flats between F_START/F_END. Reuses the tested graphics codecs. GL-free.
#pragma once

#include "archive/wad.h"
#include "graphics/material_set.h"

namespace elads::gfx {

MaterialSet buildMaterialSetFromWad(const archive::Wad&);

} // namespace elads::gfx
