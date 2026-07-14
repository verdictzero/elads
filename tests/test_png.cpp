// SPDX-License-Identifier: GPL-3.0-or-later
// Verify PNG encoding produces a valid PNG stream.
#include "check.h"
#include "graphics/png.h"

using namespace elads;

static void run() {
    gfx::Image img(8, 8);
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x)
            img.set(x, y, 255, 0, 0, 255); // solid red

    const util::Bytes png = gfx::encodePng(img);
    CHECK(png.size() > 8);
    // PNG signature: 137 80 78 71 13 10 26 10
    CHECK(png[0] == 137 && png[1] == 'P' && png[2] == 'N' && png[3] == 'G');
    CHECK(png[4] == 13 && png[5] == 10 && png[6] == 26 && png[7] == 10);
}

TEST_MAIN(run())
