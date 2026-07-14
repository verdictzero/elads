// SPDX-License-Identifier: GPL-3.0-or-later
#include "mapeditor/model/map_save.h"

#include <cctype>
#include <stdexcept>

#include "mapeditor/model/doom_map_io.h"
#include "mapeditor/model/udmf.h"

namespace elads::map {
namespace {

std::string upper(std::string s) {
    for (char& c : s)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

// End (exclusive) of the contiguous map-data lump run starting at `marker`+1.
size_t mapDataEnd(const std::vector<archive::Lump>& L, int marker) {
    size_t end = static_cast<size_t>(marker) + 1;
    while (end < L.size() && isMapDataLump(L[end].name))
        ++end;
    return end;
}

int findMarker(const archive::Wad& wad, const std::string& mapName) {
    const std::string want = upper(mapName);
    for (const MapEntry& e : findMaps(wad))
        if (upper(e.name) == want)
            return e.marker;
    return -1;
}

std::vector<archive::Lump> serializeMap(const MapModel& m, bool udmf, const std::string& ns) {
    if (!udmf)
        return writeDoomMap(m);
    std::vector<archive::Lump> out;
    const std::string text = writeUdmf(m, ns);
    archive::Lump textmap;
    textmap.name = "TEXTMAP";
    textmap.data.assign(text.begin(), text.end());
    out.push_back(std::move(textmap));
    out.push_back(archive::Lump{"ENDMAP", {}}); // UDMF maps are terminated by ENDMAP
    return out;
}

} // namespace

MapModel loadMapFromWad(const archive::Wad& wad, const std::string& mapName) {
    const std::string want = upper(mapName);
    for (const MapEntry& e : findMaps(wad)) {
        if (upper(e.name) != want)
            continue;
        const std::vector<archive::Lump> lumps = mapLumps(wad, e.marker);
        if (e.udmf) {
            for (const archive::Lump& l : lumps)
                if (upper(l.name) == "TEXTMAP")
                    return parseUdmf(std::string(l.data.begin(), l.data.end())).model;
            throw std::runtime_error("UDMF map '" + mapName + "' has no TEXTMAP lump");
        }
        return readDoomMap(lumps);
    }
    throw std::runtime_error("map '" + mapName + "' not found");
}

void saveMapToWad(archive::Wad& wad, const std::string& mapName, const MapModel& model, bool udmf,
                  const std::string& namespaceId) {
    const int marker = findMarker(wad, mapName);

    std::vector<archive::Lump> newLumps;
    if (udmf) {
        newLumps = serializeMap(model, true, namespaceId);
    } else {
        // Preserve the existing binary format (Doom vs Hexen) and, for Hexen, keep the compiled
        // ACS BEHAVIOR lump so an edit+save doesn't downgrade or blank it.
        MapFormat fmt = MapFormat::Doom;
        util::Bytes behavior;
        if (marker >= 0) {
            const std::vector<archive::Lump> existing = mapLumps(wad, marker);
            fmt = detectMapFormat(existing);
            for (const archive::Lump& l : existing)
                if (upper(l.name) == "BEHAVIOR")
                    behavior = l.data;
        }
        newLumps = writeMap(model, fmt);
        if (fmt == MapFormat::Hexen && !behavior.empty())
            for (archive::Lump& l : newLumps)
                if (l.name == "BEHAVIOR")
                    l.data = behavior;
    }

    std::vector<archive::Lump>& L = wad.lumps();

    if (marker < 0) {
        // Append a fresh map: marker lump followed by its data lumps.
        wad.add(mapName);
        for (const archive::Lump& nl : newLumps)
            wad.add(nl.name, nl.data);
        return;
    }

    // Replace the existing map-data run [begin, end) in place, keeping the marker + other lumps.
    const size_t begin = static_cast<size_t>(marker) + 1;
    const size_t end = mapDataEnd(L, marker);
    L.erase(L.begin() + static_cast<std::ptrdiff_t>(begin),
            L.begin() + static_cast<std::ptrdiff_t>(end));
    L.insert(L.begin() + static_cast<std::ptrdiff_t>(begin), newLumps.begin(), newLumps.end());
}

} // namespace elads::map
