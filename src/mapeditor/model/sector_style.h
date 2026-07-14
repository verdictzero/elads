// SPDX-License-Identifier: GPL-3.0-or-later
// elads — sector visual styling read from UDMF keys (GUI/GL-free, header-only).
//
// GZDoom colours a sector's lighting with `lightcolor` (and fogs it with `fadecolor`). elads keeps
// unmodelled UDMF keys in each object's `extra` list (lossless round-trip), so these read straight
// from there — no model/serialiser change, and writing back stays byte-identical. Promoting them
// to typed fields is a later step; for now the 3D view multiplies surface tints by the sector's
// light colour. See docs/design/11-udmf-advanced.md §"Colours & fog" and the A3 plan item.
#pragma once

#include <cstdlib>
#include <string>

#include "mapeditor/model/map_objects.h"

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

} // namespace elads::map
