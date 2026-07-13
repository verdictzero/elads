// SPDX-License-Identifier: GPL-3.0-or-later
#include "mapeditor/model/doom_map_io.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>

#include "util/byte_io.h"

namespace elads::map {
namespace {

constexpr uint16_t kNoneRef16 = 0xFFFF; // Doom's "no sidedef" sentinel

std::string upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}
bool ieq(const std::string& a, const char* b) { return upper(a) == b; }

const archive::Lump* findLump(const std::vector<archive::Lump>& lumps, const char* name) {
    for (const auto& l : lumps)
        if (ieq(l.name, name))
            return &l;
    return nullptr;
}

int16_t toI16(double v) {
    return static_cast<int16_t>(std::lround(v));
}
int refTo16(int idx) { return idx < 0 ? kNoneRef16 : static_cast<uint16_t>(idx); }
int refFrom16(uint16_t v) { return v == kNoneRef16 ? kNoRef : static_cast<int>(v); }

} // namespace

bool isMapDataLump(const std::string& name) {
    static const char* kNames[] = {
        "THINGS",  "LINEDEFS", "SIDEDEFS", "VERTEXES", "SEGS",    "SSECTORS",
        "NODES",   "SECTORS",  "REJECT",   "BLOCKMAP", "BEHAVIOR", "SCRIPTS",
        "TEXTMAP", "ENDMAP",   "ZNODES",   "DIALOGUE", "GL_VERT",  "GL_SEGS",
        "GL_SSECT", "GL_NODES", "GL_PVS",
    };
    const std::string u = upper(name);
    for (const char* n : kNames)
        if (u == n)
            return true;
    return false;
}

std::vector<MapEntry> findMaps(const archive::Wad& wad) {
    std::vector<MapEntry> maps;
    const auto& L = wad.lumps();
    for (size_t i = 0; i + 1 < L.size(); ++i) {
        if (isMapDataLump(L[i].name))
            continue; // a marker is not itself a map-data lump
        const std::string next = upper(L[i + 1].name);
        if (next == "TEXTMAP")
            maps.push_back({L[i].name, static_cast<int>(i), true});
        else if (next == "THINGS" || next == "LINEDEFS")
            maps.push_back({L[i].name, static_cast<int>(i), false});
    }
    return maps;
}

std::vector<archive::Lump> mapLumps(const archive::Wad& wad, int marker) {
    std::vector<archive::Lump> out;
    const auto& L = wad.lumps();
    for (size_t i = static_cast<size_t>(marker) + 1; i < L.size(); ++i) {
        if (!isMapDataLump(L[i].name))
            break;
        out.push_back(L[i]);
    }
    return out;
}

MapModel readDoomMap(const std::vector<archive::Lump>& lumps) {
    MapModel m;

    if (const auto* v = findLump(lumps, "VERTEXES")) {
        util::ByteReader r(v->data);
        const size_t n = v->data.size() / 4;
        for (size_t i = 0; i < n; ++i) {
            const int16_t x = r.i16();
            const int16_t y = r.i16();
            m.addVertex({static_cast<double>(x), static_cast<double>(y)});
        }
    }
    if (const auto* s = findLump(lumps, "SECTORS")) {
        util::ByteReader r(s->data);
        const size_t n = s->data.size() / 26;
        for (size_t i = 0; i < n; ++i) {
            Sector sec;
            sec.floorHeight = r.i16();
            sec.ceilHeight = r.i16();
            sec.floorTex = r.fixedString(8);
            sec.ceilTex = r.fixedString(8);
            sec.lightLevel = r.i16();
            sec.special = r.u16();
            sec.tag = r.u16();
            m.addSector(std::move(sec));
        }
    }
    if (const auto* sd = findLump(lumps, "SIDEDEFS")) {
        util::ByteReader r(sd->data);
        const size_t n = sd->data.size() / 30;
        for (size_t i = 0; i < n; ++i) {
            Sidedef side;
            side.offsetX = r.i16();
            side.offsetY = r.i16();
            side.upper = r.fixedString(8);   // Doom order: upper, lower, middle
            side.lower = r.fixedString(8);
            side.middle = r.fixedString(8);
            side.sector = r.i16();
            m.addSidedef(std::move(side));
        }
    }
    if (const auto* ld = findLump(lumps, "LINEDEFS")) {
        util::ByteReader r(ld->data);
        const size_t n = ld->data.size() / 14;
        for (size_t i = 0; i < n; ++i) {
            Linedef l;
            l.v1 = r.u16();
            l.v2 = r.u16();
            l.flags = r.u16();
            l.special = r.u16();
            l.tag = r.u16();
            l.front = refFrom16(r.u16());
            l.back = refFrom16(r.u16());
            m.addLinedef(std::move(l));
        }
    }
    if (const auto* th = findLump(lumps, "THINGS")) {
        util::ByteReader r(th->data);
        const size_t n = th->data.size() / 10;
        for (size_t i = 0; i < n; ++i) {
            Thing t;
            const int16_t x = r.i16();
            const int16_t y = r.i16();
            t.pos = {static_cast<double>(x), static_cast<double>(y)};
            t.angle = r.i16();
            t.type = r.u16();
            t.flags = r.u16();
            m.addThing(std::move(t));
        }
    }
    return m;
}

std::vector<archive::Lump> writeDoomMap(const MapModel& m) {
    util::ByteWriter things, linedefs, sidedefs, vertexes, sectors;

    for (size_t i = 0; i < m.thingCount(); ++i) {
        const Thing& t = m.thing(static_cast<int>(i));
        things.i16(toI16(t.pos.x));
        things.i16(toI16(t.pos.y));
        things.i16(static_cast<int16_t>(t.angle));
        things.u16(static_cast<uint16_t>(t.type));
        things.u16(static_cast<uint16_t>(t.flags));
    }
    for (size_t i = 0; i < m.linedefCount(); ++i) {
        const Linedef& l = m.linedef(static_cast<int>(i));
        linedefs.u16(static_cast<uint16_t>(l.v1));
        linedefs.u16(static_cast<uint16_t>(l.v2));
        linedefs.u16(static_cast<uint16_t>(l.flags));
        linedefs.u16(static_cast<uint16_t>(l.special));
        linedefs.u16(static_cast<uint16_t>(l.tag));
        linedefs.u16(static_cast<uint16_t>(refTo16(l.front)));
        linedefs.u16(static_cast<uint16_t>(refTo16(l.back)));
    }
    for (size_t i = 0; i < m.sidedefCount(); ++i) {
        const Sidedef& s = m.sidedef(static_cast<int>(i));
        sidedefs.i16(static_cast<int16_t>(s.offsetX));
        sidedefs.i16(static_cast<int16_t>(s.offsetY));
        sidedefs.fixedString(s.upper, 8);
        sidedefs.fixedString(s.lower, 8);
        sidedefs.fixedString(s.middle, 8);
        sidedefs.i16(static_cast<int16_t>(s.sector));
    }
    for (size_t i = 0; i < m.vertexCount(); ++i) {
        const Vertex& v = m.vertex(static_cast<int>(i));
        vertexes.i16(toI16(v.pos.x));
        vertexes.i16(toI16(v.pos.y));
    }
    for (size_t i = 0; i < m.sectorCount(); ++i) {
        const Sector& s = m.sector(static_cast<int>(i));
        sectors.i16(static_cast<int16_t>(s.floorHeight));
        sectors.i16(static_cast<int16_t>(s.ceilHeight));
        sectors.fixedString(s.floorTex, 8);
        sectors.fixedString(s.ceilTex, 8);
        sectors.i16(static_cast<int16_t>(s.lightLevel));
        sectors.u16(static_cast<uint16_t>(s.special));
        sectors.u16(static_cast<uint16_t>(s.tag));
    }

    return {
        {"THINGS", things.take()},
        {"LINEDEFS", linedefs.take()},
        {"SIDEDEFS", sidedefs.take()},
        {"VERTEXES", vertexes.take()},
        {"SECTORS", sectors.take()},
    };
}

} // namespace elads::map
