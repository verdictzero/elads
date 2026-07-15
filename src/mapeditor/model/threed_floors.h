// SPDX-License-Identifier: GPL-3.0-or-later
// elads — GZDoom 3D floors (Sector_Set3DFloor) → slab geometry (GUI/GL-free).
//
// A 3D floor is a solid slab floating inside a target sector, defined by a *control* sector
// elsewhere in the map. A linedef in the control sector carries special 160 (Sector_Set3DFloor);
// its arg0 tags the target sector(s). The slab spans the control sector's floor..ceiling and takes
// its top/bottom flats from the control sector and its side texture from the 160 line. This module
// resolves those relationships into a flat list the 3D view can draw; see
// docs/design/11-udmf-advanced.md §"3D floors" and the A4 plan item.
#pragma once

#include <string>
#include <vector>

#include "mapeditor/model/map_model.h"

namespace elads::map {

constexpr int kSpecialSet3DFloor = 160;

struct ThreeDFloor {
    int targetSector = kNoRef;   // sector that receives the slab
    double topZ = 0.0;           // slab top (>= botZ)
    double botZ = 0.0;           // slab bottom
    std::string texTop, texBot;  // control sector's ceiling/floor flats
    std::string texSide;         // the 160 line's side texture
    int type = 1;                // Sector_Set3DFloor arg1 (1 solid, 2 swimmable, 3 non-solid, …)
    int alpha = 255;             // arg3 translucency (rendered opaque in this first cut)
};

// One ThreeDFloor per (Sector_Set3DFloor line × matching-tag target sector). Tags are matched on
// arg0 (values > 255 via arg4's high byte are not yet handled); a control sector is never its own
// target. topZ/botZ are ordered so topZ >= botZ regardless of the control sector's orientation.
std::vector<ThreeDFloor> compute3DFloors(const MapModel&);

} // namespace elads::map
