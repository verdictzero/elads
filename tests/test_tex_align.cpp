// SPDX-License-Identifier: GPL-3.0-or-later
// Wall texture alignment (A2): U from distance + X offset; V from peg-derived texture-top Z + Y
// offset; peg-rule anchoring for one-sided / upper / lower. Pure math — no GL.
#include <cmath>

#include "check.h"
#include "mapeditor/model/tex_align.h"

using namespace elads;

static bool near(double a, double b, double eps = 1e-9) { return std::fabs(a - b) < eps; }

static void testU() {
    // U = (offsetX + dist) / w.
    CHECK(near(map::texU(0, 64, 0), 0.0));
    CHECK(near(map::texU(64, 64, 0), 1.0));
    CHECK(near(map::texU(0, 64, 16), 0.25)); // +16px X offset on a 64-wide texture
    CHECK(near(map::texU(96, 64, 0), 1.5));  // tiles past 1.0
}

static void testVandYOffset() {
    // V increases downward from the texture-top Z; +Y offset shifts V by offsetY/h.
    const double topZ = 128, h = 128;
    CHECK(near(map::texV(128, topZ, h, 0), 0.0)); // at the top row
    CHECK(near(map::texV(0, topZ, h, 0), 1.0));   // 128 units down = one tile
    CHECK(near(map::texV(64, topZ, h, 0), 0.5));
    CHECK(near(map::texV(128, topZ, h, 16), 16.0 / 128.0)); // Y offset shifts V
}

// One-sided middle: pegged -> top of texture at the ceiling; unpegged(bottom) -> bottom at floor.
static void testOneSidedPeg() {
    const double floorZ = 0, ceilZ = 96, h = 128;
    const double pegged = map::pegTopZ(map::WallPart::OneSidedMiddle, floorZ, ceilZ, ceilZ, h,
                                       /*pegTop=*/false, /*pegBot=*/false);
    CHECK(near(pegged, ceilZ));                        // texture top at the ceiling
    CHECK(near(map::texV(ceilZ, pegged, h, 0), 0.0));  // top row at ceiling

    const double unpeg = map::pegTopZ(map::WallPart::OneSidedMiddle, floorZ, ceilZ, ceilZ, h, false,
                                      /*pegBot=*/true);
    CHECK(near(unpeg, floorZ + h));                    // texture bottom at the floor
    CHECK(near(map::texV(floorZ, unpeg, h, 0), 1.0));  // bottom row (V=1) at floor
}

// Upper: pegged(default) -> bottom of texture at the lower ceiling; unpegged(top) -> top at higher.
static void testUpperPeg() {
    const double lowCeil = 64, highCeil = 160, h = 128; // section [64,160]
    const double def = map::pegTopZ(map::WallPart::Upper, lowCeil, highCeil, 0, h, false, false);
    CHECK(near(def, lowCeil + h));                     // bottom anchored to lower ceiling
    CHECK(near(map::texV(lowCeil, def, h, 0), 1.0));   // V=1 (texture bottom) at the lower ceiling

    const double pegTop = map::pegTopZ(map::WallPart::Upper, lowCeil, highCeil, 0, h, true, false);
    CHECK(near(pegTop, highCeil));                     // top anchored to the higher ceiling
    CHECK(near(map::texV(highCeil, pegTop, h, 0), 0.0));
}

// Lower: pegged(default) tiles from the front ceiling; unpegged(bottom) -> bottom at lower floor.
static void testLowerPeg() {
    const double lowFloor = 0, highFloor = 32, frontCeil = 128, h = 64; // step [0,32]
    const double def = map::pegTopZ(map::WallPart::Lower, lowFloor, highFloor, frontCeil, h, false,
                                    false);
    CHECK(near(def, frontCeil)); // texture-top continues from the front ceiling
    CHECK(near(map::texV(frontCeil, def, h, 0), 0.0));

    const double unpeg = map::pegTopZ(map::WallPart::Lower, lowFloor, highFloor, frontCeil, h, false,
                                      true);
    CHECK(near(unpeg, lowFloor + h));                  // bottom anchored to the lower floor
    CHECK(near(map::texV(lowFloor, unpeg, h, 0), 1.0));
}

static void run() {
    testU();
    testVandYOffset();
    testOneSidedPeg();
    testUpperPeg();
    testLowerPeg();
}

TEST_MAIN(run())
