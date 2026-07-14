// SPDX-License-Identifier: GPL-3.0-or-later
#include "graphics/composite.h"

namespace elads::gfx {

void blit(Image& dst, const Image& src, int atX, int atY) {
    for (int sy = 0; sy < src.height; ++sy) {
        const int dy = atY + sy;
        if (dy < 0 || dy >= dst.height)
            continue;
        for (int sx = 0; sx < src.width; ++sx) {
            const int dx = atX + sx;
            if (dx < 0 || dx >= dst.width)
                continue;
            if (!src.opaque(sx, sy))
                continue;
            const uint8_t* p = src.pixel(sx, sy);
            dst.set(dx, dy, p[0], p[1], p[2], p[3]);
        }
    }
}

Image assembleTexture(const TextureDef& def, const std::vector<std::string>& pnames,
                      const PatchLookup& lookup) {
    Image out(def.width, def.height); // transparent canvas
    for (const PatchRef& pr : def.patches) {
        if (pr.patchIndex < 0 || static_cast<size_t>(pr.patchIndex) >= pnames.size())
            continue;
        const Image* patch = lookup(pnames[static_cast<size_t>(pr.patchIndex)]);
        if (patch)
            blit(out, *patch, pr.originX, pr.originY);
    }
    return out;
}

} // namespace elads::gfx
