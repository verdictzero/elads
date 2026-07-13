// SPDX-License-Identifier: GPL-3.0-or-later
// elads — map geometry objects (GUI/GL-free).
//
// A single in-memory model serves Doom, Hexen, and UDMF maps; format-specific fields
// (Hexen/UDMF specials+args, thing z/tid) are always present and simply unused for
// plainer formats. See docs/design/03-data-model.md.
#pragma once

#include <array>
#include <string>
#include <utility>
#include <vector>

#include "util/geometry.h"

namespace elads::map {

constexpr int kNoRef = -1;                 // "no reference" index (e.g. one-sided linedef back)
constexpr const char* kNoTexture = "-";    // Doom's "no texture" sentinel

using Args = std::array<int, 5>;           // Hexen/UDMF special arguments

// Unknown/format-specific UDMF keys are preserved verbatim here so a load→save round-trip
// is lossless even for keys elads does not model with a typed field (see
// docs/design/03-data-model.md). Empty for classic (binary) maps.
using KeyVals = std::vector<std::pair<std::string, std::string>>;

struct Vertex {
    util::Vec2 pos;
    KeyVals extra;
};

struct Sidedef {
    int sector = kNoRef;
    int offsetX = 0;
    int offsetY = 0;
    std::string upper = kNoTexture;
    std::string middle = kNoTexture;
    std::string lower = kNoTexture;
    KeyVals extra;
};

struct Linedef {
    int v1 = kNoRef;
    int v2 = kNoRef;
    int front = kNoRef;     // sidedef index (right)
    int back = kNoRef;      // sidedef index (left); kNoRef => one-sided
    int flags = 0;
    int special = 0;        // 0 for plain Doom action-less lines
    int tag = 0;            // Doom sector tag (UDMF: id)
    Args args{};            // Hexen/UDMF
    KeyVals extra;

    bool twoSided() const { return back != kNoRef; }
};

struct Sector {
    int floorHeight = 0;
    int ceilHeight = 0;
    std::string floorTex;
    std::string ceilTex;
    int lightLevel = 160;
    int special = 0;
    int tag = 0;
    KeyVals extra;
};

struct Thing {
    util::Vec2 pos;
    double z = 0.0;         // Hexen/UDMF
    int angle = 0;
    int type = 0;           // editor number (DoomEdNum)
    int flags = 0;
    int tid = 0;            // Hexen/UDMF thing id
    int special = 0;        // Hexen/UDMF
    Args args{};
    KeyVals extra;
};

} // namespace elads::map
