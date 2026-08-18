// pose_math/pose_math.h
#pragma once
#include <cstdint>

struct Quaternion { float x, y, z, w; };
struct EulerRadians { float yaw; float pitch; float roll; };

EulerRadians QuaternionToEuler(Quaternion q);

// 65536 units = 360 degrees, per classic Unreal Engine 1/2 FRotator convention.
int32_t RadiansToUnrealRotatorUnits(float radians);
