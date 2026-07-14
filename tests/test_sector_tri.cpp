// SPDX-License-Identifier: GPL-3.0-or-later
// Triangulate sector floor polygons and verify triangle count + total area.
#include <cmath>
#include <vector>

#include "check.h"
#include "mapeditor/model/sector_tri.h"

using namespace elads;

// Build a MapModel with one sector bounded by `pts` (CCW), as one-sided linedefs.
static map::MapModel polygonSector(const std::vector<util::Vec2>& pts) {
    map::MapModel m;
    for (const auto& p : pts)
        m.addVertex(p);
    m.addSector(map::Sector{});
    const int n = static_cast<int>(pts.size());
    for (int i = 0; i < n; ++i) {
        map::Sidedef sd;
        sd.sector = 0;
        const int side = m.addSidedef(sd);
        map::Linedef l;
        l.v1 = i;
        l.v2 = (i + 1) % n;
        l.front = side;
        m.addLinedef(l);
    }
    return m;
}

static void run() {
    // Square 64x64 -> 2 triangles, area 4096.
    {
        const map::MapModel m = polygonSector({{0, 0}, {64, 0}, {64, 64}, {0, 64}});
        const map::Triangulation t = map::triangulateSector(m, 0);
        CHECK_EQ(t.points.size(), static_cast<size_t>(4));
        CHECK_EQ(t.indices.size(), static_cast<size_t>(6));
        CHECK_EQ(t.triangleCount(), static_cast<size_t>(2));
        CHECK(std::fabs(t.area() - 4096.0) < 1e-6);
    }

    // Non-convex L-shape -> 4 triangles, area 3072 (64*32 + 32*32).
    {
        const map::MapModel m =
            polygonSector({{0, 0}, {64, 0}, {64, 32}, {32, 32}, {32, 64}, {0, 64}});
        const map::Triangulation t = map::triangulateSector(m, 0);
        CHECK_EQ(t.points.size(), static_cast<size_t>(6));
        CHECK_EQ(t.triangleCount(), static_cast<size_t>(4));
        CHECK(std::fabs(t.area() - 3072.0) < 1e-6);
    }

    // A sector with no boundary yields an empty triangulation.
    {
        map::MapModel empty;
        empty.addSector(map::Sector{});
        const map::Triangulation t = map::triangulateSector(empty, 0);
        CHECK_EQ(t.triangleCount(), static_cast<size_t>(0));
    }
}

TEST_MAIN(run())
