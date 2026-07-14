// SPDX-License-Identifier: GPL-3.0-or-later
#include "graphics/flat.h"

#include <stdexcept>

namespace elads::gfx {

FlatDims inferFlatDimensions(size_t size) {
    switch (size) {
        case 4096:  return {64, 64, true};
        case 8192:  return {64, 128, true};
        case 64000: return {320, 200, true};
        case 65536: return {256, 256, true};
        default:    return {0, 0, false};
    }
}

Image decodeFlat(const util::Bytes& data, const Palette& pal, int width, int height) {
    const size_t need = static_cast<size_t>(width) * height;
    if (width <= 0 || height <= 0 || data.size() < need)
        throw std::runtime_error("decodeFlat: data too small for dimensions");
    Image img(width, height);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            const Rgb c = pal.color(data[static_cast<size_t>(y) * width + x]);
            img.set(x, y, c.r, c.g, c.b, 255);
        }
    return img;
}

util::Bytes encodeFlat(const Image& img, const Palette& pal) {
    util::Bytes out;
    out.reserve(static_cast<size_t>(img.width) * img.height);
    for (int y = 0; y < img.height; ++y)
        for (int x = 0; x < img.width; ++x) {
            const uint8_t* px = img.pixel(x, y);
            out.push_back(pal.nearest(px[0], px[1], px[2]));
        }
    return out;
}

} // namespace elads::gfx
