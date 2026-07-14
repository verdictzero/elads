// SPDX-License-Identifier: GPL-3.0-or-later
// elads — sector visual styling read from UDMF keys (GUI/GL-free, header-only).
//
// GZDoom colours a sector's lighting with `lightcolor` (and fogs it with `fadecolor`). elads keeps
// unmodelled UDMF keys in each object's `extra` list (lossless round-trip), so these read straight
// from there — no model/serialiser change, and writing back stays byte-identical. Promoting them
// to typed fields is a later step; for now the 3D view multiplies surface tints by the sector's
// light colour. See docs/design/11-udmf-advanced.md §"Colours & fog" and the A3 plan item.
#pragma once

#include <cmath>
#include <cstdlib>
#include <string>

#include "mapeditor/model/map_objects.h"
#include "mapeditor/model/tex_align.h"

namespace elads::map {

struct ColorRGB {
    float r = 1.f, g = 1.f, b = 1.f;
};

// Unpack a 0xRRGGBB integer into a normalized colour.
inline ColorRGB colorFromInt(long v) {
    return {static_cast<float>((v >> 16) & 0xFF) / 255.f,
            static_cast<float>((v >> 8) & 0xFF) / 255.f,
            static_cast<float>(v & 0xFF) / 255.f};
}

// Find `key` in `extra` and parse its value as an integer colour (decimal or 0x-hex, as UDMF
// allows). Returns `def` if the key is absent.
inline ColorRGB readColorKey(const KeyVals& extra, const std::string& key, ColorRGB def = {}) {
    for (const auto& kv : extra)
        if (kv.first == key)
            return colorFromInt(std::strtol(kv.second.c_str(), nullptr, 0));
    return def;
}

// A sector's light colour (`lightcolor`), white when unset.
inline ColorRGB sectorLightColor(const Sector& s) {
    return readColorKey(s.extra, "lightcolor", ColorRGB{1.f, 1.f, 1.f});
}

// Read a floating-point value for `key` from `extra`, or `def` if absent.
inline double readDoubleKey(const KeyVals& extra, const std::string& key, double def) {
    for (const auto& kv : extra)
        if (kv.first == key)
            return std::strtod(kv.second.c_str(), nullptr);
    return def;
}

// A sector's floor/ceiling flat transform from the UDMF pan/scale/rotation keys (defaults =
// identity). `floor` picks the `*floor` vs `*ceiling` key set. A zero scale falls back to 1.
inline FlatXform sectorFlatXform(const Sector& s, bool floor) {
    const char* suffix = floor ? "floor" : "ceiling";
    auto key = [&](const char* base) { return std::string(base) + suffix; };
    FlatXform t;
    t.panX = readDoubleKey(s.extra, key("xpanning"), 0.0);
    t.panY = readDoubleKey(s.extra, key("ypanning"), 0.0);
    t.scaleX = readDoubleKey(s.extra, key("xscale"), 1.0);
    t.scaleY = readDoubleKey(s.extra, key("yscale"), 1.0);
    t.rotRad = readDoubleKey(s.extra, key("rotation"), 0.0) * M_PI / 180.0;
    if (t.scaleX == 0.0) t.scaleX = 1.0;
    if (t.scaleY == 0.0) t.scaleY = 1.0;
    return t;
}

} // namespace elads::map
