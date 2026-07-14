// SPDX-License-Identifier: GPL-3.0-or-later
// Round-trip a PK3 (zip) archive and check folder-namespace mapping.
#include "archive/pk3.h"
#include "check.h"

using namespace elads;

static util::Bytes str(const char* s) {
    return util::Bytes(reinterpret_cast<const uint8_t*>(s),
                       reinterpret_cast<const uint8_t*>(s) + std::char_traits<char>::length(s));
}

static void roundTrip(bool compress) {
    archive::Pk3 pk3;
    pk3.add("MAPINFO", str("map MAP01 \"Test\"\n"));
    pk3.add("maps/MAP01.txt", str("hello elads"));
    pk3.add("textures/BRICK.dat", util::Bytes{1, 2, 3, 4, 5, 6, 7, 8});

    const util::Bytes zip = pk3.write(compress);
    CHECK(zip.size() > 0);

    const archive::Pk3 back = archive::Pk3::read(zip);
    CHECK_EQ(back.entryCount(), static_cast<size_t>(3));

    const archive::Pk3Entry* map = back.find("maps/MAP01.txt");
    CHECK(map != nullptr);
    if (map)
        CHECK(map->data == str("hello elads"));

    const archive::Pk3Entry* tex = back.find("textures/BRICK.dat");
    CHECK(tex != nullptr);
    if (tex)
        CHECK(tex->data == (util::Bytes{1, 2, 3, 4, 5, 6, 7, 8}));

    CHECK(back.find("MAPINFO") != nullptr);
}

static void run() {
    roundTrip(true);   // deflate
    roundTrip(false);  // stored

    // Folder -> namespace mapping.
    CHECK_EQ(archive::pk3Namespace("textures/BRICK.png"), std::string("textures"));
    CHECK_EQ(archive::pk3Namespace("flats/FLOOR.png"), std::string("flats"));
    CHECK_EQ(archive::pk3Namespace("maps/MAP01.wad"), std::string("maps"));
    CHECK_EQ(archive::pk3Namespace("MAPINFO"), std::string("global"));
    CHECK_EQ(archive::pk3Namespace("acs/script.o"), std::string("acs"));
    CHECK_EQ(archive::pk3Namespace("weird/thing.dat"), std::string("global"));

    // Invalid zip is rejected.
    bool threw = false;
    try {
        archive::Pk3::read(util::Bytes{'n', 'o', 't', 'z', 'i', 'p'});
    } catch (const std::exception&) {
        threw = true;
    }
    CHECK(threw);
}

TEST_MAIN(run())
