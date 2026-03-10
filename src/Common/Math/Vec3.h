#pragma once

#include <cmath>

#include <simd/simd.h>

namespace mc::math::vec3 {

inline simd_float3 add(const simd_float3& a, const simd_float3& b) {
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}

inline simd_float3 sub(const simd_float3& a, const simd_float3& b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline simd_float3 mul(const simd_float3& v, float s) {
  return {v.x * s, v.y * s, v.z * s};
}

inline float dot(const simd_float3& a, const simd_float3& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline simd_float3 cross(const simd_float3& a, const simd_float3& b) {
  return {
      a.y * b.z - a.z * b.y,
      a.z * b.x - a.x * b.z,
      a.x * b.y - a.y * b.x,
  };
}

inline simd_float3 normalize(const simd_float3& v) {
  const float len = std::sqrt(dot(v, v));
  if (len < 1e-6f) {
    return {0.0f, 0.0f, 0.0f};
  }
  return {v.x / len, v.y / len, v.z / len};
}

}  // namespace mc::math::vec3
