// SPDX-License-Identifier: GPL-3.0-or-later
// elads — sector floor/ceiling triangulation.
//
// Extracts a sector's boundary loops from its linedefs/sidedefs and triangulates them with
// earcut (ADR-0008). The result feeds the 2D sector fill and the 3D flats (see
// docs/design/04-map-editor.md §2.2.3). As the design notes, loop extraction — not earcut —
// is the tricky part; this handles simple/holed sectors, not pathological self-touching ones.
#pragma once

#include <cstdint>
#include <vector>

#include "mapeditor/model/map_model.h"
#include "util/geometry.h"

namespace elads::map {

struct Triangulation {
    std::vector<util::Vec2> points;   // ring vertices, in earcut index order
    std::vector<uint32_t> indices;    // triangle indices into `points` (multiple of 3)

    size_t triangleCount() const { return indices.size() / 3; }
    double area() const;              // total (absolute) triangle area
};

// Triangulate the floor/ceiling polygon of sector `sectorIndex`. Empty if the sector has no
// usable boundary.
Triangulation triangulateSector(const MapModel&, int sectorIndex);

} // namespace elads::map
