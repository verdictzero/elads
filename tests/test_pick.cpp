// SPDX-License-Identifier: GPL-3.0-or-later
// Picking (B2): 2D screen<->world unproject, 3D screen ray + floor-plane hit, nearestThing, and
// pick() precedence. Pure math — no GL.
#include <cmath>

#include "check.h"
#include "mapeditor/edit/selection.h"
#include "mapeditor/model/map_model.h"
#include "mapeditor/view2d/map_view_2d.h"
#include "mapeditor/view3d/map_view_3d.h"

using namespace elads;

static bool near(double a, double b, double eps = 1e-4) { return std::fabs(a - b) < eps; }

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

static void test2DUnproject() {
    view::Camera2D cam;
    cam.centerX = 100;
    cam.centerY = 50;
    cam.pixelsPerUnit = 2.0;
    cam.width = 800;
    cam.height = 600;

    // Screen centre maps to the camera centre.
    const util::Vec2 mid = view::screenToWorld(cam, 400, 300);
    CHECK(near(mid.x, 100.0));
    CHECK(near(mid.y, 50.0));

    // +x pixels move +x world; +y pixels move -y world (screen y is flipped).
    const util::Vec2 right = view::screenToWorld(cam, 500, 300); // +100px at 2px/unit => +50u
    CHECK(near(right.x, 150.0));
    CHECK(near(right.y, 50.0));
    const util::Vec2 down = view::screenToWorld(cam, 400, 400); // +100px down => -50u world
    CHECK(near(down.x, 100.0));
    CHECK(near(down.y, 0.0));

    // Round-trip world -> screen -> world.
    for (util::Vec2 w : {util::Vec2{0, 0}, util::Vec2{123, -45}, util::Vec2{250, 300}}) {
        const util::Vec2 s = view::worldToScreen(cam, w);
        const util::Vec2 back = view::screenToWorld(cam, s.x, s.y);
        CHECK(near(back.x, w.x, 1e-3));
        CHECK(near(back.y, w.y, 1e-3));
    }
}

static void test3DRay() {
    // Camera at origin height 64, looking straight along -Z (yaw=0, pitch=0).
    view::Camera3D cam;
    cam.x = 0;
    cam.y = 64;
    cam.z = 0;
    cam.yaw = 0;
    cam.pitch = 0;
    cam.width = 800;
    cam.height = 600;

    // The centre ray points straight forward (map -Y), level.
    const view::Ray3D center = view::screenRay(cam, 400, 300);
    CHECK(near(center.dx, 0.0, 1e-3));
    CHECK(near(center.dy, 0.0, 1e-3));
    CHECK(near(center.dz, -1.0, 1e-3));

    // A ray below screen centre angles downward (dy < 0) and hits the floor (Y=0) in front.
    const view::Ray3D low = view::screenRay(cam, 400, 500);
    CHECK(low.dy < 0.0);
    util::Vec2 hit;
    CHECK(view::rayHitHeight(low, 0.0, hit));
    CHECK(hit.y < 0.0); // in front of the camera => decreasing map Y

    // A level/upward ray never reaches the floor below.
    const view::Ray3D up = view::screenRay(cam, 400, 100);
    CHECK(up.dy > 0.0);
    util::Vec2 none;
    CHECK(!view::rayHitHeight(up, 0.0, none));

    // Pitching down makes the centre ray hit the floor at a finite distance ahead.
    cam.pitch = -0.5f;
    const view::Ray3D c2 = view::screenRay(cam, 400, 300);
    CHECK(c2.dy < 0.0);
    util::Vec2 fh;
    CHECK(view::rayHitHeight(c2, 0.0, fh));
    CHECK(fh.y < 0.0);
}

static void testNearestThing() {
    map::MapModel m = square();
    map::Thing t0;
    t0.pos = {32, 32};
    t0.type = 1;
    const int i0 = m.addThing(t0);
    map::Thing t1;
    t1.pos = {200, 200};
    t1.type = 3004;
    const int i1 = m.addThing(t1);

    CHECK_EQ(m.nearestThing({34, 30}, 16.0), i0);
    CHECK_EQ(m.nearestThing({205, 198}, 16.0), i1);
    CHECK_EQ(m.nearestThing({128, 128}, 16.0), map::kNoRef); // nothing nearby
}

static void testPickPrecedence() {
    map::MapModel m = square(256.0);
    // A thing near the centre (away from any vertex/line).
    map::Thing t;
    t.pos = {128, 128};
    t.type = 1;
    const int ti = m.addThing(t);

    // Right on a corner vertex -> Vertex wins.
    edit::Selection s = edit::pick(m, {0, 0}, 8.0);
    CHECK(s.type == edit::ObjType::Vertex);
    CHECK_EQ(s.index, 0);

    // Near the centre thing (no vertex/line close) -> Thing.
    s = edit::pick(m, {130, 126}, 8.0);
    CHECK(s.type == edit::ObjType::Thing);
    CHECK_EQ(s.index, ti);

    // On the bottom edge midpoint (y=0), away from vertices -> Linedef.
    s = edit::pick(m, {128, 1}, 8.0);
    CHECK(s.type == edit::ObjType::Linedef);

    // Interior point with nothing close -> the enclosing Sector.
    s = edit::pick(m, {64, 200}, 8.0);
    CHECK(s.type == edit::ObjType::Sector);
    CHECK_EQ(s.index, 0);

    // Fully outside -> nothing.
    s = edit::pick(m, {-50, -50}, 8.0);
    CHECK(s.empty());

    // Type-restricted pick ignores higher-precedence hits.
    s = edit::pickOfType(m, {0, 0}, 8.0, edit::ObjType::Sector);
    CHECK(s.type == edit::ObjType::Sector); // the corner is on sector 0's boundary triangle
}

static void run() {
    test2DUnproject();
    test3DRay();
    testNearestThing();
    testPickPrecedence();
}

TEST_MAIN(run())
