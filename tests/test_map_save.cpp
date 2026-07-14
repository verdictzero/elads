// SPDX-License-Identifier: GPL-3.0-or-later
// Save-back (B4): serialize an edited MapModel into a WAD (binary + UDMF), replacing the map's
// data lumps in place while preserving the marker and all non-map lumps. Load->edit->save->reload
// must round-trip. Pure core — no GL.
#include "check.h"
#include "archive/wad.h"
#include "mapeditor/edit/map_edit.h"
#include "mapeditor/model/map_model.h"
#include "mapeditor/model/map_save.h"
#include "util/undo.h"

using namespace elads;

// A 256x256 one-sided square with one thing (integer coords survive binary int16 storage).
static map::MapModel square() {
    map::MapModel m;
    m.addVertex({0, 0});
    m.addVertex({256, 0});
    m.addVertex({256, 256});
    m.addVertex({0, 256});
    map::Sector sec;
    sec.floorHeight = 0;
    sec.ceilHeight = 128;
    sec.floorTex = "FLAT1";
    sec.ceilTex = "CEIL1";
    sec.lightLevel = 160;
    m.addSector(sec);
    const int e[4][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}};
    for (auto& ed : e) {
        map::Sidedef sd;
        sd.sector = 0;
        sd.middle = "WALL1";
        const int s = m.addSidedef(sd);
        map::Linedef l;
        l.v1 = ed[0];
        l.v2 = ed[1];
        l.front = s;
        m.addLinedef(l);
    }
    map::Thing t;
    t.pos = {64, 64};
    t.type = 1;
    t.angle = 90;
    m.addThing(t);
    return m;
}

static void testBinaryRoundTrip() {
    archive::Wad wad;
    // Bracket the map with unrelated lumps to prove they survive.
    wad.add("PLAYPAL", util::Bytes(768, 7));
    map::saveMapToWad(wad, "MAP01", square(), /*udmf=*/false);
    wad.add("F_END", {});

    // Serialize + reparse the whole WAD (exercises the on-disk byte layout too).
    const archive::Wad reloaded = archive::Wad::read(wad.write());
    CHECK(reloaded.find("PLAYPAL") != nullptr);
    CHECK(reloaded.find("F_END") != nullptr);
    CHECK(reloaded.find("MAP01") != nullptr);

    const map::MapModel m = map::loadMapFromWad(reloaded, "MAP01");
    CHECK_EQ(m.vertexCount(), static_cast<size_t>(4));
    CHECK_EQ(m.linedefCount(), static_cast<size_t>(4));
    CHECK_EQ(m.sidedefCount(), static_cast<size_t>(4));
    CHECK_EQ(m.sectorCount(), static_cast<size_t>(1));
    CHECK_EQ(m.thingCount(), static_cast<size_t>(1));
    CHECK(m.vertex(2).pos == util::Vec2(256, 256));
    CHECK_EQ(m.sector(0).ceilHeight, 128);
    CHECK_EQ(m.thing(0).type, 1);
    CHECK_EQ(m.thing(0).angle, 90);
}

static void testEditThenSaveReplacesInPlace() {
    archive::Wad wad;
    wad.add("PLAYPAL", util::Bytes(768, 3));
    map::saveMapToWad(wad, "MAP01", square(), false);
    wad.add("TAILLUMP", util::Bytes(4, 9));
    const size_t lumpsAfterFirstSave = wad.lumpCount();

    // Load, edit (move a vertex + raise the floor), save back into the SAME wad.
    map::MapModel m = map::loadMapFromWad(wad, "MAP01");
    util::UndoManager undo;
    edit::moveVertex(m, undo, 2, {300, 300});
    edit::setSectorHeights(m, undo, 0, 32, 160);
    map::saveMapToWad(wad, "MAP01", m, false);

    // Replacing in place must not change the lump count (same 5 binary lumps) and must keep
    // the bracketing lumps.
    CHECK_EQ(wad.lumpCount(), lumpsAfterFirstSave);
    CHECK(wad.find("PLAYPAL") != nullptr);
    CHECK(wad.find("TAILLUMP") != nullptr);

    const map::MapModel back = map::loadMapFromWad(wad, "MAP01");
    CHECK(back.vertex(2).pos == util::Vec2(300, 300));
    CHECK_EQ(back.sector(0).floorHeight, 32);
    CHECK_EQ(back.sector(0).ceilHeight, 160);
    // The TAILLUMP still follows the map, not swallowed into it.
    CHECK_EQ(back.vertexCount(), static_cast<size_t>(4));
}

static void testUdmfRoundTrip() {
    archive::Wad wad;
    map::saveMapToWad(wad, "MAP02", square(), /*udmf=*/true, "zdoom");
    // UDMF preserves floats: nudge a vertex to a fractional coordinate.
    map::MapModel m = map::loadMapFromWad(wad, "MAP02");
    util::UndoManager undo;
    edit::moveVertex(m, undo, 0, {-12.5, 7.25});
    map::saveMapToWad(wad, "MAP02", m, true, "zdoom");

    const archive::Wad reloaded = archive::Wad::read(wad.write());
    CHECK(reloaded.find("TEXTMAP") != nullptr);
    CHECK(reloaded.find("ENDMAP") != nullptr);
    const map::MapModel back = map::loadMapFromWad(reloaded, "MAP02");
    CHECK(back.vertex(0).pos == util::Vec2(-12.5, 7.25));
    CHECK_EQ(back.sectorCount(), static_cast<size_t>(1));
    CHECK_EQ(back.thingCount(), static_cast<size_t>(1));
}

static void testAppendNewMap() {
    archive::Wad wad;
    map::saveMapToWad(wad, "MAP01", square(), false);
    // Saving a second, different map name appends rather than overwriting the first.
    map::MapModel other = square();
    util::UndoManager undo;
    edit::setSectorHeights(other, undo, 0, 999, 1000);
    map::saveMapToWad(wad, "MAP03", other, false);

    CHECK(map::loadMapFromWad(wad, "MAP01").sector(0).floorHeight == 0);
    CHECK(map::loadMapFromWad(wad, "MAP03").sector(0).floorHeight == 999);
}

static void run() {
    testBinaryRoundTrip();
    testEditThenSaveReplacesInPlace();
    testUdmfRoundTrip();
    testAppendNewMap();
}

TEST_MAIN(run())
