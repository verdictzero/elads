// SPDX-License-Identifier: GPL-3.0-or-later
// Round-trip PNAMES and TEXTUREx (composite texture definitions).
#include "check.h"
#include "graphics/texturex.h"

using namespace elads;

static void run() {
    // --- PNAMES ---
    const std::vector<std::string> pnames = {"WALL01", "WALL02", "DOOR3"};
    const std::vector<std::string> pback = gfx::parsePnames(gfx::writePnames(pnames));
    CHECK_EQ(pback.size(), pnames.size());
    CHECK_EQ(pback[0], std::string("WALL01"));
    CHECK_EQ(pback[2], std::string("DOOR3"));

    // --- TEXTUREx ---
    std::vector<gfx::TextureDef> texs;
    gfx::TextureDef big;
    big.name = "BIGWALL";
    big.width = 128;
    big.height = 128;
    big.patches = {{0, 0, 0}, {64, 0, 1}};
    texs.push_back(big);

    gfx::TextureDef door;
    door.name = "SMALLDR";
    door.width = 32;
    door.height = 72;
    door.patches = {{0, -8, 2}};
    texs.push_back(door);

    const std::vector<gfx::TextureDef> back = gfx::parseTextureX(gfx::writeTextureX(texs));

    CHECK_EQ(back.size(), static_cast<size_t>(2));

    CHECK_EQ(back[0].name, std::string("BIGWALL"));
    CHECK_EQ(back[0].width, 128);
    CHECK_EQ(back[0].height, 128);
    CHECK_EQ(back[0].patches.size(), static_cast<size_t>(2));
    CHECK_EQ(back[0].patches[1].originX, 64);
    CHECK_EQ(back[0].patches[1].patchIndex, 1);

    CHECK_EQ(back[1].name, std::string("SMALLDR"));
    CHECK_EQ(back[1].height, 72);
    CHECK_EQ(back[1].patches.size(), static_cast<size_t>(1));
    CHECK_EQ(back[1].patches[0].originY, -8); // signed offset survives
    CHECK_EQ(back[1].patches[0].patchIndex, 2);
}

TEST_MAIN(run())
