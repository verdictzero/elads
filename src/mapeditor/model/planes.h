// SPDX-License-Identifier: GPL-3.0-or-later
// elads — sloped sector planes (UDMF slopes; see docs/design/11-udmf-advanced.md §1).
//
// A sector's floor and ceiling are planes; flat sectors are just horizontal planes. Slopes are
// derived from slope things (types 9500 floor / 9501 ceiling) that give 3 points per plane;
// more sources (Plane_Align, vertex heights) are planned. The 3D renderer evaluates heights
// per (x,y) so floors/ceilings/walls follow the slope.
#pragma once

#include <vector>

#include "mapeditor/model/map_model.h"

namespace elads::map {

// Plane a*x + b*y + c*z + d = 0, solved for z. For flat planes c = 1.
struct Plane {
    double a = 0.0;
    double b = 0.0;
    double c = 1.0;
    double d = 0.0;

    double heightAt(double x, double y) const;
    static Plane flat(double z);
    static Plane fromPoints(double x0, double y0, double z0, double x1, double y1, double z1,
                            double x2, double y2, double z2);
    bool sloped() const { return a != 0.0 || b != 0.0; }
};

struct SectorPlanes {
    Plane floor;
    Plane ceil;
};

// Floor/ceiling planes for every sector (flat by default; sloped where slope things apply).
std::vector<SectorPlanes> computeSectorPlanes(const MapModel&);

// Index of the sector containing point (x,y), or kNoRef if none (via sector triangulation).
int sectorAt(const MapModel&, double x, double y);

} // namespace elads::map
