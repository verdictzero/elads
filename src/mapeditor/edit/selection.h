// SPDX-License-Identifier: GPL-3.0-or-later
// elads — editor selection + picking (GUI/GL-free).
//
// A Selection names one map object (or none). pick() resolves a world-space point to the most
// specific object under it, using the editor's usual precedence (vertices, then things, then
// linedefs, then the enclosing sector). Screen->world unprojection lives with the cameras
// (view2d screenToWorld, view3d screenRay); this layer is pure model math so it is fully
// headless-testable. See docs/design/04-map-editor.md §2 and the B2 plan item.
#pragma once

#include "mapeditor/model/map_model.h"
#include "util/geometry.h"

namespace elads::edit {

enum class ObjType { None, Vertex, Linedef, Sidedef, Sector, Thing };

struct Selection {
    ObjType type = ObjType::None;
    int index = map::kNoRef;

    bool empty() const { return type == ObjType::None || index == map::kNoRef; }
    bool operator==(const Selection& o) const { return type == o.type && index == o.index; }
    bool operator!=(const Selection& o) const { return !(*this == o); }
};

// What object a click at world point `p` selects, given a pick radius (world units). Precedence:
// a vertex within radius wins; else a thing within radius; else a linedef within radius; else the
// sector whose polygon contains `p`; else nothing.
Selection pick(const map::MapModel&, util::Vec2 p, double pickRadius);

// Same, restricted to a single object kind (e.g. sector-only mode). Returns None if nothing of
// that kind qualifies.
Selection pickOfType(const map::MapModel&, util::Vec2 p, double pickRadius, ObjType only);

} // namespace elads::edit
