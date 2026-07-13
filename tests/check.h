// SPDX-License-Identifier: GPL-3.0-or-later
// elads — minimal test-check harness (no external framework).
// Each test executable defines its own main() that runs checks and returns
// eladstest::failures() ? 1 : 0, so CTest reports pass/fail per binary.
#pragma once

#include <cstdio>

namespace eladstest {
inline int g_failures = 0;
inline int failures() { return g_failures; }
} // namespace eladstest

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                           \
            std::fprintf(stderr, "CHECK FAILED: %s  (%s:%d)\n", #cond,           \
                         __FILE__, __LINE__);                                    \
            ++eladstest::g_failures;                                             \
        }                                                                        \
    } while (0)

#define CHECK_EQ(a, b)                                                           \
    do {                                                                         \
        if (!((a) == (b))) {                                                     \
            std::fprintf(stderr, "CHECK_EQ FAILED: %s == %s  (%s:%d)\n", #a, #b, \
                         __FILE__, __LINE__);                                    \
            ++eladstest::g_failures;                                             \
        }                                                                        \
    } while (0)

#define TEST_MAIN(body)                                                          \
    int main() {                                                                 \
        body;                                                                    \
        if (eladstest::failures() == 0)                                          \
            std::printf("OK (%s)\n", __FILE__);                                  \
        return eladstest::failures() ? 1 : 0;                                    \
    }
