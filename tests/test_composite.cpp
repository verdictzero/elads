// SPDX-License-Identifier: GPL-3.0-or-later
// Composite a TEXTUREx definition from patches: offsets, overlap, and transparency.
#include "check.h"
#include "graphics/composite.h"

using namespace elads;

static gfx::Image solid(int w, int h, uint8_t r, uint8_t g, uint8_t b) {
    gfx::Image img(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            img.set(x, y, r, g, b, 255);
    return img;
}

static void run() {
    // Patch 0: solid red 8x8. Patch 1: solid green 8x8 with a transparent hole at (3..4,3..4).
    const gfx::Image red = solid(8, 8, 255, 0, 0);
    gfx::Image green = solid(8, 8, 0, 255, 0);
    for (int y = 3; y <= 4; ++y)
        for (int x = 3; x <= 4; ++x)
            green.set(x, y, 0, 0, 0, 0); // punch a hole

    const std::vector<std::string> pnames = {"PRED", "PGRN"};
    auto lookup = [&](const std::string& n) -> const gfx::Image* {
        if (n == "PRED") return &red;
        if (n == "PGRN") return &green;
        return nullptr;
    };

    // 12x8 texture: red at x=0, green at x=4 (green overlaps red on x[4,8)).
    gfx::TextureDef def;
    def.name = "COMBO";
    def.width = 12;
    def.height = 8;
    def.patches = {{0, 0, 0}, {4, 0, 1}};

    const gfx::Image out = gfx::assembleTexture(def, pnames, lookup);
    CHECK(out.width == 12 && out.height == 8);

    // Red-only region.
    CHECK(out.pixel(0, 0)[0] == 255 && out.pixel(0, 0)[1] == 0 && out.opaque(0, 0));
    // Green drawn over red (green is opaque here) -> green wins.
    CHECK(out.pixel(4, 0)[1] == 255 && out.pixel(4, 0)[0] == 0);
    // Green-only tail.
    CHECK(out.pixel(11, 0)[1] == 255 && out.opaque(11, 0));
    // Green's hole over red (dest x=7,y=3): red shows through.
    CHECK(out.pixel(7, 3)[0] == 255 && out.pixel(7, 3)[1] == 0 && out.opaque(7, 3));
    // Green's hole past red's edge (dest x=8,y=3): nothing underneath -> transparent.
    CHECK(!out.opaque(8, 3));

    // A missing patch is simply skipped (no crash, leaves transparency).
    gfx::TextureDef bad;
    bad.width = 4;
    bad.height = 4;
    bad.patches = {{0, 0, 5}}; // patchIndex out of range
    const gfx::Image empty = gfx::assembleTexture(bad, pnames, lookup);
    CHECK(!empty.opaque(0, 0));
}

TEST_MAIN(run())
