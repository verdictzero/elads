// SPDX-License-Identifier: GPL-3.0-or-later
// elads — 256-color palette (PLAYPAL) support.
//
// Doom graphics are palette indices; a Palette converts them to RGB and (for importing
// PNGs) finds the nearest index for an RGB color. See docs/design/08-formats-reference.md §9.
#pragma once

#include <array>
#include <cstdint>

#include "util/byte_io.h"

namespace elads::gfx {

struct Rgb {
    uint8_t r = 0, g = 0, b = 0;
};
inline bool operator==(const Rgb& a, const Rgb& b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

class Palette {
public:
    Palette() = default;

    // Load palette `index` from a PLAYPAL lump (768 bytes per palette). Throws if too short.
    static Palette fromPlaypal(const util::Bytes& data, int index = 0);

    // A deterministic non-trivial palette for tests/tools: color(i) = (i, 255-i, (i*2)&255).
    static Palette testRamp();

    const Rgb& color(int i) const { return colors_[static_cast<size_t>(i) & 0xFF]; }
    void setColor(int i, Rgb c) { colors_[static_cast<size_t>(i) & 0xFF] = c; }

    // Nearest palette index to an RGB color (squared-distance search). Used for import.
    uint8_t nearest(uint8_t r, uint8_t g, uint8_t b) const;

    util::Bytes toPlaypal() const; // 768 bytes

private:
    std::array<Rgb, 256> colors_{};
};

} // namespace elads::gfx
