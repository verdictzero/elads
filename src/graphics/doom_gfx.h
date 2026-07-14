// SPDX-License-Identifier: GPL-3.0-or-later
// elads — Doom "picture" (patch/sprite/graphic) format decode + encode.
//
// The column/post format with transparency (index-0 gaps), used by patches, sprites, and
// menu graphics. Byte layout: docs/design/08-formats-reference.md §7. Decoded to RGBA via a
// Palette; encoding quantizes RGBA back to palette indices (alpha 0 => transparent).
//
// Limitation: assumes classic patches with height <= 254 (absolute topdelta). Tall-patch
// (cumulative-topdelta) support is a follow-up.
#pragma once

#include "graphics/image.h"
#include "graphics/palette.h"
#include "util/byte_io.h"

namespace elads::gfx {

struct PictureOffsets {
    int left = 0;
    int top = 0;
};

// Decode a Doom picture lump to an RGBA image (transparent where the columns have gaps).
Image decodeDoomGfx(const util::Bytes& lump, const Palette&, PictureOffsets* offsets = nullptr);

// Encode an RGBA image to the Doom picture format; alpha 0 => transparent.
util::Bytes encodeDoomGfx(const Image&, const Palette&, PictureOffsets offsets = {});

// True if the lump plausibly parses as a Doom picture of a sane size (used by entry-type
// detection). Conservative — validates header dimensions and column offsets in range.
bool looksLikeDoomGfx(const util::Bytes& lump);

} // namespace elads::gfx
