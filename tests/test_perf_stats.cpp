#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "perf_stats.h"

using namespace xiii;

TEST_CASE("RateLimiter allows the first call") {
    RateLimiter rl(11);
    CHECK(rl.Allow(1000));
}

TEST_CASE("RateLimiter denies within the interval and allows at it") {
    RateLimiter rl(11);
    REQUIRE(rl.Allow(1000));
    CHECK_FALSE(rl.Allow(1005));
    CHECK_FALSE(rl.Allow(1010));
    CHECK(rl.Allow(1011));
}

TEST_CASE("RateLimiter measures from the last ALLOWED call, not denied ones") {
    RateLimiter rl(10);
    REQUIRE(rl.Allow(100));
    CHECK_FALSE(rl.Allow(109));   // denied; must not move the reference point
    CHECK(rl.Allow(110));
}

TEST_CASE("RateLimiter survives 32-bit tick wraparound") {
    RateLimiter rl(11);
    REQUIRE(rl.Allow(0xFFFFFFF8u));       // 8 ticks before wrap
    CHECK_FALSE(rl.Allow(0xFFFFFFFEu));   // +6
    CHECK(rl.Allow(3));                   // +11 across the wrap
}

TEST_CASE("RateLimiter with interval 0 always allows") {
    RateLimiter rl(0);
    CHECK(rl.Allow(5));
    CHECK(rl.Allow(5));
    CHECK(rl.Allow(6));
}

TEST_CASE("PhaseStats starts empty with zero average") {
    PhaseStats ps;
    CHECK(ps.count == 0);
    CHECK(ps.AvgUs() == 0);
    CHECK(ps.maxUs == 0);
}

TEST_CASE("PhaseStats accumulates count, average, and max") {
    PhaseStats ps;
    ps.Add(10);
    ps.Add(20);
    ps.Add(60);
    CHECK(ps.count == 3);
    CHECK(ps.AvgUs() == 30);
    CHECK(ps.maxUs == 60);
}

TEST_CASE("PhaseStats reset clears everything") {
    PhaseStats ps;
    ps.Add(500);
    ps.Reset();
    CHECK(ps.count == 0);
    CHECK(ps.AvgUs() == 0);
    CHECK(ps.maxUs == 0);
}
