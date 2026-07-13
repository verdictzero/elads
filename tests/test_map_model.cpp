// SPDX-License-Identifier: GPL-3.0-or-later
// Build a one-sector square and exercise topology helpers + undo/redo.
#include "check.h"
#include "mapeditor/model/map_model.h"
#include "util/undo.h"

using namespace elads;

static void run() {
    map::MapModel m;

    // A 64x64 square sector, corners CCW.
    const int v0 = m.addVertex({0, 0});
    const int v1 = m.addVertex({64, 0});
    const int v2 = m.addVertex({64, 64});
    const int v3 = m.addVertex({0, 64});

    map::Sector sec;
    sec.floorHeight = 0;
    sec.ceilHeight = 128;
    sec.floorTex = "FLOOR4_8";
    sec.ceilTex = "CEIL3_5";
    const int s0 = m.addSector(sec);

    auto side = [&] {
        map::Sidedef sd;
        sd.sector = s0;
        sd.middle = "STARTAN2";
        return m.addSidedef(sd);
    };
    // One linedef per edge, one-sided (front only).
    auto edge = [&](int a, int b) {
        map::Linedef l;
        l.v1 = a;
        l.v2 = b;
        l.front = side();
        l.flags = 1; // impassable
        return m.addLinedef(l);
    };
    const int bottom = edge(v0, v1);
    edge(v1, v2);
    edge(v2, v3);
    edge(v3, v0);

    CHECK_EQ(m.vertexCount(), static_cast<size_t>(4));
    CHECK_EQ(m.linedefCount(), static_cast<size_t>(4));
    CHECK_EQ(m.sidedefCount(), static_cast<size_t>(4));
    CHECK_EQ(m.sectorCount(), static_cast<size_t>(1));

    // Topology: every linedef's front points at sector 0; one-sided => no back sector.
    CHECK_EQ(m.frontSector(m.linedef(bottom)), s0);
    CHECK_EQ(m.backSector(m.linedef(bottom)), map::kNoRef);
    CHECK(!m.linedef(bottom).twoSided());

    // Bounds.
    const util::BBox box = m.bounds();
    CHECK(box.valid());
    CHECK(box.width() == 64.0 && box.height() == 64.0);

    // Hit-testing.
    CHECK_EQ(m.nearestVertex({2, 2}, 8.0), v0);
    CHECK_EQ(m.nearestVertex({100, 100}, 8.0), map::kNoRef);
    CHECK_EQ(m.nearestLinedef({32, 2}, 8.0), bottom); // bottom edge lies along y=0

    // Undo/redo: move v0 and roll it back.
    util::UndoManager undo;
    const util::Vec2 oldPos = m.vertex(v0).pos;
    const util::Vec2 newPos{-16, -16};
    undo.perform(
        "move vertex",
        [&] { m.vertex(v0).pos = newPos; },
        [&] { m.vertex(v0).pos = oldPos; });
    CHECK(m.vertex(v0).pos == newPos);
    CHECK(undo.canUndo());

    CHECK(undo.undo());
    CHECK(m.vertex(v0).pos == oldPos);
    CHECK(undo.canRedo());

    CHECK(undo.redo());
    CHECK(m.vertex(v0).pos == newPos);
}

TEST_MAIN(run())
