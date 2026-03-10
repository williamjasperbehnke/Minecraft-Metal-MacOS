#include "Client/Core/Camera.h"

#include <algorithm>
#include <cmath>

#include "Common/Math/Vec3.h"

namespace mc {

using math::vec3::add;
using math::vec3::cross;
using math::vec3::mul;
using math::vec3::normalize;

void Camera::toggleViewMode() {
  switch (viewMode_) {
    case ViewMode::FirstPerson: viewMode_ = ViewMode::ThirdPersonBack; break;
    case ViewMode::ThirdPersonBack: viewMode_ = ViewMode::ThirdPersonFront; break;
    case ViewMode::ThirdPersonFront: viewMode_ = ViewMode::FirstPerson; break;
  }
}

simd_float3 Camera::firstPersonEye(double playerX, double playerY, double playerZ, bool crouching) const {
  const float eyeHeight = kEyeHeight - (crouching ? kCrouchEyeOffset : 0.0f);
  return {static_cast<float>(playerX), static_cast<float>(playerY) + eyeHeight, static_cast<float>(playerZ)};
}

simd_float3 Camera::forward(float yawRadians, float pitchRadians) const {
  const float cp = std::cos(pitchRadians);
  const simd_float3 f{std::sin(yawRadians) * cp, std::sin(pitchRadians), std::cos(yawRadians) * cp};
  return normalize(f);
}

simd_float3 Camera::right(float yawRadians, float pitchRadians) const {
  const simd_float3 upDir{0.0f, 1.0f, 0.0f};
  return normalize(cross(upDir, forward(yawRadians, pitchRadians)));
}

simd_float3 Camera::up(float yawRadians, float pitchRadians) const {
  return normalize(cross(right(yawRadians, pitchRadians), forward(yawRadians, pitchRadians)));
}

simd_float3 Camera::renderEye(const simd_float3& firstEye, float yawRadians, float pitchRadians,
                              const std::function<bool(int, int, int)>& isSolidAt) const {
  if (viewMode_ == ViewMode::FirstPerson) {
    return firstEye;
  }

  const float dir = (viewMode_ == ViewMode::ThirdPersonFront) ? 1.0f : -1.0f;
  const simd_float3 offsetDir = mul(forward(yawRadians, pitchRadians), dir);
  constexpr int kSteps = 24;
  float distance = thirdPersonDistance_;

  for (int step = 1; step <= kSteps; ++step) {
    const float d = thirdPersonDistance_ * static_cast<float>(step) / static_cast<float>(kSteps);
    const simd_float3 p = add(firstEye, mul(offsetDir, d));
    const int tx = static_cast<int>(std::floor(p.x));
    const int ty = static_cast<int>(std::floor(p.y));
    const int tz = static_cast<int>(std::floor(p.z));
    if (isSolidAt(tx, ty, tz)) {
      distance = thirdPersonDistance_ * static_cast<float>(std::max(0, step - 1)) / static_cast<float>(kSteps);
      break;
    }
  }

  return add(firstEye, mul(offsetDir, distance));
}

simd_float3 Camera::viewTarget(const simd_float3& renderEye, const simd_float3& firstEye, float yawRadians, float pitchRadians) const {
  if (viewMode_ == ViewMode::ThirdPersonFront) {
    return firstEye;
  }
  return add(renderEye, forward(yawRadians, pitchRadians));
}

}  // namespace mc
