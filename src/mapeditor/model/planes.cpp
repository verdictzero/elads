// SPDX-License-Identifier: GPL-3.0-or-later
#include "mapeditor/model/planes.h"

#include <cmath>
#include <vector>

#include "mapeditor/model/sector_tri.h"

namespace elads::map {
namespace {
constexpr double kEps = 1e-9;

// Slope thing editor numbers (GZDoom): 3 of a kind per sector define the plane.
constexpr int kSlopeFloorThing = 9500;
constexpr int kSlopeCeilThing = 9501;

double triSign(util::Vec2 p1, util::Vec2 p2, util::Vec2 p3) {
    return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
}
bool pointInTri(util::Vec2 p, util::Vec2 a, util::Vec2 b, util::Vec2 c) {
    const double d1 = triSign(p, a, b), d2 = triSign(p, b, c), d3 = triSign(p, c, a);
    const bool neg = d1 < 0 || d2 < 0 || d3 < 0;
    const bool pos = d1 > 0 || d2 > 0 || d3 > 0;
    return !(neg && pos);
}
} // namespace

double Plane::heightAt(double x, double y) const {
    if (std::fabs(c) < kEps)
        return -d; // degenerate; shouldn't happen for floor/ceiling planes
    return -(a * x + b * y + d) / c;
}

Plane Plane::flat(double z) { return {0.0, 0.0, 1.0, -z}; }

Plane Plane::fromPoints(double x0, double y0, double z0, double x1, double y1, double z1, double x2,
                        double y2, double z2) {
    // Normal = (p1 - p0) x (p2 - p0).
    const double ux = x1 - x0, uy = y1 - y0, uz = z1 - z0;
    const double vx = x2 - x0, vy = y2 - y0, vz = z2 - z0;
    double a = uy * vz - uz * vy;
    double b = uz * vx - ux * vz;
    double c = ux * vy - uy * vx;
    if (std::fabs(c) < kEps)
        return Plane::flat((z0 + z1 + z2) / 3.0); // collinear / vertical → treat as flat
    if (c < 0.0) {                                // orient the normal upward
        a = -a;
        b = -b;
        c = -c;
    }
    const double d = -(a * x0 + b * y0 + c * z0);
    return {a, b, c, d};
}

int sectorAt(const MapModel& m, double x, double y) {
    const util::Vec2 p{x, y};
    for (int s = 0; s < static_cast<int>(m.sectorCount()); ++s) {
        const Triangulation t = triangulateSector(m, s);
        for (size_t i = 0; i + 3 <= t.indices.size(); i += 3) {
            if (pointInTri(p, t.points[t.indices[i]], t.points[t.indices[i + 1]],
                           t.points[t.indices[i + 2]]))
                return s;
        }
    }
    return kNoRef;
}

std::vector<SectorPlanes> computeSectorPlanes(const MapModel& m) {
    const int nsec = static_cast<int>(m.sectorCount());
    std::vector<SectorPlanes> planes(static_cast<size_t>(nsec));
    for (int s = 0; s < nsec; ++s) {
        planes[static_cast<size_t>(s)].floor = Plane::flat(m.sector(s).floorHeight);
        planes[static_cast<size_t>(s)].ceil = Plane::flat(m.sector(s).ceilHeight);
    }

    // Collect slope-thing points per sector.
    struct Pt {
        double x, y, z;
    };
    std::vector<std::vector<Pt>> floorPts(static_cast<size_t>(nsec)), ceilPts(static_cast<size_t>(nsec));
    for (int i = 0; i < static_cast<int>(m.thingCount()); ++i) {
        const Thing& th = m.thing(i);
        if (th.type != kSlopeFloorThing && th.type != kSlopeCeilThing)
            continue;
        const int s = sectorAt(m, th.pos.x, th.pos.y);
        if (s == kNoRef)
            continue;
        auto& bucket = (th.type == kSlopeFloorThing) ? floorPts : ceilPts;
        bucket[static_cast<size_t>(s)].push_back({th.pos.x, th.pos.y, th.z});
    }

    for (int s = 0; s < nsec; ++s) {
        const auto& fp = floorPts[static_cast<size_t>(s)];
        if (fp.size() >= 3)
            planes[static_cast<size_t>(s)].floor =
                Plane::fromPoints(fp[0].x, fp[0].y, fp[0].z, fp[1].x, fp[1].y, fp[1].z, fp[2].x,
                                  fp[2].y, fp[2].z);
        const auto& cp = ceilPts[static_cast<size_t>(s)];
        if (cp.size() >= 3)
            planes[static_cast<size_t>(s)].ceil =
                Plane::fromPoints(cp[0].x, cp[0].y, cp[0].z, cp[1].x, cp[1].y, cp[1].z, cp[2].x,
                                  cp[2].y, cp[2].z);
    }
    return planes;
}

} // namespace elads::map
