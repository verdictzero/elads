// SPDX-License-Identifier: GPL-3.0-or-later
#include "graphics/palette.h"

#include <stdexcept>

namespace elads::gfx {

Palette Palette::fromPlaypal(const util::Bytes& data, int index) {
    const size_t base = static_cast<size_t>(index) * 768;
    if (base + 768 > data.size())
        throw std::runtime_error("PLAYPAL too short for palette index " + std::to_string(index));
    Palette p;
    for (int i = 0; i < 256; ++i)
        p.colors_[static_cast<size_t>(i)] = {data[base + i * 3], data[base + i * 3 + 1],
                                             data[base + i * 3 + 2]};
    return p;
}

Palette Palette::testRamp() {
    Palette p;
    for (int i = 0; i < 256; ++i)
        p.colors_[static_cast<size_t>(i)] = {static_cast<uint8_t>(i), static_cast<uint8_t>(255 - i),
                                             static_cast<uint8_t>((i * 2) & 0xFF)};
    return p;
}

uint8_t Palette::nearest(uint8_t r, uint8_t g, uint8_t b) const {
    int best = 0;
    long bestDist = -1;
    for (int i = 0; i < 256; ++i) {
        const Rgb& c = colors_[static_cast<size_t>(i)];
        const long dr = static_cast<long>(r) - c.r;
        const long dg = static_cast<long>(g) - c.g;
        const long db = static_cast<long>(b) - c.b;
        const long d = dr * dr + dg * dg + db * db;
        if (bestDist < 0 || d < bestDist) {
            bestDist = d;
            best = i;
            if (d == 0)
                break;
        }
    }
    return static_cast<uint8_t>(best);
}

util::Bytes Palette::toPlaypal() const {
    util::Bytes out;
    out.reserve(768);
    for (const Rgb& c : colors_) {
        out.push_back(c.r);
        out.push_back(c.g);
        out.push_back(c.b);
    }
    return out;
}

} // namespace elads::gfx
