// SPDX-License-Identifier: GPL-3.0-or-later
// elads — classic Doom binary map (de)serialization + map-marker discovery.
//
// Reads/writes the editable Doom map lumps (THINGS/LINEDEFS/SIDEDEFS/VERTEXES/SECTORS)
// to and from a MapModel. Build lumps (SEGS/SSECTORS/NODES/REJECT/BLOCKMAP) are produced
// later by the node builder (see docs/design/07-build-test-pipeline.md), not here.
// Byte layouts: docs/design/08-formats-reference.md §3.
#pragma once

#include <string>
#include <vector>

#include "archive/wad.h"
#include "mapeditor/model/map_model.h"

namespace elads::map {

// True for lump names that belong to a map group (classic + Hexen + UDMF markers).
bool isMapDataLump(const std::string& name);

// A discovered map: its marker lump name, the marker's index, and whether it's UDMF.
struct MapEntry {
    std::string name;
    int marker = -1;
    bool udmf = false;
};

// Scan a WAD for map marker lumps (a name immediately followed by THINGS or TEXTMAP).
std::vector<MapEntry> findMaps(const archive::Wad&);

// The lumps belonging to the map at `marker` (marker+1 up to the first non-map lump).
std::vector<archive::Lump> mapLumps(const archive::Wad&, int marker);

// Build a MapModel from a classic Doom map's lumps (looked up by name within `lumps`).
MapModel readDoomMap(const std::vector<archive::Lump>& lumps);

// Serialize to the 5 editable Doom lumps, in canonical order.
std::vector<archive::Lump> writeDoomMap(const MapModel&);

} // namespace elads::map
