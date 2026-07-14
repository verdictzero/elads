// SPDX-License-Identifier: GPL-3.0-or-later
// Classify a variety of synthetic lumps through the entry-type detection pipeline.
#include "archive/entry_type.h"
#include "check.h"
#include "graphics/doom_gfx.h"
#include "graphics/palette.h"
#include "graphics/texturex.h"

using namespace elads;
using archive::EntryType;

static util::Bytes bytes(std::initializer_list<int> v) {
    util::Bytes b;
    for (int x : v)
        b.push_back(static_cast<uint8_t>(x));
    return b;
}

static void run() {
    // Zero-length -> marker.
    CHECK(archive::detectEntryType("MAP01", {}) == EntryType::Marker);

    // Definitive names.
    CHECK(archive::detectEntryType("PLAYPAL", util::Bytes(768, 0)) == EntryType::Palette);
    CHECK(archive::detectEntryType("COLORMAP", util::Bytes(8704, 0)) == EntryType::Colormap);
    CHECK(archive::detectEntryType("PNAMES", gfx::writePnames({"WALL01"})) == EntryType::Pnames);
    CHECK(archive::detectEntryType("TEXTURE1", util::Bytes{0, 0, 0, 0}) == EntryType::TextureX);
    CHECK(archive::detectEntryType("MAPINFO", util::Bytes{'x', '\n'}) == EntryType::Text);
    CHECK(archive::detectEntryType("ZSCRIPT", util::Bytes{'c', 'l', 'a', 's', 's'}) == EntryType::Text);

    // Magic bytes.
    CHECK(archive::detectEntryType("X", bytes({0x89, 'P', 'N', 'G', 13, 10})) == EntryType::Png);
    CHECK(archive::detectEntryType("D_E1M1", bytes({'M', 'U', 'S', 0x1A, 0, 0})) == EntryType::Mus);
    CHECK(archive::detectEntryType("D_E1M2", bytes({'M', 'T', 'h', 'd', 0, 0})) == EntryType::Midi);
    CHECK(archive::detectEntryType("NESTED", bytes({'P', 'W', 'A', 'D', 0, 0, 0, 0})) == EntryType::Wad);
    CHECK(archive::detectEntryType("ZIPPED", bytes({0x50, 0x4B, 0x03, 0x04, 0, 0})) == EntryType::Zip);

    // DMX digital sound: format id 3, 22050 Hz, 8 samples + 8 sample bytes.
    CHECK(archive::detectEntryType(
              "DSPISTOL",
              bytes({3, 0, 0x22, 0x56, 8, 0, 0, 0, 128, 128, 128, 128, 128, 128, 128, 128})) ==
          EntryType::DmxSound);

    // Flat by size.
    CHECK(archive::detectEntryType("FLOOR0_1", util::Bytes(4096, 5)) == EntryType::Flat);

    // Doom picture (small, so its size is not a flat size).
    const gfx::Palette pal = gfx::Palette::testRamp();
    gfx::Image g(3, 3);
    g.set(0, 0, pal.color(1).r, pal.color(1).g, pal.color(1).b, 255);
    g.set(2, 2, pal.color(2).r, pal.color(2).g, pal.color(2).b, 255);
    CHECK(archive::detectEntryType("SOMEGFX", gfx::encodeDoomGfx(g, pal)) == EntryType::DoomGfx);

    // Printable text with an unknown name.
    CHECK(archive::detectEntryType("STUFF", util::Bytes{'h', 'e', 'l', 'l', 'o', '\n'}) == EntryType::Text);

    // Genuine binary junk -> unknown.
    CHECK(archive::detectEntryType("JUNK", bytes({0xDE, 0xAD, 0xBE, 0xEF, 0x01})) == EntryType::Unknown);

    // Name round-trips to a readable string.
    CHECK(std::string(archive::entryTypeName(EntryType::Flat)) == "flat");
}

TEST_MAIN(run())
