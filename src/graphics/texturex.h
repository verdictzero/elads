// SPDX-License-Identifier: GPL-3.0-or-later
// elads — composite wall textures: PNAMES + TEXTURE1/TEXTURE2 parse/write.
//
// A composite texture assembles named patches at offsets into a width x height canvas.
// PNAMES is the shared patch-name table that TEXTUREx entries index. Byte layout:
// docs/design/08-formats-reference.md §6.
#pragma once

#include <string>
#include <vector>

#include "util/byte_io.h"

namespace elads::gfx {

struct PatchRef {
    int originX = 0;
    int originY = 0;
    int patchIndex = 0; // index into PNAMES
};

struct TextureDef {
    std::string name;
    int width = 0;
    int height = 0;
    std::vector<PatchRef> patches;
};

// PNAMES: int32 count, then count x 8-char patch names (uppercase).
std::vector<std::string> parsePnames(const util::Bytes&);
util::Bytes writePnames(const std::vector<std::string>&);

// TEXTURE1/TEXTURE2: int32 count, int32 offsets[count], then the texture records.
std::vector<TextureDef> parseTextureX(const util::Bytes&);
util::Bytes writeTextureX(const std::vector<TextureDef>&);

} // namespace elads::gfx
