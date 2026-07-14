// SPDX-License-Identifier: GPL-3.0-or-later
// Interactive editor controller (bind B2/B3): hover/select/drag/delete/nudge/undo + camera,
// all driven through screen-space input. Pure logic — no GL. Screen coords are derived from
// world points via worldToScreen so the round-trip is exact.
#include <cmath>

#include "check.h"
#include "mapeditor/edit/editor.h"
#include "mapeditor/model/map_model.h"
#include "mapeditor/view2d/map_view_2d.h"

using namespace elads;

static bool near(double a, double b, double eps = 1e-6) { return std::fabs(a - b) < eps; }

static map::MapModel square(double side = 256.0) {
    map::MapModel m;
    m.addVertex({0, 0});
    m.addVertex({side, 0});
    m.addVertex({side, side});
    m.addVertex({0, side});
    map::Sector sec;
    sec.floorHeight = 0;
    sec.ceilHeight = 128;
    m.addSector(sec);
    const int e[4][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}};
    for (auto& ed : e) {
        map::Sidedef sd;
        sd.sector = 0;
        const int s = m.addSidedef(sd);
        map::Linedef l;
        l.v1 = ed[0];
        l.v2 = ed[1];
        l.front = s;
        m.addLinedef(l);
    }
    return m;
}

static edit::MapEditor makeEditor() {
    edit::MapEditor ed(square(256.0));
    ed.camera().centerX = 128;
    ed.camera().centerY = 128;
    ed.camera().pixelsPerUnit = 2.0;
    ed.camera().width = 512;
    ed.camera().height = 512;
    return ed;
}

// Screen pixel for a world point under the editor's camera.
static util::Vec2 screenOf(const edit::MapEditor& ed, util::Vec2 world) {
    return view::worldToScreen(ed.camera(), world);
}

static void testHoverSelect() {
    edit::MapEditor ed = makeEditor();
    ed.setMode(edit::MapEditor::Mode::Vertices);

    const util::Vec2 s = screenOf(ed, {0, 0});
    ed.hover(s.x, s.y);
    CHECK(ed.highlight().type == edit::ObjType::Vertex);
    CHECK_EQ(ed.highlight().index, 0);

    // Hover far from any vertex -> no highlight.
    const util::Vec2 mid = screenOf(ed, {128, 128});
    ed.hover(mid.x, mid.y);
    CHECK(ed.highlight().empty());

    ed.clickSelect(s.x, s.y);
    CHECK(ed.selection().type == edit::ObjType::Vertex);
    CHECK_EQ(ed.selection().index, 0);
}

static void testModeSwitchClears() {
    edit::MapEditor ed = makeEditor();
    ed.setMode(edit::MapEditor::Mode::Vertices);
    const util::Vec2 s = screenOf(ed, {0, 0});
    ed.clickSelect(s.x, s.y);
    CHECK(!ed.selection().empty());
    ed.setMode(edit::MapEditor::Mode::Sectors);
    CHECK(ed.selection().empty()); // switching modes drops the selection
    CHECK(ed.modeType() == edit::ObjType::Sector);
}

static void testSectorPick() {
    edit::MapEditor ed = makeEditor();
    ed.setMode(edit::MapEditor::Mode::Sectors);
    const util::Vec2 s = screenOf(ed, {64, 200});
    ed.clickSelect(s.x, s.y);
    CHECK(ed.selection().type == edit::ObjType::Sector);
    CHECK_EQ(ed.selection().index, 0);
}

static void testVertexDrag() {
    edit::MapEditor ed = makeEditor();
    ed.setMode(edit::MapEditor::Mode::Vertices);
    ed.setGridSnap(true);
    ed.setGridSize(8.0);

    const util::Vec2 grab = screenOf(ed, {0, 0});
    CHECK(ed.beginDrag(grab.x, grab.y));
    CHECK(ed.dragging());

    // Drag toward (66,63); with 8-unit snap it lands on (64,64).
    const util::Vec2 to = screenOf(ed, {66, 63});
    ed.updateDrag(to.x, to.y);
    CHECK(ed.model().vertex(0).pos == util::Vec2(64, 64)); // live preview, snapped

    ed.endDrag();
    CHECK(!ed.dragging());
    CHECK(ed.model().vertex(0).pos == util::Vec2(64, 64));
    // The whole drag is one undo step back to the original position.
    CHECK(ed.undoLast());
    CHECK(ed.model().vertex(0).pos == util::Vec2(0, 0));
    CHECK(ed.redoLast());
    CHECK(ed.model().vertex(0).pos == util::Vec2(64, 64));

    // Grabbing empty space fails.
    const util::Vec2 empty = screenOf(ed, {128, 128});
    CHECK(!ed.beginDrag(empty.x, empty.y));
}

static void testDragCancel() {
    edit::MapEditor ed = makeEditor();
    ed.setMode(edit::MapEditor::Mode::Vertices);
    const util::Vec2 grab = screenOf(ed, {256, 0}); // vertex 1
    CHECK(ed.beginDrag(grab.x, grab.y));
    const util::Vec2 to = screenOf(ed, {200, 40});
    ed.updateDrag(to.x, to.y);
    CHECK(ed.model().vertex(1).pos != util::Vec2(256, 0));
    ed.cancelDrag();
    CHECK(ed.model().vertex(1).pos == util::Vec2(256, 0)); // restored
    CHECK(!ed.undo().canUndo());                            // nothing recorded
}

static void testThings() {
    edit::MapEditor ed = makeEditor();
    map::Thing t;
    t.pos = {64, 64};
    t.type = 1;
    ed.model().addThing(t);
    ed.setMode(edit::MapEditor::Mode::Things);
    ed.setGridSnap(false);

    const util::Vec2 s = screenOf(ed, {64, 64});
    ed.clickSelect(s.x, s.y);
    CHECK(ed.selection().type == edit::ObjType::Thing);

    // Drag the thing to (100,120).
    CHECK(ed.beginDrag(s.x, s.y));
    const util::Vec2 to = screenOf(ed, {100, 120});
    ed.updateDrag(to.x, to.y);
    ed.endDrag();
    CHECK(ed.model().thing(0).pos == util::Vec2(100, 120));
    CHECK(ed.undoLast());
    CHECK(ed.model().thing(0).pos == util::Vec2(64, 64));

    // Delete it (undoable).
    ed.clickSelect(s.x, s.y);
    ed.deleteSelection();
    CHECK_EQ(ed.model().thingCount(), static_cast<size_t>(0));
    CHECK(ed.undoLast());
    CHECK_EQ(ed.model().thingCount(), static_cast<size_t>(1));
}

static void testNudge() {
    edit::MapEditor ed = makeEditor();
    ed.setMode(edit::MapEditor::Mode::Vertices);
    const util::Vec2 s = screenOf(ed, {0, 0});
    ed.clickSelect(s.x, s.y);
    ed.nudgeSelection(8, -8);
    CHECK(ed.model().vertex(0).pos == util::Vec2(8, -8));
    CHECK(ed.undoLast());
    CHECK(ed.model().vertex(0).pos == util::Vec2(0, 0));
}

static void testCamera() {
    edit::MapEditor ed = makeEditor();
    // Zoom in around a world point; that point stays under the same screen pixel.
    const util::Vec2 worldPt{200, 96};
    const util::Vec2 sPix = screenOf(ed, worldPt);
    ed.zoomAt(2.0, sPix.x, sPix.y);
    CHECK(near(ed.camera().pixelsPerUnit, 4.0));
    const util::Vec2 after = view::screenToWorld(ed.camera(), sPix.x, sPix.y);
    CHECK(near(after.x, worldPt.x, 1e-3));
    CHECK(near(after.y, worldPt.y, 1e-3));

    // Pan by +100px right / +50px down.
    const double cx = ed.camera().centerX, cy = ed.camera().centerY;
    ed.panPixels(100, 50);
    CHECK(near(ed.camera().centerX, cx - 100.0 / 4.0));
    CHECK(near(ed.camera().centerY, cy + 50.0 / 4.0));
}

static void testDrawSector() {
    edit::MapEditor ed(map::MapModel{}); // empty map
    ed.camera().centerX = 128;
    ed.camera().centerY = 128;
    ed.camera().pixelsPerUnit = 2.0;
    ed.camera().width = 512;
    ed.camera().height = 512;
    ed.setMode(edit::MapEditor::Mode::Draw);
    ed.setGridSnap(false);

    auto click = [&](util::Vec2 world) {
        const util::Vec2 s = view::worldToScreen(ed.camera(), world);
        return ed.addDrawPoint(s.x, s.y);
    };

    // Trace a triangle; no sector yet.
    CHECK(!click({0, 0}));
    CHECK(!click({128, 0}));
    CHECK(!click({64, 128}));
    CHECK_EQ(ed.drawPoints().size(), static_cast<size_t>(3));
    CHECK_EQ(ed.model().sectorCount(), static_cast<size_t>(0));

    // Click near the first point -> closes into a sector.
    CHECK(click({2, 1})); // within the pick radius of (0,0)
    CHECK_EQ(ed.model().sectorCount(), static_cast<size_t>(1));
    CHECK_EQ(ed.model().vertexCount(), static_cast<size_t>(3));
    CHECK_EQ(ed.model().linedefCount(), static_cast<size_t>(3));
    CHECK(ed.drawPoints().empty()); // loop consumed

    // The whole trace is one undoable step.
    CHECK(ed.undoLast());
    CHECK(ed.model().empty());

    // Cancel mid-trace discards points without creating anything.
    click({0, 0});
    click({64, 0});
    CHECK_EQ(ed.drawPoints().size(), static_cast<size_t>(2));
    ed.cancelDraw();
    CHECK(ed.drawPoints().empty());
    CHECK_EQ(ed.model().sectorCount(), static_cast<size_t>(0));

    // Switching modes also clears an in-progress trace.
    click({10, 10});
    ed.setMode(edit::MapEditor::Mode::Vertices);
    CHECK(ed.drawPoints().empty());
}

static void run() {
    testHoverSelect();
    testModeSwitchClears();
    testSectorPick();
    testVertexDrag();
    testDragCancel();
    testThings();
    testNudge();
    testCamera();
    testDrawSector();
}

TEST_MAIN(run())
