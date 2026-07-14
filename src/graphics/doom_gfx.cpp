// SPDX-License-Identifier: GPL-3.0-or-later
#include "graphics/doom_gfx.h"

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace elads::gfx {
namespace {
constexpr uint8_t kColumnEnd = 0xFF;
} // namespace

Image decodeDoomGfx(const util::Bytes& lump, const Palette& pal, PictureOffsets* offsets) {
    util::ByteReader r(lump);
    const int width = r.i16();
    const int height = r.i16();
    const int left = r.i16();
    const int top = r.i16();
    if (width <= 0 || height <= 0 || width > 4096 || height > 4096)
        throw std::runtime_error("Doom gfx: implausible dimensions");
    if (offsets)
        *offsets = {left, top};

    std::vector<uint32_t> colOfs(static_cast<size_t>(width));
    for (int x = 0; x < width; ++x)
        colOfs[static_cast<size_t>(x)] = r.u32();

    Image img(width, height); // starts fully transparent
    for (int x = 0; x < width; ++x) {
        util::ByteReader c(lump);
        c.seek(colOfs[static_cast<size_t>(x)]);
        for (;;) {
            const uint8_t topDelta = c.u8();
            if (topDelta == kColumnEnd)
                break;
            const uint8_t length = c.u8();
            c.u8(); // unused pad byte before pixels
            for (uint8_t i = 0; i < length; ++i) {
                const uint8_t idx = c.u8();
                const int y = topDelta + i;
                if (y >= 0 && y < height) {
                    const Rgb col = pal.color(idx);
                    img.set(x, y, col.r, col.g, col.b, 255);
                }
            }
            c.u8(); // unused pad byte after pixels
        }
    }
    return img;
}

util::Bytes encodeDoomGfx(const Image& img, const Palette& pal, PictureOffsets offsets) {
    const int width = img.width;
    const int height = img.height;

    // Build each column's post data independently.
    std::vector<util::Bytes> columns(static_cast<size_t>(width));
    for (int x = 0; x < width; ++x) {
        util::ByteWriter col;
        int y = 0;
        while (y < height) {
            if (!img.opaque(x, y)) {
                ++y;
                continue;
            }
            const int start = y;
            util::Bytes run;
            while (y < height && img.opaque(x, y) && (y - start) < 254) {
                const uint8_t* px = img.pixel(x, y);
                run.push_back(pal.nearest(px[0], px[1], px[2]));
                ++y;
            }
            col.u8(static_cast<uint8_t>(start)); // topdelta (assumes start <= 254)
            col.u8(static_cast<uint8_t>(run.size()));
            col.u8(0); // unused pad
            col.bytes(run);
            col.u8(0); // unused pad
        }
        col.u8(kColumnEnd);
        columns[static_cast<size_t>(x)] = col.take();
    }

    // Header + column-offset table + column data.
    util::ByteWriter w;
    w.i16(static_cast<int16_t>(width));
    w.i16(static_cast<int16_t>(height));
    w.i16(static_cast<int16_t>(offsets.left));
    w.i16(static_cast<int16_t>(offsets.top));

    const uint32_t tableStart = static_cast<uint32_t>(w.size());
    for (int x = 0; x < width; ++x)
        w.u32(0); // placeholders, patched below

    for (int x = 0; x < width; ++x) {
        const uint32_t here = static_cast<uint32_t>(w.size());
        w.patchU32(tableStart + static_cast<size_t>(x) * 4, here);
        w.bytes(columns[static_cast<size_t>(x)]);
    }
    return w.take();
}

bool looksLikeDoomGfx(const util::Bytes& lump) {
    if (lump.size() < 8)
        return false;
    try {
        util::ByteReader r(lump);
        const int width = r.i16();
        const int height = r.i16();
        r.i16();
        r.i16();
        if (width <= 0 || height <= 0 || width > 4096 || height > 2048)
            return false;
        const size_t need = 8 + static_cast<size_t>(width) * 4;
        if (lump.size() < need)
            return false;
        for (int x = 0; x < width; ++x) {
            const uint32_t off = r.u32();
            if (off >= lump.size())
                return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace elads::gfx
