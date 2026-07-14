// SPDX-License-Identifier: GPL-3.0-or-later
// Sector styling (A3): read lightcolor from a sector's UDMF extra keys. Pure — no GL.
#include <cmath>

#include "check.h"
#include "mapeditor/model/map_objects.h"
#include "mapeditor/model/sector_style.h"

using namespace elads;

static bool near(float a, float b, float eps = 1.0f / 255.f) { return std::fabs(a - b) < eps; }

static void run() {
    // 0xRRGGBB unpack.
    const map::ColorRGB red = map::colorFromInt(0xFF0000);
    CHECK(near(red.r, 1.f) && near(red.g, 0.f) && near(red.b, 0.f));
    const map::ColorRGB mix = map::colorFromInt(0x8040C0);
    CHECK(near(mix.r, 128 / 255.f) && near(mix.g, 64 / 255.f) && near(mix.b, 192 / 255.f));

    // Absent key -> default white.
    map::Sector s;
    const map::ColorRGB def = map::sectorLightColor(s);
    CHECK(near(def.r, 1.f) && near(def.g, 1.f) && near(def.b, 1.f));

    // Decimal value in extra (GZDoom stores lightcolor as a decimal int). 0xFF8040 = 16744512.
    s.extra.push_back({"lightcolor", "16744512"});
    const map::ColorRGB c = map::sectorLightColor(s);
    CHECK(near(c.r, 1.f) && near(c.g, 128 / 255.f) && near(c.b, 64 / 255.f));

    // 0x-hex value parses too.
    map::Sector s2;
    s2.extra.push_back({"lightcolor", "0x00FF00"});
    const map::ColorRGB g = map::sectorLightColor(s2);
    CHECK(near(g.r, 0.f) && near(g.g, 1.f) && near(g.b, 0.f));

    // A different key is ignored; explicit default is honored.
    map::Sector s3;
    s3.extra.push_back({"fadecolor", "255"});
    const map::ColorRGB d = map::readColorKey(s3.extra, "lightcolor", map::ColorRGB{0.5f, 0.5f, 0.5f});
    CHECK(near(d.r, 0.5f) && near(d.g, 0.5f) && near(d.b, 0.5f));
}

TEST_MAIN(run())
