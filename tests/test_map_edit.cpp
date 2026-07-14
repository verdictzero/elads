// SPDX-License-Identifier: GPL-3.0-or-later
// Editing operations + undo (B3): every op mutates the model, then undo restores the exact
// prior state and redo re-applies it. Pure model logic — no GL.
#include "check.h"
#include "mapeditor/edit/map_edit.h"
#include "mapeditor/model/map_model.h"
#include "util/undo.h"

using namespace elads;

// A 256x256 one-sided square sector.
static map::MapModel square(double side = 256.0) {
    map::MapModel m;
    m.addVertex({0, 0});
    m.addVertex({side, 0});
    m.addVertex({side, side});
    m.addVertex({0, side});
    map::Sector sec;
    sec.floorHeight = 0;
    sec.ceilHeight = 128;
    sec.floorTex = "FLAT1";
    sec.ceilTex = "F_SKY1";
    m.addSector(sec);
    const int e[4][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}};
    for (auto& ed : e) {
        map::Sidedef sd;
        sd.sector = 0;
        sd.middle = "WALL";
        const int s = m.addSidedef(sd);
        map::Linedef l;
        l.v1 = ed[0];
        l.v2 = ed[1];
        l.front = s;
        m.addLinedef(l);
    }
    return m;
}

static void testMoveVertex() {
    map::MapModel m = square();
    util::UndoManager undo;
    const util::Vec2 orig = m.vertex(0).pos;
    edit::moveVertex(m, undo, 0, {-16, -32});
    CHECK(m.vertex(0).pos == util::Vec2(-16, -32));
    CHECK(undo.undo());
    CHECK(m.vertex(0).pos == orig);
    CHECK(undo.redo());
    CHECK(m.vertex(0).pos == util::Vec2(-16, -32));

    // No-op move records nothing.
    const size_t depth = undo.undoDepth();
    edit::moveVertex(m, undo, 0, m.vertex(0).pos);
    CHECK_EQ(undo.undoDepth(), depth);
}

static void testMoveVertices() {
    map::MapModel m = square();
    util::UndoManager undo;
    edit::moveVertices(m, undo, {0, 1, 2, 3}, {10, 20});
    CHECK(m.vertex(0).pos == util::Vec2(10, 20));
    CHECK(m.vertex(2).pos == util::Vec2(266, 276));
    CHECK(undo.undo());
    CHECK(m.vertex(0).pos == util::Vec2(0, 0));
    CHECK(m.vertex(2).pos == util::Vec2(256, 256));
}

static void testSectorProps() {
    map::MapModel m = square();
    util::UndoManager undo;
    edit::setSectorHeights(m, undo, 0, 32, 96);
    CHECK_EQ(m.sector(0).floorHeight, 32);
    CHECK_EQ(m.sector(0).ceilHeight, 96);
    edit::setSectorTexture(m, undo, 0, /*floor=*/true, "NUKAGE1");
    CHECK_EQ(m.sector(0).floorTex, std::string("NUKAGE1"));
    CHECK(undo.undo()); // texture
    CHECK_EQ(m.sector(0).floorTex, std::string("FLAT1"));
    CHECK(undo.undo()); // heights
    CHECK_EQ(m.sector(0).floorHeight, 0);
    CHECK_EQ(m.sector(0).ceilHeight, 128);
}

static void testSidedefProps() {
    map::MapModel m = square();
    util::UndoManager undo;
    edit::setSidedefTexture(m, undo, 0, edit::SideTex::Upper, "BRICK7");
    CHECK_EQ(m.sidedef(0).upper, std::string("BRICK7"));
    edit::setSidedefOffset(m, undo, 0, 8, -4);
    CHECK_EQ(m.sidedef(0).offsetX, 8);
    CHECK_EQ(m.sidedef(0).offsetY, -4);
    CHECK(undo.undo());
    CHECK_EQ(m.sidedef(0).offsetX, 0);
    CHECK(undo.undo());
    CHECK_EQ(m.sidedef(0).upper, std::string("-"));
}

static void testFlip() {
    map::MapModel m = square();
    util::UndoManager undo;
    const int v1 = m.linedef(0).v1, v2 = m.linedef(0).v2;
    const int fr = m.linedef(0).front, bk = m.linedef(0).back;
    edit::flipLinedef(m, undo, 0);
    CHECK_EQ(m.linedef(0).v1, v2);
    CHECK_EQ(m.linedef(0).v2, v1);
    CHECK_EQ(m.linedef(0).front, bk);
    CHECK_EQ(m.linedef(0).back, fr);
    CHECK(undo.undo());
    CHECK_EQ(m.linedef(0).v1, v1);
    CHECK_EQ(m.linedef(0).front, fr);
}

static void testSplit() {
    map::MapModel m = square(256.0);
    util::UndoManager undo;
    const size_t v0 = m.vertexCount(), sd0 = m.sidedefCount(), l0 = m.linedefCount();
    // Split the bottom edge (line 0: v0(0,0) -> v1(256,0)) at the midpoint.
    const int origV2 = m.linedef(0).v2;
    const int nv = edit::splitLinedef(m, undo, 0, 0.5);
    CHECK_EQ(nv, static_cast<int>(v0));
    CHECK(m.vertex(nv).pos == util::Vec2(128, 0));       // midpoint
    CHECK_EQ(m.vertexCount(), v0 + 1);
    CHECK_EQ(m.sidedefCount(), sd0 + 1);                 // one-sided: one copied sidedef
    CHECK_EQ(m.linedefCount(), l0 + 1);
    CHECK_EQ(m.linedef(0).v2, nv);                       // original now ends at split
    const int newLine = static_cast<int>(l0);
    CHECK_EQ(m.linedef(newLine).v1, nv);                 // new half starts at split
    CHECK_EQ(m.linedef(newLine).v2, origV2);             // ...and ends at the old endpoint
    CHECK_EQ(m.sidedef(m.linedef(newLine).front).sector, 0); // copied sidedef keeps the sector

    // Undo restores counts and the original endpoint exactly.
    CHECK(undo.undo());
    CHECK_EQ(m.vertexCount(), v0);
    CHECK_EQ(m.sidedefCount(), sd0);
    CHECK_EQ(m.linedefCount(), l0);
    CHECK_EQ(m.linedef(0).v2, origV2);
    // Redo reproduces the split identically.
    CHECK(undo.redo());
    CHECK_EQ(m.vertexCount(), v0 + 1);
    CHECK(m.vertex(nv).pos == util::Vec2(128, 0));

    // Invalid params are rejected without recording.
    const size_t depth = undo.undoDepth();
    CHECK_EQ(edit::splitLinedef(m, undo, 0, 0.0), map::kNoRef);
    CHECK_EQ(edit::splitLinedef(m, undo, 99, 0.5), map::kNoRef);
    CHECK_EQ(undo.undoDepth(), depth);
}

static void testThings() {
    map::MapModel m = square();
    util::UndoManager undo;
    map::Thing t;
    t.pos = {100, 100};
    t.type = 3001;
    const int idx = edit::addThing(m, undo, t);
    CHECK_EQ(m.thingCount(), static_cast<size_t>(1));
    CHECK_EQ(idx, 0);

    map::Thing t2;
    t2.pos = {50, 50};
    t2.type = 2001;
    edit::addThing(m, undo, t2);
    CHECK_EQ(m.thingCount(), static_cast<size_t>(2));

    // Delete the first; the second remains, and undo restores it at index 0.
    edit::deleteThing(m, undo, 0);
    CHECK_EQ(m.thingCount(), static_cast<size_t>(1));
    CHECK_EQ(m.thing(0).type, 2001);
    CHECK(undo.undo()); // undo delete
    CHECK_EQ(m.thingCount(), static_cast<size_t>(2));
    CHECK_EQ(m.thing(0).type, 3001);
    CHECK_EQ(m.thing(1).type, 2001);
}

static void testCreateSector() {
    map::MapModel m; // empty
    util::UndoManager undo;
    map::Sector proto;
    proto.floorHeight = 0;
    proto.ceilHeight = 256;
    proto.floorTex = "FLOOR5_1";
    proto.ceilTex = "CEIL1_1";
    map::Sidedef sideProto;
    sideProto.middle = "STONE2";
    const std::vector<util::Vec2> loop = {{0, 0}, {128, 0}, {128, 128}, {0, 128}};
    const int s = edit::createSector(m, undo, loop, proto, sideProto);
    CHECK_EQ(s, 0);
    CHECK_EQ(m.sectorCount(), static_cast<size_t>(1));
    CHECK_EQ(m.vertexCount(), static_cast<size_t>(4));
    CHECK_EQ(m.linedefCount(), static_cast<size_t>(4));
    CHECK_EQ(m.sidedefCount(), static_cast<size_t>(4));
    CHECK_EQ(m.sidedef(0).sector, 0);
    CHECK_EQ(m.linedef(3).v2, 0); // loop closes back to the first vertex
    CHECK_EQ(m.sector(0).ceilHeight, 256);

    CHECK(undo.undo());
    CHECK(m.empty());
    CHECK(undo.redo());
    CHECK_EQ(m.sectorCount(), static_cast<size_t>(1));
    CHECK_EQ(m.vertexCount(), static_cast<size_t>(4));

    // Too-small loops are rejected.
    CHECK_EQ(edit::createSector(m, undo, {{0, 0}, {1, 1}}, proto, sideProto), map::kNoRef);
}

static void run() {
    testMoveVertex();
    testMoveVertices();
    testSectorProps();
    testSidedefProps();
    testFlip();
    testSplit();
    testThings();
    testCreateSector();
}

TEST_MAIN(run())
