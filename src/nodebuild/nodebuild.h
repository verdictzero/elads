// SPDX-License-Identifier: GPL-3.0-or-later
// elads — embedded BSP node builder (vanilla Doom format).
//
// Builds the map "build lumps" a source port needs to run a map — SEGS, SSECTORS, NODES,
// BLOCKMAP, REJECT — plus a VERTEXES lump augmented with any split points, straight from a
// MapModel, with no external tool. It's a self-contained recursive BSP builder for well-formed
// maps (the option in ADR-0007 to embed a node builder rather than shell out to AJBSP/ZDBSP; the
// external toolchain remains for pathological maps). Classic 16-bit lumps, so it targets the
// vanilla node format every source port accepts. See docs/design/07-build-test-pipeline.md.
#pragma once

#include <vector>

#include "archive/wad.h"
#include "mapeditor/model/doom_map_io.h" // MapFormat
#include "mapeditor/model/map_model.h"

namespace elads::nodebuild {

struct BuildStats {
    int segs = 0;
    int subsectors = 0;
    int nodes = 0;
    int splitVertices = 0; // vertices the builder added while partitioning
};

struct BuildResult {
    // The six lumps to place after the map marker: an updated VERTEXES (originals + split points)
    // followed by SEGS, SSECTORS, NODES, REJECT, BLOCKMAP.
    std::vector<archive::Lump> lumps;
    BuildStats stats;
};

// Build nodes for `model`. Throws std::runtime_error on a degenerate map (no linedefs/sectors).
BuildResult buildNodes(const map::MapModel& model);

// A complete, playable map lump set in canonical order — the editable lumps (in `format`) with the
// builder's augmented VERTEXES and SEGS/SSECTORS/NODES/REJECT/BLOCKMAP — ready to place after a map
// marker in a WAD. (Hexen adds its BEHAVIOR lump after BLOCKMAP.)
std::vector<archive::Lump> buildMapLumps(const map::MapModel& model,
                                         map::MapFormat format = map::MapFormat::Doom);

} // namespace elads::nodebuild
