// SPDX-License-Identifier: GPL-3.0-or-later
// Thing billboard corner math (A5): upright, camera-facing quad corners. Pure — no GL.
#include <cmath>

#include "check.h"
#include "mapeditor/view3d/map_view_3d.h"

using namespace elads;

static bool near(double a, double b, double eps = 1e-6) { return std::fabs(a - b) < eps; }

static void run() {
    double c[4][3];

    // yaw = 0: the "right" axis is world +X, so the quad spans ±halfW in X at constant Z.
    view::billboardCorners(100, 10, 200, 0.f, 16.0, 56.0, c);
    // bottom-left, bottom-right, top-left, top-right
    CHECK(near(c[0][0], 84) && near(c[0][1], 10) && near(c[0][2], 200));  // bl
    CHECK(near(c[1][0], 116) && near(c[1][1], 10) && near(c[1][2], 200)); // br
    CHECK(near(c[2][0], 84) && near(c[2][1], 66) && near(c[2][2], 200));  // tl (baseY + height)
    CHECK(near(c[3][0], 116) && near(c[3][1], 66) && near(c[3][2], 200)); // tr

    // Bottoms share baseY; tops share baseY + height; width is 2*halfW.
    CHECK(near(c[0][1], c[1][1]));
    CHECK(near(c[2][1], c[3][1]));
    CHECK(near(c[1][0] - c[0][0], 32));

    // yaw = pi/2: the "right" axis rotates to world -Z, so the quad spans in Z, constant X.
    view::billboardCorners(100, 0, 200, static_cast<float>(M_PI / 2), 16.0, 56.0, c);
    CHECK(near(c[0][0], 100) && near(c[1][0], 100)); // X constant
    CHECK(near(c[0][2], 216) && near(c[1][2], 184)); // spans ±16 in Z
    CHECK(near(c[2][1], 56));                         // top row height
}

TEST_MAIN(run())
