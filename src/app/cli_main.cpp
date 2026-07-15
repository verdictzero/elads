// SPDX-License-Identifier: GPL-3.0-or-later
// elads — command-line front-end over the GUI/GL-free core.
//
// A small, scriptable tool that exercises the archive + map-model layers without any
// GUI/OpenGL. Grows into the batch/automation surface described in
// docs/design/05-text-script-editor.md as the core matures.
//
//   elads wad-info <file.wad>            list lumps and detected maps
//   elads map-info <file.wad> <MAPNAME>  parse a map and print its stats
//   elads demo-wad <out.wad>             write a sample PWAD (binary + UDMF maps)
#include <cstdio>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

#include "archive/entry_type.h"
#include "archive/pk3.h"
#include "archive/wad.h"
#include "mapeditor/model/doom_map_io.h"
#include "mapeditor/model/map_save.h"
#include "mapeditor/model/udmf.h"
#include "nodebuild/nodebuild.h"

using namespace elads;

namespace {

util::Bytes readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f)
        throw std::runtime_error("cannot open '" + path + "'");
    return util::Bytes(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

void writeFile(const std::string& path, const util::Bytes& data) {
    std::ofstream f(path, std::ios::binary);
    if (!f)
        throw std::runtime_error("cannot write '" + path + "'");
    f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

// A 64x64 one-sector room with a player-1 start — the smallest useful sample map.
map::MapModel sampleRoom() {
    map::MapModel m;
    m.addVertex({0, 0});
    m.addVertex({64, 0});
    m.addVertex({64, 64});
    m.addVertex({0, 64});
    map::Sector sec;
    sec.ceilHeight = 128;
    sec.floorTex = "FLOOR4_8";
    sec.ceilTex = "CEIL3_5";
    sec.lightLevel = 176;
    m.addSector(sec);
    const int e[4][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}};
    for (int i = 0; i < 4; ++i) {
        map::Sidedef sd;
        sd.sector = 0;
        sd.middle = "STARTAN2";
        m.addSidedef(sd);
        map::Linedef l;
        l.v1 = e[i][0];
        l.v2 = e[i][1];
        l.front = i;
        l.flags = 1;
        m.addLinedef(l);
    }
    map::Thing p1;
    p1.pos = {32, 32};
    p1.type = 1;
    p1.angle = 90;
    p1.flags = 7;
    m.addThing(p1);
    return m;
}

int demoWad(const std::string& path) {
    const map::MapModel room = sampleRoom();
    archive::Wad wad;
    wad.setType(archive::WadType::Pwad);

    // MAP01: classic Doom binary format.
    wad.add("MAP01");
    for (auto& l : map::writeDoomMap(room))
        wad.add(l.name, l.data);

    // MAP02: UDMF text format.
    wad.add("MAP02");
    const std::string textmap = map::writeUdmf(room, "zdoom");
    wad.add("TEXTMAP", util::Bytes(textmap.begin(), textmap.end()));
    wad.add("ENDMAP");

    writeFile(path, wad.write());
    std::printf("wrote %s (%zu lumps: MAP01 binary, MAP02 UDMF)\n", path.c_str(), wad.lumpCount());
    return 0;
}

int demoPk3(const std::string& path) {
    archive::Pk3 pk3;
    pk3.add("MAPINFO", []() {
        const char* s = "map MAP01 \"Demo\"\n{\n}\n";
        return util::Bytes(reinterpret_cast<const uint8_t*>(s),
                           reinterpret_cast<const uint8_t*>(s) + std::char_traits<char>::length(s));
    }());
    pk3.add("maps/MAP01.txt", util::Bytes{'h', 'i'});
    pk3.add("textures/BRICK.dat", util::Bytes{1, 2, 3, 4});
    writeFile(path, pk3.write(true));
    std::printf("wrote %s (%zu entries)\n", path.c_str(), pk3.entryCount());
    return 0;
}

int pk3Info(const std::string& path) {
    const archive::Pk3 pk3 = archive::Pk3::read(readFile(path));
    std::printf("%s: %zu entries\n", path.c_str(), pk3.entryCount());
    for (const auto& e : pk3.entries()) {
        const auto type = archive::detectEntryType(e.path, e.data);
        std::printf("  %-24s %8zu bytes  ns=%-9s %s\n", e.path.c_str(), e.data.size(),
                    archive::pk3Namespace(e.path).c_str(), archive::entryTypeName(type));
    }
    return 0;
}

int usage() {
    std::puts("elads — Doom dev environment (CLI core)\n");
    std::puts("usage:");
    std::puts("  elads wad-info <file.wad>");
    std::puts("  elads lump-types <file.wad>");
    std::puts("  elads map-info <file.wad> <MAPNAME>");
    std::puts("  elads build-nodes <in.wad> <MAPNAME> <out.wad>");
    std::puts("  elads demo-wad <out.wad>");
    std::puts("  elads pk3-info <file.pk3>");
    std::puts("  elads demo-pk3 <out.pk3>");
    return 2;
}

// Load a binary map from a WAD, build its nodes with the embedded builder, and write a new WAD
// with the same map made playable (editable lumps + SEGS/SSECTORS/NODES/REJECT/BLOCKMAP).
int buildNodes(const std::string& inPath, const std::string& mapName, const std::string& outPath) {
    archive::Wad wad = archive::Wad::read(readFile(inPath));
    int marker = -1;
    map::MapFormat fmt = map::MapFormat::Doom;
    for (const auto& e : map::findMaps(wad)) {
        if (e.name != mapName)
            continue;
        if (e.udmf)
            throw std::runtime_error("build-nodes: '" + mapName + "' is UDMF; only binary maps are supported");
        marker = e.marker;
        fmt = map::detectMapFormat(map::mapLumps(wad, e.marker));
        break;
    }
    if (marker < 0)
        throw std::runtime_error("map '" + mapName + "' not found");

    const map::MapModel model = map::loadMapFromWad(wad, mapName);
    const std::vector<archive::Lump> built = nodebuild::buildMapLumps(model, fmt);

    // Replace the map's data run [marker+1, end) with the freshly built, playable lumps.
    auto& L = wad.lumps();
    size_t begin = static_cast<size_t>(marker) + 1, end = begin;
    while (end < L.size() && map::isMapDataLump(L[end].name))
        ++end;
    L.erase(L.begin() + static_cast<std::ptrdiff_t>(begin),
            L.begin() + static_cast<std::ptrdiff_t>(end));
    L.insert(L.begin() + static_cast<std::ptrdiff_t>(begin), built.begin(), built.end());

    writeFile(outPath, wad.write());
    const nodebuild::BuildResult stats = nodebuild::buildNodes(model);
    std::printf("built %s: %d segs, %d subsectors, %d nodes, %d split verts -> %s\n",
                mapName.c_str(), stats.stats.segs, stats.stats.subsectors, stats.stats.nodes,
                stats.stats.splitVertices, outPath.c_str());
    return 0;
}

int wadInfo(const std::string& path) {
    const archive::Wad wad = archive::Wad::read(readFile(path));
    std::printf("%s: %s, %zu lumps\n", path.c_str(),
                wad.type() == archive::WadType::Iwad ? "IWAD" : "PWAD", wad.lumpCount());
    for (size_t i = 0; i < wad.lumpCount(); ++i)
        std::printf("  [%4zu] %-8s %8zu bytes\n", i, wad.lumps()[i].name.c_str(),
                    wad.lumps()[i].data.size());

    const auto maps = map::findMaps(wad);
    std::printf("maps: %zu\n", maps.size());
    for (const auto& e : maps)
        std::printf("  %-8s (%s)\n", e.name.c_str(), e.udmf ? "UDMF" : "binary");
    return 0;
}

void printMapStats(const map::MapModel& m, const char* fmt) {
    const util::BBox b = m.bounds();
    std::printf("  format:   %s\n", fmt);
    std::printf("  vertices: %zu\n", m.vertexCount());
    std::printf("  linedefs: %zu\n", m.linedefCount());
    std::printf("  sidedefs: %zu\n", m.sidedefCount());
    std::printf("  sectors:  %zu\n", m.sectorCount());
    std::printf("  things:   %zu\n", m.thingCount());
    if (b.valid())
        std::printf("  bounds:   [%g, %g] .. [%g, %g]  (%g x %g)\n", b.minX, b.minY, b.maxX,
                    b.maxY, b.width(), b.height());
}

int lumpTypes(const std::string& path) {
    const archive::Wad wad = archive::Wad::read(readFile(path));
    for (size_t i = 0; i < wad.lumpCount(); ++i) {
        const auto& l = wad.lumps()[i];
        const auto type = archive::detectEntryType(l.name, l.data);
        std::printf("  [%4zu] %-8s %8zu bytes  %s\n", i, l.name.c_str(), l.data.size(),
                    archive::entryTypeName(type));
    }
    return 0;
}

int mapInfo(const std::string& path, const std::string& mapName) {
    const archive::Wad wad = archive::Wad::read(readFile(path));
    for (const auto& e : map::findMaps(wad)) {
        if (e.name != mapName) // exact match (marker names are already canonical-cased in WADs)
            continue;
        const auto lumps = map::mapLumps(wad, e.marker);
        std::printf("%s in %s:\n", mapName.c_str(), path.c_str());
        if (e.udmf) {
            const archive::Lump* tm = nullptr;
            for (const auto& l : lumps)
                if (l.name == "TEXTMAP")
                    tm = &l;
            if (!tm)
                throw std::runtime_error("UDMF map has no TEXTMAP lump");
            const std::string text(tm->data.begin(), tm->data.end());
            const map::UdmfMap um = map::parseUdmf(text);
            std::printf("  namespace: %s\n", um.namespaceId.c_str());
            printMapStats(um.model, "UDMF");
        } else {
            printMapStats(map::readDoomMap(lumps), "Doom (binary)");
        }
        return 0;
    }
    std::fprintf(stderr, "map '%s' not found in %s\n", mapName.c_str(), path.c_str());
    return 1;
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc >= 3 && std::string(argv[1]) == "wad-info")
            return wadInfo(argv[2]);
        if (argc >= 3 && std::string(argv[1]) == "lump-types")
            return lumpTypes(argv[2]);
        if (argc >= 4 && std::string(argv[1]) == "map-info")
            return mapInfo(argv[2], argv[3]);
        if (argc >= 5 && std::string(argv[1]) == "build-nodes")
            return buildNodes(argv[2], argv[3], argv[4]);
        if (argc >= 3 && std::string(argv[1]) == "demo-wad")
            return demoWad(argv[2]);
        if (argc >= 3 && std::string(argv[1]) == "pk3-info")
            return pk3Info(argv[2]);
        if (argc >= 3 && std::string(argv[1]) == "demo-pk3")
            return demoPk3(argv[2]);
        return usage();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
}
