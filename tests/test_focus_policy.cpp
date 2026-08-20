#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "focus_policy.h"

using namespace xiii;

static int a = 0, b = 0;          // distinct dummies standing in for HWNDs
static void* const kFg   = &a;
static void* const kGame = &b;

TEST_CASE("foreground owned by our process is reported truthfully") {
    CHECK(ForegroundToReport(true, kFg, kGame) == kFg);
}

TEST_CASE("foreign foreground is replaced by the game window") {
    CHECK(ForegroundToReport(false, kFg, kGame) == kGame);
}

TEST_CASE("foreign foreground passes through while no game window is known") {
    CHECK(ForegroundToReport(false, kFg, nullptr) == kFg);
}

TEST_CASE("null foreground (session switch) still reports the game window") {
    CHECK(ForegroundToReport(false, nullptr, kGame) == kGame);
}

TEST_CASE("null foreground with no game window stays null") {
    CHECK(ForegroundToReport(false, nullptr, nullptr) == nullptr);
}
