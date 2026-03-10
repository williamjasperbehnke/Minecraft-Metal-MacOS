#pragma once

#include <functional>

#include <simd/simd.h>

namespace mc {

class Camera {
public:
  enum class ViewMode : unsigned char {
    FirstPerson = 0,
    ThirdPersonBack = 1,
    ThirdPersonFront = 2,
  };

  void toggleViewMode();
  bool isThirdPerson() const { return viewMode_ != ViewMode::FirstPerson; }

  simd_float3 firstPersonEye(double playerX, double playerY, double playerZ, bool crouching) const;
  simd_float3 forward(float yawRadians, float pitchRadians) const;
  simd_float3 right(float yawRadians, float pitchRadians) const;
  simd_float3 up(float yawRadians, float pitchRadians) const;
  simd_float3 renderEye(const simd_float3& firstEye, float yawRadians, float pitchRadians,
                        const std::function<bool(int, int, int)>& isSolidAt) const;
  simd_float3 viewTarget(const simd_float3& renderEye, const simd_float3& firstEye, float yawRadians, float pitchRadians) const;

private:
  ViewMode viewMode_ = ViewMode::FirstPerson;
  float thirdPersonDistance_ = 3.4f;

  static constexpr float kEyeHeight = 1.62f;
  static constexpr float kCrouchEyeOffset = 0.25f;
};

}  // namespace mc
