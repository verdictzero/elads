// SPDX-License-Identifier: GPL-3.0-or-later
// Hexen binary map format (D2): 20-byte THINGS (tid/z/special/args) + 16-byte LINEDEFS
// (special+args), detected by a BEHAVIOR lump. Round-trips through writeMap/readDoomMap, and
// save-back preserves the format + compiled ACS. Pure core — no GL.
#include "check.h"
#include "archive/wad.h"
#include "mapeditor/edit/map_edit.h"
#include "mapeditor/model/doom_map_io.h"
#include "mapeditor/model/map_model.h"
#include "mapeditor/model/map_save.h"
#include "util/undo.h"

using namespace elads;

// A tiny map exercising Hexen-only fields.
static map::MapModel hexenModel() {
    map::MapModel m;
    m.addVertex({0, 0});
    m.addVertex({128, 0});
    m.addVertex({128, 128});
    m.addVertex({0, 128});
    map::Sector sec;
    sec.floorHeight = 0;
    sec.ceilHeight = 128;
    m.addSector(sec);
    for (int i = 0; i < 4; ++i) {
        map::Sidedef sd;
        sd.sector = 0;
        const int side = m.addSidedef(sd);
        map::Linedef l;
        l.v1 = i;
        l.v2 = (i + 1) % 4;
        l.front = side;
        l.flags = 0x0080;      // e.g. ML_REPEAT_SPECIAL
        l.special = 181;       // Plane_Align
        l.args = {1, 2, 3, 4, 5};
        m.addLinedef(l);
    }
    map::Thing t;
    t.pos = {64, 96};
    t.z = 48;
    t.angle = 90;
    t.type = 3001;
    t.flags = 0x07;
    t.tid = 42;
    t.special = 80;            // ACS_Execute
    t.args = {10, 20, 30, 40, 50};
    m.addThing(t);
    return m;
}

static void testFormatAndSizes() {
    const map::MapModel m = hexenModel();
    const auto hx = map::writeMap(m, map::MapFormat::Hexen);
    const auto dm = map::writeMap(m, map::MapFormat::Doom);

    auto find = [](const std::vector<archive::Lump>& ls, const char* n) -> const archive::Lump* {
        for (const auto& l : ls)
            if (l.name == n)
                return &l;
        return nullptr;
    };
    // Hexen: 20-byte things, 16-byte linedefs, and a BEHAVIOR marker.
    CHECK(find(hx, "BEHAVIOR") != nullptr);
    CHECK_EQ(find(hx, "THINGS")->data.size(), static_cast<size_t>(20)); // 1 thing
    CHECK_EQ(find(hx, "LINEDEFS")->data.size(), static_cast<size_t>(4 * 16));
    CHECK(map::detectMapFormat(hx) == map::MapFormat::Hexen);

    // Doom: 10-byte things, 14-byte linedefs, no BEHAVIOR.
    CHECK(find(dm, "BEHAVIOR") == nullptr);
    CHECK_EQ(find(dm, "THINGS")->data.size(), static_cast<size_t>(10));
    CHECK_EQ(find(dm, "LINEDEFS")->data.size(), static_cast<size_t>(4 * 14));
    CHECK(map::detectMapFormat(dm) == map::MapFormat::Doom);
}

static void testRoundTrip() {
    const map::MapModel m = hexenModel();
    const map::MapModel back = map::readDoomMap(map::writeMap(m, map::MapFormat::Hexen));

    CHECK_EQ(back.thingCount(), static_cast<size_t>(1));
    const map::Thing& t = back.thing(0);
    CHECK(t.pos == util::Vec2(64, 96));
    CHECK_EQ(t.z, 48.0);
    CHECK_EQ(t.angle, 90);
    CHECK_EQ(t.type, 3001);
    CHECK_EQ(t.flags, 0x07);
    CHECK_EQ(t.tid, 42);
    CHECK_EQ(t.special, 80);
    CHECK(t.args == map::Args({10, 20, 30, 40, 50}));

    const map::Linedef& l = back.linedef(0);
    CHECK_EQ(l.special, 181);
    CHECK_EQ(l.flags, 0x0080);
    CHECK(l.args == map::Args({1, 2, 3, 4, 5}));
    CHECK_EQ(l.v1, 0);
    CHECK_EQ(l.v2, 1);
}

// A Hexen map with a non-empty BEHAVIOR, loaded/edited/saved, must stay Hexen with ACS intact.
static void testSavePreservesHexen() {
    archive::Wad wad;
    wad.add("MAP01");
    for (const auto& l : map::writeMap(hexenModel(), map::MapFormat::Hexen)) {
        if (l.name == "BEHAVIOR")
            wad.add("BEHAVIOR", util::Bytes{'A', 'C', 'S', 0}); // pretend-compiled ACS
        else
            wad.add(l.name, l.data);
    }

    map::MapModel m = map::loadMapFromWad(wad, "MAP01");
    CHECK_EQ(m.thing(0).special, 80); // Hexen fields survived the load
    util::UndoManager undo;
    edit::setSectorHeights(m, undo, 0, 8, 120);
    map::saveMapToWad(wad, "MAP01", m, /*udmf=*/false);

    // Reload: still Hexen, ACS preserved, edit applied, Hexen fields intact.
    const auto lumps = map::mapLumps(wad, wad.indexOf("MAP01"));
    CHECK(map::detectMapFormat(lumps) == map::MapFormat::Hexen);
    const archive::Lump* beh = wad.find("BEHAVIOR");
    CHECK(beh != nullptr && beh->data.size() == 4); // ACS bytes kept, not blanked

    const map::MapModel back = map::loadMapFromWad(wad, "MAP01");
    CHECK_EQ(back.sector(0).floorHeight, 8);
    CHECK_EQ(back.thing(0).tid, 42);
    CHECK_EQ(back.linedef(0).special, 181);
    CHECK(back.linedef(0).args == map::Args({1, 2, 3, 4, 5}));
}

static void run() {
    testFormatAndSizes();
    testRoundTrip();
    testSavePreservesHexen();
}

TEST_MAIN(run())
