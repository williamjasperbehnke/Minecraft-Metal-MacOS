#pragma once

#include "Client/Core/InputState.h"
#include "Client/GameMode/GameMode.h"

namespace mc {

class LocalPlayer;

class PlayerController {
public:
  PlayerController() = default;

  void setInputState(const InputState& input);
  void addLookInput(float deltaX, float deltaY);
  void applyToPlayer(LocalPlayer& player, GameModeType mode);

  float yawRadians() const { return yawRadians_; }
  float pitchRadians() const { return pitchRadians_; }
  bool isSprinting() const { return sprinting_; }
  bool isCrouching() const { return crouching_; }
  float targetFovRadians(GameModeType mode) const;

private:
  InputState input_{};
  float yawRadians_ = 0.0f;
  float pitchRadians_ = -0.35f;
  bool sprinting_ = false;
  bool crouching_ = false;
};

}  // namespace mc
