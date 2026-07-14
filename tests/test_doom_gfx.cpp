// SPDX-License-Identifier: GPL-3.0-or-later
// Encode an RGBA image (with transparency) to Doom picture format and decode it back.
#include "check.h"
#include "graphics/doom_gfx.h"
#include "graphics/palette.h"

using namespace elads;

static void run() {
    const gfx::Palette pal = gfx::Palette::testRamp(); // color(i).r == i, so nearest() is exact

    // 5x4 image: checkerboard of opaque palette colors over transparent gaps.
    gfx::Image src(5, 4);
    for (int y = 0; y < src.height; ++y)
        for (int x = 0; x < src.width; ++x)
            if ((x + y) % 2 == 0) {
                const int idx = (y * src.width + x) & 0xFF;
                const gfx::Rgb c = pal.color(idx);
                src.set(x, y, c.r, c.g, c.b, 255);
            }

    gfx::PictureOffsets off{-2, 3};
    const util::Bytes lump = gfx::encodeDoomGfx(src, pal, off);

    CHECK(gfx::looksLikeDoomGfx(lump));

    gfx::PictureOffsets got{};
    const gfx::Image dst = gfx::decodeDoomGfx(lump, pal, &got);

    CHECK_EQ(got.left, -2);
    CHECK_EQ(got.top, 3);
    CHECK(dst.width == 5 && dst.height == 4);
    CHECK(src == dst); // exact RGBA round-trip

    // Spot-check: a transparent pixel stays transparent, an opaque one keeps its color.
    CHECK(!dst.opaque(1, 0)); // (1+0) odd -> gap
    CHECK(dst.opaque(0, 0));  // (0+0) even -> opaque
    const gfx::Rgb c00 = pal.color(0);
    CHECK(dst.pixel(0, 0)[0] == c00.r && dst.pixel(0, 0)[3] == 255);

    // Garbage is rejected by the detector.
    CHECK(!gfx::looksLikeDoomGfx(util::Bytes{1, 2, 3}));
}

TEST_MAIN(run())
