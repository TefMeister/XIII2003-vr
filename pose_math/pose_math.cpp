// pose_math/pose_math.cpp
#include "pose_math.h"
#include <cmath>

EulerRadians QuaternionToEuler(Quaternion q) {
    EulerRadians e{};

    float siny_cosp = 2.0f * (q.w * q.y + q.z * q.x);
    float cosy_cosp = 1.0f - 2.0f * (q.y * q.y + q.x * q.x);
    e.yaw = std::atan2(siny_cosp, cosy_cosp);

    float sinp = 2.0f * (q.w * q.x - q.z * q.y);
    if (std::fabs(sinp) >= 1.0f)
        e.pitch = std::copysign(3.14159265358979323846f / 2.0f, sinp);
    else
        e.pitch = std::asin(sinp);

    float sinr_cosp = 2.0f * (q.w * q.z + q.x * q.y);
    float cosr_cosp = 1.0f - 2.0f * (q.x * q.x + q.z * q.z);
    e.roll = std::atan2(sinr_cosp, cosr_cosp);

    return e;
}

int32_t RadiansToUnrealRotatorUnits(float radians) {
    constexpr float kUnitsPerRevolution = 65536.0f;
    constexpr float kTwoPi = 2.0f * 3.14159265358979323846f;
    float normalized = std::fmod(radians, kTwoPi);
    if (normalized < 0.0f) normalized += kTwoPi;
    return static_cast<int32_t>((normalized / kTwoPi) * kUnitsPerRevolution);
}
