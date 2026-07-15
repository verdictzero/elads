// SPDX-License-Identifier: GPL-3.0-or-later
// 3D floors (A4): Sector_Set3DFloor (160) control-sector -> target-sector mapping + slab extent.
// Pure data — no GL.
#include "check.h"
#include "mapeditor/model/map_model.h"
#include "mapeditor/model/threed_floors.h"

using namespace elads;

// A map with a target sector (tag 5) that gets a 3D floor from a control sector, plus an
// unrelated sector (tag 9) that must NOT.
static map::MapModel mapWith3DFloor() {
    map::MapModel m;
    // Target sector 0 (a room), tagged 5.
    m.addVertex({0, 0});
    m.addVertex({256, 0});
    m.addVertex({256, 256});
    m.addVertex({0, 256});
    map::Sector target;
    target.floorHeight = 0;
    target.ceilHeight = 256;
    target.tag = 5;
    m.addSector(target); // sector 0
    for (int i = 0; i < 4; ++i) {
        map::Sidedef sd;
        sd.sector = 0;
        const int side = m.addSidedef(sd);
        map::Linedef l;
        l.v1 = i;
        l.v2 = (i + 1) % 4;
        l.front = side;
        m.addLinedef(l);
    }

    // Control sector 1 (a dummy box off to the side), floor 64 / ceil 128, with the 160 line.
    const int cv0 = m.addVertex({500, 0});
    const int cv1 = m.addVertex({564, 0});
    const int cv2 = m.addVertex({564, 64});
    const int cv3 = m.addVertex({500, 64});
    map::Sector control;
    control.floorHeight = 64;
    control.ceilHeight = 128;
    control.floorTex = "CTRLFLR";
    control.ceilTex = "CTRLCEIL";
    m.addSector(control); // sector 1
    const int cverts[4] = {cv0, cv1, cv2, cv3};
    for (int i = 0; i < 4; ++i) {
        map::Sidedef sd;
        sd.sector = 1;
        sd.middle = "SLABSIDE";
        const int side = m.addSidedef(sd);
        map::Linedef l;
        l.v1 = cverts[i];
        l.v2 = cverts[(i + 1) % 4];
        l.front = side;
        if (i == 0) {
            l.special = map::kSpecialSet3DFloor; // 160 on one control-sector line
            l.args = {5, 1, 0, 255, 0};          // arg0 = target tag 5, arg1 = solid, arg3 = alpha
        }
        m.addLinedef(l);
    }

    // An unrelated sector (tag 9), no 3D floor expected.
    m.addVertex({0, 500});
    m.addVertex({64, 500});
    m.addVertex({64, 564});
    map::Sector other;
    other.tag = 9;
    m.addSector(other); // sector 2

    return m;
}

static void run() {
    const map::MapModel m = mapWith3DFloor();
    const auto slabs = map::compute3DFloors(m);

    CHECK_EQ(slabs.size(), static_cast<size_t>(1));
    const map::ThreeDFloor& f = slabs[0];
    CHECK_EQ(f.targetSector, 0);             // the tag-5 room, not the control or tag-9 sector
    CHECK_EQ(f.topZ, 128.0);                 // control ceiling
    CHECK_EQ(f.botZ, 64.0);                  // control floor
    CHECK_EQ(f.texTop, std::string("CTRLCEIL"));
    CHECK_EQ(f.texBot, std::string("CTRLFLR"));
    CHECK_EQ(f.texSide, std::string("SLABSIDE"));
    CHECK_EQ(f.type, 1);
    CHECK_EQ(f.alpha, 255);

    // No 160 lines -> no slabs.
    map::MapModel plain = m;
    for (int i = 0; i < static_cast<int>(plain.linedefCount()); ++i)
        plain.linedef(i).special = 0;
    CHECK(map::compute3DFloors(plain).empty());
}

TEST_MAIN(run())
