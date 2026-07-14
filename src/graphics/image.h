// SPDX-License-Identifier: GPL-3.0-or-later
// elads — a simple 32-bit RGBA image (GUI/GL-free).
//
// All decoded assets (paletted Doom gfx, PNG, flats) become RGBA8 in memory; the render
// backend uploads these as textures. See docs/design/06-graphics-texture-editor.md.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace elads::gfx {

struct Image {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba; // width*height*4, row-major, non-premultiplied

    Image() = default;
    Image(int w, int h) : width(w), height(h), rgba(static_cast<size_t>(w) * h * 4, 0) {}

    size_t index(int x, int y) const { return (static_cast<size_t>(y) * width + x) * 4; }

    void set(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        const size_t i = index(x, y);
        rgba[i] = r;
        rgba[i + 1] = g;
        rgba[i + 2] = b;
        rgba[i + 3] = a;
    }

    bool opaque(int x, int y) const { return rgba[index(x, y) + 3] != 0; }
    const uint8_t* pixel(int x, int y) const { return &rgba[index(x, y)]; }
};

inline bool operator==(const Image& a, const Image& b) {
    return a.width == b.width && a.height == b.height && a.rgba == b.rgba;
}

} // namespace elads::gfx
