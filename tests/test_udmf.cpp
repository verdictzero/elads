// SPDX-License-Identifier: GPL-3.0-or-later
// Parse UDMF text, verify typed fields + unknown-key preservation, and prove a
// parse -> write -> parse round-trip is lossless.
#include "check.h"
#include "mapeditor/model/udmf.h"

using namespace elads;

static const char* kSample = R"UDMF(
namespace = "zdoom";

// a small square room
vertex { x = 0.0; y = 0.0; }
vertex { x = 64.0; y = 0.0; }
vertex { x = 64.0; y = 64.0; }
vertex { x = 0.0; y = 64.0; }

sidedef { sector = 0; texturemiddle = "STARTAN2"; }
sidedef { sector = 0; texturemiddle = "STARTAN2"; }
sidedef { sector = 0; texturemiddle = "STARTAN2"; }
sidedef { sector = 0; texturemiddle = "STARTAN2"; }

linedef { v1 = 0; v2 = 1; sidefront = 0; blocking = true; }
linedef { v1 = 1; v2 = 2; sidefront = 1; blocking = true; }
linedef { v1 = 2; v2 = 3; sidefront = 2; blocking = true; }
linedef { v1 = 3; v2 = 0; sidefront = 3; blocking = true; special = 11; arg0 = 3; }

sector { heightfloor = 0; heightceiling = 128; texturefloor = "FLOOR4_8"; textureceiling = "CEIL3_5"; lightlevel = 176; }

thing { x = 32.0; y = 32.0; angle = 90; type = 1; skill1 = true; single = true; }
)UDMF";

static const map::Linedef* findLineWithExtra(const map::MapModel& m, const std::string& key) {
    for (const auto& l : m.linedefs())
        for (const auto& kv : l.extra)
            if (kv.first == key)
                return &l;
    return nullptr;
}

static void checkModel(const map::UdmfMap& um) {
    CHECK_EQ(um.namespaceId, std::string("zdoom"));
    const map::MapModel& m = um.model;
    CHECK_EQ(m.vertexCount(), static_cast<size_t>(4));
    CHECK_EQ(m.sidedefCount(), static_cast<size_t>(4));
    CHECK_EQ(m.linedefCount(), static_cast<size_t>(4));
    CHECK_EQ(m.sectorCount(), static_cast<size_t>(1));
    CHECK_EQ(m.thingCount(), static_cast<size_t>(1));

    // Typed fields.
    CHECK(m.vertex(1).pos == (util::Vec2{64, 0}));
    CHECK_EQ(m.sidedef(0).middle, std::string("STARTAN2"));
    CHECK_EQ(m.sector(0).ceilHeight, 128);
    CHECK_EQ(m.sector(0).floorTex, std::string("FLOOR4_8"));
    CHECK_EQ(m.sector(0).lightLevel, 176);
    CHECK_EQ(m.linedef(3).special, 11);
    CHECK_EQ(m.linedef(3).args[0], 3);
    CHECK_EQ(m.thing(0).type, 1);
    CHECK_EQ(m.thing(0).angle, 90);

    // Unknown keys preserved verbatim in `extra` (lossless round-trip requirement).
    CHECK(findLineWithExtra(m, "blocking") != nullptr);
    bool thingHasSkill = false;
    for (const auto& kv : m.thing(0).extra)
        if (kv.first == "skill1")
            thingHasSkill = true;
    CHECK(thingHasSkill);
}

static void run() {
    const map::UdmfMap um = map::parseUdmf(kSample);
    checkModel(um);

    // parse -> write -> parse must reproduce the same model (incl. preserved unknown keys).
    const std::string text2 = map::writeUdmf(um);
    const map::UdmfMap um2 = map::parseUdmf(text2);
    checkModel(um2);

    // The unknown 'blocking' flag really made it through the writer.
    CHECK(text2.find("blocking = true;") != std::string::npos);
    CHECK(text2.find("namespace = \"zdoom\";") != std::string::npos);
}

TEST_MAIN(run())
