// SPDX-License-Identifier: GPL-3.0-or-later
// Round-trip a WAD in memory and verify the reader/writer agree.
#include "archive/wad.h"
#include "check.h"

using namespace elads;

static void run() {
    archive::Wad wad;
    wad.setType(archive::WadType::Pwad);
    wad.add("MAP01");                                   // zero-length marker
    wad.add("THINGS", util::Bytes{1, 2, 3, 4, 5});
    wad.add("LINEDEFS", util::Bytes{10, 20, 30});
    wad.add("SECTORS", util::Bytes{99});
    wad.add("S_START");                                 // another marker

    const util::Bytes bytes = wad.write();

    // Header sanity: "PWAD", numlumps == 5.
    CHECK(bytes.size() >= 12);
    CHECK(bytes[0] == 'P' && bytes[1] == 'W' && bytes[2] == 'A' && bytes[3] == 'D');

    const archive::Wad back = archive::Wad::read(bytes);
    CHECK(back.type() == archive::WadType::Pwad);
    CHECK_EQ(back.lumpCount(), static_cast<size_t>(5));

    CHECK_EQ(back.lumps()[0].name, std::string("MAP01"));
    CHECK(back.lumps()[0].data.empty());
    CHECK_EQ(back.lumps()[1].name, std::string("THINGS"));
    CHECK(back.lumps()[1].data == (util::Bytes{1, 2, 3, 4, 5}));
    CHECK(back.lumps()[3].data == (util::Bytes{99}));
    CHECK(back.lumps()[4].data.empty());

    // Case-insensitive lookup.
    CHECK_EQ(back.indexOf("things"), 1);
    CHECK(back.find("LineDefs") != nullptr);
    CHECK_EQ(back.indexOf("NOPE"), -1);

    // write() must be deterministic (re-serializing the parsed WAD yields identical bytes).
    CHECK(back.write() == bytes);
}

TEST_MAIN(run())
