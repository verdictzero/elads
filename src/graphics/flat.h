// SPDX-License-Identifier: GPL-3.0-or-later
// elads — Doom "flat" (floor/ceiling texture) decode/encode.
//
// Flats are raw, headerless, fully-opaque width*height palette-index bytes; dimensions are
// inferred from the lump size (and namespace). See docs/design/08-formats-reference.md §8.
#pragma once

#include <cstddef>

#include "graphics/image.h"
#include "graphics/palette.h"
#include "util/byte_io.h"

namespace elads::gfx {

struct FlatDims {
    int width = 0;
    int height = 0;
    bool known = false;
};

// Map a raw flat lump size to canonical dimensions (4096=64x64, 8192=64x128,
// 64000=320x200, 65536=256x256). `known` is false for unrecognized sizes.
FlatDims inferFlatDimensions(size_t size);

// Decode a raw flat to an opaque RGBA image. Throws if data is smaller than width*height.
Image decodeFlat(const util::Bytes& data, const Palette&, int width, int height);

// Encode an image to a raw flat (nearest-color; alpha ignored — flats are opaque).
util::Bytes encodeFlat(const Image&, const Palette&);

} // namespace elads::gfx
