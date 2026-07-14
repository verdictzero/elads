// SPDX-License-Identifier: GPL-3.0-or-later
// Build a MaterialSet from a synthetic WAD (PLAYPAL + PNAMES + TEXTURE1 + patch + flat)
// and confirm the composite texture and the flat are resolved — no IWAD needed.
#include "archive/wad.h"
#include "check.h"
#include "graphics/doom_gfx.h"
#include "graphics/flat.h"
#include "graphics/palette.h"
#include "graphics/texturex.h"
#include "graphics/wad_materials.h"

using namespace elads;

static void run() {
    const gfx::Palette pal = gfx::Palette::testRamp();

    // A 16x16 patch, encoded as a Doom picture.
    gfx::Image patch(16, 16);
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x) {
            const gfx::Rgb c = pal.color((x + y * 16) & 0xFF);
            patch.set(x, y, c.r, c.g, c.b, 255);
        }
    const util::Bytes patchLump = gfx::encodeDoomGfx(patch, pal);

    // A 64x64 flat.
    gfx::Image flatImg(64, 64);
    for (int i = 0; i < 64 * 64; ++i)
        flatImg.set(i % 64, i / 64, 30, 60, 90, 255);
    const util::Bytes flatLump = gfx::encodeFlat(flatImg, pal);

    // PNAMES + TEXTURE1 referencing the patch.
    const util::Bytes pnames = gfx::writePnames({"PATCH0"});
    gfx::TextureDef def;
    def.name = "WALL01";
    def.width = 64;
    def.height = 128;
    def.patches = {{0, 0, 0}};
    const util::Bytes tex1 = gfx::writeTextureX({def});

    // Assemble the WAD.
    archive::Wad wad;
    wad.add("PLAYPAL", pal.toPlaypal());
    wad.add("PNAMES", pnames);
    wad.add("TEXTURE1", tex1);
    wad.add("PATCH0", patchLump);
    wad.add("F_START");
    wad.add("FLOOR5_1", flatLump);
    wad.add("F_END");

    const gfx::MaterialSet mats = gfx::buildMaterialSetFromWad(wad);

    const gfx::Image* wall = mats.find("WALL01");
    CHECK(wall != nullptr);
    if (wall) {
        CHECK_EQ(wall->width, 64);
        CHECK_EQ(wall->height, 128);
        // The patch pixel (0,0) should have been composited into the texture.
        CHECK(wall->opaque(0, 0));
    }

    const gfx::Image* flat = mats.find("floor5_1"); // case-insensitive
    CHECK(flat != nullptr);
    if (flat) {
        CHECK_EQ(flat->width, 64);
        CHECK_EQ(flat->height, 64);
    }

    CHECK(mats.find("NOPE") == nullptr);
    CHECK(mats.find("-") == nullptr);
}

TEST_MAIN(run())
