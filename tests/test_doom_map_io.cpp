// SPDX-License-Identifier: GPL-3.0-or-later
// Round-trip a MapModel through classic Doom binary lumps (via a WAD) and back.
#include "archive/wad.h"
#include "check.h"
#include "mapeditor/model/doom_map_io.h"

using namespace elads;

static map::MapModel makeSquare() {
    map::MapModel m;
    m.addVertex({0, 0});
    m.addVertex({64, 0});
    m.addVertex({64, 64});
    m.addVertex({0, 64});

    map::Sector sec;
    sec.floorHeight = 0;
    sec.ceilHeight = 128;
    sec.floorTex = "FLOOR4_8";
    sec.ceilTex = "CEIL3_5";
    sec.lightLevel = 176;
    sec.tag = 7;
    m.addSector(sec);

    for (int i = 0; i < 4; ++i) {
        map::Sidedef sd;
        sd.sector = 0;
        sd.middle = "STARTAN2";
        m.addSidedef(sd);
    }
    const int e[4][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}};
    for (int i = 0; i < 4; ++i) {
        map::Linedef l;
        l.v1 = e[i][0];
        l.v2 = e[i][1];
        l.front = i;
        l.flags = 1; // impassable
        m.addLinedef(l);
    }
    map::Thing player;
    player.pos = {32, 32};
    player.type = 1; // Player 1 start
    player.angle = 90;
    player.flags = 7;
    m.addThing(player);
    return m;
}

static void run() {
    const map::MapModel src = makeSquare();

    // Serialize to the 5 editable lumps and wrap them in a PWAD behind a MAP01 marker.
    archive::Wad wad;
    wad.add("MAP01");
    for (auto& l : map::writeDoomMap(src))
        wad.add(l.name, l.data);

    const util::Bytes bytes = wad.write();
    const archive::Wad back = archive::Wad::read(bytes);

    // Map discovery finds MAP01 as a binary map.
    const auto maps = map::findMaps(back);
    CHECK_EQ(maps.size(), static_cast<size_t>(1));
    CHECK_EQ(maps[0].name, std::string("MAP01"));
    CHECK(!maps[0].udmf);

    const map::MapModel dst = map::readDoomMap(map::mapLumps(back, maps[0].marker));

    CHECK_EQ(dst.vertexCount(), src.vertexCount());
    CHECK_EQ(dst.linedefCount(), src.linedefCount());
    CHECK_EQ(dst.sidedefCount(), src.sidedefCount());
    CHECK_EQ(dst.sectorCount(), src.sectorCount());
    CHECK_EQ(dst.thingCount(), src.thingCount());

    // Spot-check fields survived the binary round-trip.
    CHECK(dst.vertex(2).pos == (util::Vec2{64, 64}));
    CHECK_EQ(dst.sector(0).floorTex, std::string("FLOOR4_8"));
    CHECK_EQ(dst.sector(0).ceilTex, std::string("CEIL3_5"));
    CHECK_EQ(dst.sector(0).lightLevel, 176);
    CHECK_EQ(dst.sector(0).tag, 7);
    CHECK_EQ(dst.sidedef(0).middle, std::string("STARTAN2"));
    CHECK_EQ(dst.linedef(1).v1, 1);
    CHECK_EQ(dst.linedef(1).v2, 2);
    CHECK_EQ(dst.linedef(1).front, 1);
    CHECK_EQ(dst.linedef(0).back, map::kNoRef); // one-sided
    CHECK_EQ(dst.thing(0).type, 1);
    CHECK_EQ(dst.thing(0).angle, 90);
    CHECK(dst.thing(0).pos == (util::Vec2{32, 32}));
}

TEST_MAIN(run())
