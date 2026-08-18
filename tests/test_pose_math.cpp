// tests/test_pose_math.cpp
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "pose_math.h"
#include <cmath>

TEST_CASE("identity quaternion yields zero yaw/pitch/roll") {
    Quaternion q{0.0f, 0.0f, 0.0f, 1.0f};
    EulerRadians e = QuaternionToEuler(q);
    CHECK(e.yaw == doctest::Approx(0.0f));
    CHECK(e.pitch == doctest::Approx(0.0f));
    CHECK(e.roll == doctest::Approx(0.0f));
}

TEST_CASE("90 degree yaw quaternion (rotation about Y) yields pi/2 yaw") {
    float half = 3.14159265358979323846f / 4.0f;
    Quaternion q{0.0f, std::sin(half), 0.0f, std::cos(half)};
    EulerRadians e = QuaternionToEuler(q);
    CHECK(e.yaw == doctest::Approx(3.14159265358979323846f / 2.0f).epsilon(0.001));
}

TEST_CASE("zero radians converts to zero rotator units") {
    CHECK(RadiansToUnrealRotatorUnits(0.0f) == 0);
}

TEST_CASE("full revolution converts to zero rotator units (wraps)") {
    float twoPi = 2.0f * 3.14159265358979323846f;
    CHECK(RadiansToUnrealRotatorUnits(twoPi) == 0);
}

TEST_CASE("half revolution converts to 32768 rotator units") {
    float pi = 3.14159265358979323846f;
    CHECK(RadiansToUnrealRotatorUnits(pi) == 32768);
}

TEST_CASE("negative angle wraps into positive rotator range") {
    float half = 3.14159265358979323846f / 2.0f;
    int32_t result = RadiansToUnrealRotatorUnits(-half);
    CHECK(result == 49152);  // -90deg == 270deg == 3/4 * 65536
}
