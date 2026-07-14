// SPDX-License-Identifier: GPL-3.0-or-later
// Slope plane math + slope sources (A1): Plane::fromPoints/heightAt, sectorAt, slope things
// (9500/9501), and Plane_Align (181). Pure data — no GL.
#include <cmath>

#include "check.h"
#include "mapeditor/model/map_model.h"
#include "mapeditor/model/planes.h"

using namespace elads;

static bool near(double a, double b, double eps = 1e-6) { return std::fabs(a - b) < eps; }

// A single square sector [0,256]^2, floor 0 / ceiling 128, one-sided edges.
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

static void testPlaneMath() {
    // Flat plane.
    const map::Plane flat = map::Plane::flat(64.0);
    CHECK(flat.isFlat());
    CHECK(near(flat.heightAt(10, 20), 64.0));
    CHECK(near(flat.heightAt(-500, 999), 64.0));

    // A plane through 3 non-collinear points reproduces those heights exactly and
    // interpolates linearly between them.
    const util::Vec2 p0{0, 0}, p1{100, 0}, p2{0, 100};
    const map::Plane p = map::Plane::fromPoints(p0, 0.0, p1, 50.0, p2, 20.0);
    CHECK(!p.isFlat());
    CHECK(near(p.heightAt(p0), 0.0));
    CHECK(near(p.heightAt(p1), 50.0));
    CHECK(near(p.heightAt(p2), 20.0));
    // Midpoint of p0..p1 is halfway up the x-gradient.
    CHECK(near(p.heightAt({50, 0}), 25.0));
    // Point (100,100) = p1 + p2 offset from p0: 50 + 20 = 70.
    CHECK(near(p.heightAt({100, 100}), 70.0));

    // Degenerate (collinear) input falls back to flat at z0.
    const map::Plane deg = map::Plane::fromPoints({0, 0}, 5.0, {10, 0}, 5.0, {20, 0}, 5.0);
    CHECK(deg.isFlat());
    CHECK(near(deg.heightAt(3, 7), 5.0));
}

static void testSectorAt() {
    const map::MapModel m = square(256.0);
    CHECK_EQ(map::sectorAt(m, 128, 128), 0); // centre
    CHECK_EQ(map::sectorAt(m, 4, 4), 0);     // near a corner, inside
    CHECK_EQ(map::sectorAt(m, -10, 50), map::kNoRef);
    CHECK_EQ(map::sectorAt(m, 300, 300), map::kNoRef);
}

static void testFlatByDefault() {
    const map::MapModel m = square();
    const auto planes = map::computeSectorPlanes(m);
    CHECK_EQ(planes.size(), static_cast<size_t>(1));
    CHECK(planes[0].floor.isFlat());
    CHECK(planes[0].ceil.isFlat());
    CHECK(near(planes[0].floor.heightAt(50, 50), 0.0));
    CHECK(near(planes[0].ceil.heightAt(50, 50), 128.0));
}

static void testSlopeThings() {
    map::MapModel m = square(256.0);
    // Three floor slope things (type 9500) with distinct heights inside the sector.
    auto slopeThing = [&](double x, double y, double z) {
        map::Thing t;
        t.pos = {x, y};
        t.z = z;
        t.type = 9500;
        m.addThing(t);
    };
    slopeThing(10, 10, 0.0);
    slopeThing(240, 10, 64.0);
    slopeThing(10, 240, 32.0);

    const auto planes = map::computeSectorPlanes(m);
    CHECK(!planes[0].floor.isFlat());
    // The plane passes through the three defining points.
    CHECK(near(planes[0].floor.heightAt(10, 10), 0.0, 1e-4));
    CHECK(near(planes[0].floor.heightAt(240, 10), 64.0, 1e-4));
    CHECK(near(planes[0].floor.heightAt(10, 240), 32.0, 1e-4));
    // Height rises with +x (the 0->64 gradient).
    CHECK(planes[0].floor.heightAt(240, 10) > planes[0].floor.heightAt(10, 10));
    // Ceiling stays flat (only floor things were placed).
    CHECK(planes[0].ceil.isFlat());
}

// Two adjacent square sectors sharing a middle line; the shared line carries Plane_Align (181)
// to slope sector 0's floor. Expect a tilt that meets sector 1's floor at the hinge.
static void testPlaneAlign() {
    map::MapModel m;
    // Vertices: left sector [0..128], right sector [128..256], both y in [0..128].
    const int v0 = m.addVertex({0, 0});
    const int v1 = m.addVertex({128, 0});
    const int v2 = m.addVertex({128, 128});
    const int v3 = m.addVertex({0, 128});
    const int v4 = m.addVertex({256, 0});
    const int v5 = m.addVertex({256, 128});

    map::Sector s0; // sloped sector (front)
    s0.floorHeight = 0;
    s0.ceilHeight = 128;
    map::Sector s1; // model sector (back), floor higher
    s1.floorHeight = 64;
    s1.ceilHeight = 128;
    m.addSector(s0);
    m.addSector(s1);

    auto side = [&](int sec) {
        map::Sidedef sd;
        sd.sector = sec;
        return m.addSidedef(sd);
    };
    auto line = [&](int a, int b, int front, int back, int special, int arg0) {
        map::Linedef l;
        l.v1 = a;
        l.v2 = b;
        l.front = front;
        l.back = back;
        l.special = special;
        l.args[0] = arg0;
        return m.addLinedef(l);
    };
    // Sector 0 outer edges (one-sided).
    line(v0, v1, side(0), map::kNoRef, 0, 0);
    line(v2, v3, side(0), map::kNoRef, 0, 0);
    line(v3, v0, side(0), map::kNoRef, 0, 0);
    // Shared line v1->v2: front = sector 0, back = sector 1, Plane_Align floor front (arg0=1).
    line(v1, v2, side(0), side(1), 181, 1);
    // Sector 1 outer edges.
    line(v1, v4, side(1), map::kNoRef, 0, 0);
    line(v4, v5, side(1), map::kNoRef, 0, 0);
    line(v5, v2, side(1), map::kNoRef, 0, 0);

    const auto planes = map::computeSectorPlanes(m);
    // Sector 0's floor should now be sloped (not flat).
    CHECK(!planes[0].floor.isFlat());
    // At the hinge line (x=128) it meets the model sector's floor height (64).
    CHECK(near(planes[0].floor.heightAt(128, 64), 64.0, 1e-3));
    // At the far edge (x=0) it keeps sector 0's own nominal floor (0).
    CHECK(near(planes[0].floor.heightAt(0, 64), 0.0, 1e-3));
    // Monotonic rise from far edge to hinge.
    CHECK(planes[0].floor.heightAt(64, 64) > planes[0].floor.heightAt(0, 64));
    CHECK(planes[0].floor.heightAt(128, 64) > planes[0].floor.heightAt(64, 64));
    // Sector 1 stays flat.
    CHECK(planes[1].floor.isFlat());
}

static void run() {
    testPlaneMath();
    testSectorAt();
    testFlatByDefault();
    testSlopeThings();
    testPlaneAlign();
}

TEST_MAIN(run())
