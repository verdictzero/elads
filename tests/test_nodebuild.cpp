// SPDX-License-Identifier: GPL-3.0-or-later
// Embedded BSP node builder: builds SEGS/SSECTORS/NODES/BLOCKMAP/REJECT (+ split VERTEXES) and
// validates the tree — lump sizes, index ranges, every seg placed in a subsector, and that a
// point-in-subsector descent (Doom's node side rule) reaches a valid subsector. Pure core — no GL.
#include <cmath>
#include <vector>

#include "check.h"
#include "archive/wad.h"
#include "mapeditor/model/map_model.h"
#include "nodebuild/nodebuild.h"
#include "util/byte_io.h"

using namespace elads;

// A clockwise-wound square sector (front sidedef faces the interior, the Doom convention).
static void addCWSquare(map::MapModel& m, double x0, double y0, double s, int sector) {
    const int v0 = m.addVertex({x0, y0});
    const int v1 = m.addVertex({x0, y0 + s});
    const int v2 = m.addVertex({x0 + s, y0 + s});
    const int v3 = m.addVertex({x0 + s, y0});
    const int vs[4] = {v0, v1, v2, v3};
    for (int i = 0; i < 4; ++i) {
        map::Sidedef sd;
        sd.sector = sector;
        const int side = m.addSidedef(sd);
        map::Linedef l;
        l.v1 = vs[i];
        l.v2 = vs[(i + 1) % 4];
        l.front = side;
        m.addLinedef(l);
    }
}

// Parsed build lumps for validation.
struct Parsed {
    int nVerts = 0, nSegs = 0, nSS = 0, nNodes = 0;
    struct Seg { int v1, v2, linedef, side; };
    struct SS { int count, first; };
    struct Node { double x, y, dx, dy; uint16_t rc, lc; };
    std::vector<Seg> segs;
    std::vector<SS> ss;
    std::vector<Node> nodes;
};

static const archive::Lump& lump(const nodebuild::BuildResult& r, const char* n) {
    for (const auto& l : r.lumps)
        if (l.name == n)
            return l;
    static archive::Lump empty;
    return empty;
}

static Parsed parse(const nodebuild::BuildResult& r) {
    Parsed p;
    p.nVerts = static_cast<int>(lump(r, "VERTEXES").data.size() / 4);
    {
        util::ByteReader rd(lump(r, "SEGS").data);
        p.nSegs = static_cast<int>(lump(r, "SEGS").data.size() / 12);
        for (int i = 0; i < p.nSegs; ++i) {
            Parsed::Seg s;
            s.v1 = rd.u16();
            s.v2 = rd.u16();
            rd.u16(); // angle
            s.linedef = rd.u16();
            s.side = rd.u16();
            rd.i16(); // offset
            p.segs.push_back(s);
        }
    }
    {
        util::ByteReader rd(lump(r, "SSECTORS").data);
        p.nSS = static_cast<int>(lump(r, "SSECTORS").data.size() / 4);
        for (int i = 0; i < p.nSS; ++i) {
            Parsed::SS s;
            s.count = rd.u16();
            s.first = rd.u16();
            p.ss.push_back(s);
        }
    }
    {
        util::ByteReader rd(lump(r, "NODES").data);
        p.nNodes = static_cast<int>(lump(r, "NODES").data.size() / 28);
        for (int i = 0; i < p.nNodes; ++i) {
            Parsed::Node n;
            n.x = rd.i16();
            n.y = rd.i16();
            n.dx = rd.i16();
            n.dy = rd.i16();
            for (int k = 0; k < 8; ++k)
                rd.i16(); // bboxes
            n.rc = rd.u16();
            n.lc = rd.u16();
            p.nodes.push_back(n);
        }
    }
    return p;
}

// Descend the BSP for a point using Doom's node side rule; returns the subsector index.
static int pointSubsector(const Parsed& p, double px, double py) {
    if (p.nNodes == 0)
        return 0; // single-subsector map
    uint16_t ref = static_cast<uint16_t>(p.nNodes - 1); // root = last node
    for (int guard = 0; guard < 1000; ++guard) {
        if (ref & 0x8000)
            return ref & 0x7FFF;
        const Parsed::Node& n = p.nodes[ref];
        const double s = n.dx * (py - n.y) - n.dy * (px - n.x);
        ref = (s < 0) ? n.rc : n.lc; // front (right) if s < 0
    }
    return -1;
}

static void validate(const nodebuild::BuildResult& r, const map::MapModel& m) {
    const Parsed p = parse(r);
    CHECK_EQ(p.nSegs, r.stats.segs);
    CHECK_EQ(p.nSS, r.stats.subsectors);
    CHECK_EQ(p.nNodes, r.stats.nodes);

    // Seg vertex/linedef/side references are in range.
    for (const auto& s : p.segs) {
        CHECK(s.v1 >= 0 && s.v1 < p.nVerts);
        CHECK(s.v2 >= 0 && s.v2 < p.nVerts);
        CHECK(s.linedef >= 0 && s.linedef < static_cast<int>(m.linedefCount()));
        CHECK(s.side == 0 || s.side == 1);
    }
    // Subsector seg ranges partition the seg list exactly.
    int total = 0;
    for (const auto& s : p.ss) {
        CHECK(s.first >= 0 && s.first + s.count <= p.nSegs);
        total += s.count;
    }
    CHECK_EQ(total, p.nSegs);
    // Node child refs point to valid nodes/subsectors.
    for (const auto& n : p.nodes) {
        for (uint16_t c : {n.rc, n.lc}) {
            if (c & 0x8000)
                CHECK((c & 0x7FFF) < p.nSS);
            else
                CHECK(c < p.nNodes);
        }
    }
    // REJECT is the right size (nSectors^2 bits) and BLOCKMAP has a header.
    const size_t nSec = m.sectorCount();
    CHECK_EQ(lump(r, "REJECT").data.size(), (nSec * nSec + 7) / 8);
    CHECK(lump(r, "BLOCKMAP").data.size() >= 8);
}

static void testConvexSquare() {
    map::MapModel m;
    addCWSquare(m, 0, 0, 256, 0);
    map::Sector sec;
    m.addSector(sec);
    const nodebuild::BuildResult r = nodebuild::buildNodes(m);

    // A convex, correctly-wound room needs no partition: one subsector, no nodes, no splits.
    CHECK_EQ(r.stats.subsectors, 1);
    CHECK_EQ(r.stats.nodes, 0);
    CHECK_EQ(r.stats.segs, 4);
    CHECK_EQ(r.stats.splitVertices, 0);
    validate(r, m);
    CHECK_EQ(pointSubsector(parse(r), 128, 128), 0);
}

static void testTwoRooms() {
    map::MapModel m;
    addCWSquare(m, 0, 0, 256, 0);     // room A
    addCWSquare(m, 400, 0, 256, 1);   // room B (separate)
    map::Sector a, b;
    m.addSector(a);
    m.addSector(b);
    const nodebuild::BuildResult r = nodebuild::buildNodes(m);

    // Two disjoint rooms must be separated by at least one node into >= 2 subsectors.
    CHECK(r.stats.nodes >= 1);
    CHECK(r.stats.subsectors >= 2);
    validate(r, m);

    // A point in each room descends to a valid subsector.
    const Parsed p = parse(r);
    const int ssA = pointSubsector(p, 128, 128);
    const int ssB = pointSubsector(p, 528, 128);
    CHECK(ssA >= 0 && ssA < p.nSS);
    CHECK(ssB >= 0 && ssB < p.nSS);
}

static void run() {
    testConvexSquare();
    testTwoRooms();
}

TEST_MAIN(run())
