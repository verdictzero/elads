// SPDX-License-Identifier: GPL-3.0-or-later
// Flat (floor/ceiling texture) encode/decode round-trip + size inference.
#include "check.h"
#include "graphics/flat.h"
#include "graphics/palette.h"

using namespace elads;

static void run() {
    const gfx::Palette pal = gfx::Palette::testRamp(); // exact reverse mapping

    // Size inference.
    CHECK(gfx::inferFlatDimensions(4096).known);
    CHECK_EQ(gfx::inferFlatDimensions(4096).width, 64);
    CHECK_EQ(gfx::inferFlatDimensions(4096).height, 64);
    CHECK(gfx::inferFlatDimensions(64000).width == 320 && gfx::inferFlatDimensions(64000).height == 200);
    CHECK(!gfx::inferFlatDimensions(12345).known);

    // Build an opaque 8x8 image from palette colors, round-trip through a raw flat.
    gfx::Image src(8, 8);
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) {
            const gfx::Rgb c = pal.color((y * 8 + x) & 0xFF);
            src.set(x, y, c.r, c.g, c.b, 255);
        }

    const util::Bytes raw = gfx::encodeFlat(src, pal);
    CHECK_EQ(raw.size(), static_cast<size_t>(64)); // 8*8, headerless

    const gfx::Image dst = gfx::decodeFlat(raw, pal, 8, 8);
    CHECK(src == dst);
    CHECK(dst.opaque(0, 0)); // flats are fully opaque
}

TEST_MAIN(run())
