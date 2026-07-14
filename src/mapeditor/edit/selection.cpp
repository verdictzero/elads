// SPDX-License-Identifier: GPL-3.0-or-later
#include "mapeditor/edit/selection.h"

#include "mapeditor/model/planes.h" // sectorAt

namespace elads::edit {

Selection pickOfType(const map::MapModel& m, util::Vec2 p, double pickRadius, ObjType only) {
    switch (only) {
        case ObjType::Vertex: {
            const int i = m.nearestVertex(p, pickRadius);
            return i == map::kNoRef ? Selection{} : Selection{ObjType::Vertex, i};
        }
        case ObjType::Thing: {
            const int i = m.nearestThing(p, pickRadius);
            return i == map::kNoRef ? Selection{} : Selection{ObjType::Thing, i};
        }
        case ObjType::Linedef: {
            const int i = m.nearestLinedef(p, pickRadius);
            return i == map::kNoRef ? Selection{} : Selection{ObjType::Linedef, i};
        }
        case ObjType::Sector: {
            const int i = map::sectorAt(m, p.x, p.y);
            return i == map::kNoRef ? Selection{} : Selection{ObjType::Sector, i};
        }
        default:
            return {};
    }
}

Selection pick(const map::MapModel& m, util::Vec2 p, double pickRadius) {
    // Precedence: vertex > thing > linedef > enclosing sector.
    if (Selection s = pickOfType(m, p, pickRadius, ObjType::Vertex); !s.empty())
        return s;
    if (Selection s = pickOfType(m, p, pickRadius, ObjType::Thing); !s.empty())
        return s;
    if (Selection s = pickOfType(m, p, pickRadius, ObjType::Linedef); !s.empty())
        return s;
    return pickOfType(m, p, pickRadius, ObjType::Sector);
}

} // namespace elads::edit
