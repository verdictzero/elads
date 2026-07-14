// SPDX-License-Identifier: GPL-3.0-or-later
// Sector slope planes: 3-point plane math + deriving floor slopes from slope things.
#include <cmath>

#include "check.h"
#include "mapeditor/model/planes.h"

using namespace elads;

static bool near(double a, double b) { return std::fabs(a - b) < 1e-6; }

// A square sector [0,256]^2, floor 0 / ceiling 128.
static map::MapModel square() {
    map::MapModel m;
    m.addVertex({0, 0});
    m.addVertex({256, 0});
    m.addVertex({256, 256});
    m.addVertex({0, 256});
    map::Sector sec;
    sec.floorHeight = 0;
    sec.ceilHeight = 128;
    m.addSector(sec);
    const int e[4][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}};
    for (int i = 0; i < 4; ++i) {
        map::Sidedef sd;
        sd.sector = 0;
        const int side = m.addSidedef(sd);
        map::Linedef l;
        l.v1 = e[i][0];
        l.v2 = e[i][1];
        l.front = side;
        m.addLinedef(l);
    }
    return m;
}

static void run() {
    // --- Plane math ---
    const map::Plane flat = map::Plane::flat(64);
    CHECK(near(flat.heightAt(0, 0), 64));
    CHECK(near(flat.heightAt(999, -50), 64));
    CHECK(!flat.sloped());

    // Plane through (0,0,0),(100,0,0),(0,100,50): passes through the 3 points, tilts in y.
    const map::Plane p = map::Plane::fromPoints(0, 0, 0, 100, 0, 0, 0, 100, 50);
    CHECK(near(p.heightAt(0, 0), 0));
    CHECK(near(p.heightAt(100, 0), 0));
    CHECK(near(p.heightAt(0, 100), 50));
    CHECK(near(p.heightAt(0, 50), 25)); // linear interpolation
    CHECK(p.sloped());

    // --- sectorAt ---
    map::MapModel m = square();
    CHECK_EQ(map::sectorAt(m, 128, 128), 0);
    CHECK_EQ(map::sectorAt(m, -10, 128), map::kNoRef);

    // --- Slope from slope things (type 9500) ---
    const double pts[3][3] = {{10, 10, 0}, {240, 10, 0}, {10, 240, 64}};
    for (auto& s : pts) {
        map::Thing t;
        t.pos = {s[0], s[1]};
        t.z = s[2];
        t.type = 9500;
        m.addThing(t);
    }
    const std::vector<map::SectorPlanes> sp = map::computeSectorPlanes(m);
    CHECK_EQ(sp.size(), static_cast<size_t>(1));
    CHECK(sp[0].floor.sloped());
    CHECK(near(sp[0].floor.heightAt(10, 10), 0));
    CHECK(near(sp[0].floor.heightAt(10, 240), 64));
    CHECK(sp[0].floor.heightAt(10, 240) > sp[0].floor.heightAt(10, 10)); // rises with y
    CHECK(!sp[0].ceil.sloped());                                          // ceiling stays flat
    CHECK(near(sp[0].ceil.heightAt(0, 0), 128));
}

TEST_MAIN(run())
