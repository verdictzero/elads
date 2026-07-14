// SPDX-License-Identifier: GPL-3.0-or-later
// elads — PNG encoding (via vendored miniz).
//
// Used to save rendered frames/thumbnails from the headless renderer and to export images
// from the graphics editor. See docs/design/02-render-abstraction.md §6 (offscreen output).
#pragma once

#include "graphics/image.h"
#include "util/byte_io.h"

namespace elads::gfx {

// Encode an RGBA8 image to PNG bytes. Throws std::runtime_error on failure.
util::Bytes encodePng(const Image&);

} // namespace elads::gfx
