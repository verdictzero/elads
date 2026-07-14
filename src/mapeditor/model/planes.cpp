// SPDX-License-Identifier: GPL-3.0-or-later
#include "mapeditor/model/planes.h"

#include <cmath>

#include "mapeditor/model/sector_tri.h"

namespace elads::map {
namespace {

// Perpendicular distance from p to the infinite line through a,b (0 if a==b).
double distToLine(util::Vec2 p, util::Vec2 a, util::Vec2 b) {
    const util::Vec2 ab = b - a;
    const double len = ab.length();
    if (len < 1e-9)
        return (p - a).length();
    return std::fabs((p - a).cross(ab)) / len;
}

// Point-in-triangle via consistent edge-sign test (inclusive of edges).
bool pointInTri(util::Vec2 p, util::Vec2 a, util::Vec2 b, util::Vec2 c) {
    const double d1 = (p - b).cross(a - b);
    const double d2 = (p - c).cross(b - c);
    const double d3 = (p - a).cross(c - a);
    const bool neg = (d1 < 0.0) || (d2 < 0.0) || (d3 < 0.0);
    const bool pos = (d1 > 0.0) || (d2 > 0.0) || (d3 > 0.0);
    return !(neg && pos);
}

double surfaceHeight(const MapModel& m, int sector, bool floor) {
    const Sector& s = m.sector(sector);
    return floor ? s.floorHeight : s.ceilHeight;
}

// Plane_Align (181): tilt `arg`'s sector surface to hinge along line `l`. arg==1 slopes the
// front sector, arg==2 the back sector; the opposite sector is the height model. The hinge
// vertices take the model sector's height; the farthest vertex of the sloped sector keeps its
// own nominal height (matching GZDoom's P_AlignPlane).
void applyAlign(const MapModel& m, std::vector<SectorPlanes>& planes, const Linedef& l, int fs,
                int bs, bool floor, int arg) {
    int sloped, model;
    if (arg == 1) {
        sloped = fs;
        model = bs;
    } else if (arg == 2) {
        sloped = bs;
        model = fs;
    } else {
        return;
    }

    const util::Vec2 v1 = m.vertex(l.v1).pos;
    const util::Vec2 v2 = m.vertex(l.v2).pos;

    // Farthest vertex of the sloped sector from the hinge line.
    util::Vec2 far = v1;
    double bestDist = -1.0;
    for (int i = 0; i < static_cast<int>(m.linedefCount()); ++i) {
        const Linedef& ll = m.linedef(i);
        if (ll.v1 == kNoRef || ll.v2 == kNoRef)
            continue;
        if (m.frontSector(ll) != sloped && m.backSector(ll) != sloped)
            continue;
        for (int vi : {ll.v1, ll.v2}) {
            const util::Vec2 p = m.vertex(vi).pos;
            const double d = distToLine(p, v1, v2);
            if (d > bestDist) {
                bestDist = d;
                far = p;
            }
        }
    }
    if (bestDist <= 1e-6)
        return; // no vertex off the hinge => nothing to tilt

    const double modelH = surfaceHeight(m, model, floor);
    const double slopedH = surfaceHeight(m, sloped, floor);
    const Plane p = Plane::fromPoints(v1, modelH, v2, modelH, far, slopedH);
    (floor ? planes[sloped].floor : planes[sloped].ceil) = p;
}

} // namespace

Plane Plane::fromPoints(util::Vec2 p0, double z0, util::Vec2 p1, double z1, util::Vec2 p2,
                        double z2) {
    // Edge vectors in 3D (map X, map Y, height).
    const double ux = p1.x - p0.x, uy = p1.y - p0.y, uz = z1 - z0;
    const double vx = p2.x - p0.x, vy = p2.y - p0.y, vz = z2 - z0;
    // Normal = u x v.
    double nx = uy * vz - uz * vy;
    double ny = uz * vx - ux * vz;
    double nz = ux * vy - uy * vx;
    const double len = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (len < 1e-9 || std::fabs(nz) < 1e-9)
        return Plane::flat(z0); // collinear or vertical => cannot form a height field
    // Orient upward so c > 0 (heightAt is unaffected, but keeps a consistent normal).
    if (nz < 0.0) {
        nx = -nx;
        ny = -ny;
        nz = -nz;
    }
    nx /= len;
    ny /= len;
    nz /= len;
    const double d = -(nx * p0.x + ny * p0.y + nz * z0);
    return Plane{nx, ny, nz, d};
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
    if (nsec == 0)
        return planes;

    // --- Slope things (9500 floor, 9501 ceiling): >=3 in a sector define its plane. ---
    struct Pt {
        util::Vec2 p;
        double z;
    };
    std::vector<std::vector<Pt>> floorPts(static_cast<size_t>(nsec)),
        ceilPts(static_cast<size_t>(nsec));
    for (int i = 0; i < static_cast<int>(m.thingCount()); ++i) {
        const Thing& t = m.thing(i);
        if (t.type != 9500 && t.type != 9501)
            continue;
        const int s = sectorAt(m, t.pos.x, t.pos.y);
        if (s == kNoRef)
            continue;
        (t.type == 9500 ? floorPts : ceilPts)[static_cast<size_t>(s)].push_back({t.pos, t.z});
    }
    for (int s = 0; s < nsec; ++s) {
        const auto& fp = floorPts[static_cast<size_t>(s)];
        if (fp.size() >= 3)
            planes[static_cast<size_t>(s)].floor =
                Plane::fromPoints(fp[0].p, fp[0].z, fp[1].p, fp[1].z, fp[2].p, fp[2].z);
        const auto& cp = ceilPts[static_cast<size_t>(s)];
        if (cp.size() >= 3)
            planes[static_cast<size_t>(s)].ceil =
                Plane::fromPoints(cp[0].p, cp[0].z, cp[1].p, cp[1].z, cp[2].p, cp[2].z);
    }

    // --- Plane_Align (181): hinge the chosen sector's surface along the line. ---
    for (int i = 0; i < static_cast<int>(m.linedefCount()); ++i) {
        const Linedef& l = m.linedef(i);
        if (l.special != 181)
            continue;
        if (l.v1 == kNoRef || l.v2 == kNoRef)
            continue;
        const int fs = m.frontSector(l), bs = m.backSector(l);
        if (fs == kNoRef || bs == kNoRef)
            continue;
        applyAlign(m, planes, l, fs, bs, /*floor=*/true, l.args[0]);
        applyAlign(m, planes, l, fs, bs, /*floor=*/false, l.args[1]);
    }

    return planes;
}

} // namespace elads::map
