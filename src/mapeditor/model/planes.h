// SPDX-License-Identifier: GPL-3.0-or-later
// elads — sector floor/ceiling planes (slopes).
//
// A sector's floor and ceiling are usually flat (a single Z), but GZDoom lets them tilt.
// This module represents each surface as a plane and derives sloped planes from the map's
// slope sources, so the 3D view can evaluate a per-point height. Sources are applied in the
// order GZDoom resolves them; see docs/design/11-udmf-advanced.md and the A1 item in
// docs/implementation-plan.md. World mapping: X = map X, Y (up) = height, Z = map Y.
#pragma once

#include <vector>

#include "mapeditor/model/map_model.h"
#include "util/geometry.h"

namespace elads::map {

// A plane a*x + b*y + c*z + d = 0 evaluated as a height field z = f(x, y). `c` is kept
// non-zero (a vertical plane degenerates to flat) so heightAt is always defined.
struct Plane {
    double a = 0.0, b = 0.0, c = 1.0, d = 0.0;

    // Height (Z, i.e. map-space up) of the plane at map coordinate (x, y).
    double heightAt(double x, double y) const { return -(a * x + b * y + d) / c; }
    double heightAt(util::Vec2 p) const { return heightAt(p.x, p.y); }

    // A flat (horizontal) plane at constant height z.
    static Plane flat(double z) { return {0.0, 0.0, 1.0, -z}; }

    // Plane through three points (each a 2D map position + a height). Oriented with an upward
    // normal (c > 0). Degenerate/collinear/vertical inputs fall back to flat(z0).
    static Plane fromPoints(util::Vec2 p0, double z0, util::Vec2 p1, double z1, util::Vec2 p2,
                            double z2);

    // True when the plane is horizontal (no tilt).
    bool isFlat() const { return a == 0.0 && b == 0.0; }
};

struct SectorPlanes {
    Plane floor;
    Plane ceil;
};

// Floor/ceiling planes for every sector, indexed by sector. Flat (from floorHeight/ceilHeight)
// unless a slope source applies. Implemented sources, in resolution order:
//   1. Slope things — 9500 (floor), 9501 (ceiling): >=3 per sector define the plane.
//   2. Plane_Align (special 181): the line is a hinge; the chosen sector's surface tilts to
//      meet the neighbouring sector's height at the line.
// UDMF vertex zfloor/zceiling and Plane_Copy (118) are not yet handled (see the plan).
// Note: the slope-thing path calls sectorAt (which triangulates), so callers that render every
// frame should cache the result and recompute only on edit rather than call this per frame.
std::vector<SectorPlanes> computeSectorPlanes(const MapModel&);

// Index of the sector whose floor polygon contains map point (x, y), or kNoRef. Uses the
// sector triangulation (so it respects real, possibly non-convex, sector shapes).
int sectorAt(const MapModel&, double x, double y);

} // namespace elads::map
