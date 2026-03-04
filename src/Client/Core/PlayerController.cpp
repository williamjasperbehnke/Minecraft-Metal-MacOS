#include "Client/Core/PlayerController.h"

#include <cmath>

#include "World/Entity/Player.h"

namespace mc {

namespace {

constexpr float kBaseFovRadians = 70.0f * 3.1415926535f / 180.0f;
constexpr float kSprintFovRadians = 80.0f * 3.1415926535f / 180.0f;

}  // namespace

void PlayerController::setInputState(const InputState& input) {
  input_ = input;
}

void PlayerController::addLookInput(float deltaX, float deltaY) {
  const float sensitivity = 0.0025f;
  yawRadians_ -= deltaX * sensitivity;
  pitchRadians_ -= deltaY * sensitivity;

  const float kPitchLimit = 1.45f;
  if (pitchRadians_ > kPitchLimit) {
    pitchRadians_ = kPitchLimit;
  }
  if (pitchRadians_ < -kPitchLimit) {
    pitchRadians_ = -kPitchLimit;
  }
}

void PlayerController::applyToPlayer(LocalPlayer& player, GameModeType mode) {
  float strafe = 0.0f;
  float forward = 0.0f;
  if (input_.moveLeft) {
    strafe += 1.0f;
  }
  if (input_.moveRight) {
    strafe -= 1.0f;
  }
  if (input_.moveForward) {
    forward += 1.0f;
  }
  if (input_.moveBackward) {
    forward -= 1.0f;
  }

  sprinting_ = input_.sprint && input_.moveForward && !input_.moveBackward && !input_.crouch;
  crouching_ = (mode == GameModeType::Survival) && input_.crouch;

  if (mode == GameModeType::Spectator) {
    player.setNoClip(true);
    player.setFlying(true);
    player.setCrouching(false);
    player.setFlyVerticalInput((input_.jump ? 1.0f : 0.0f) + (input_.crouch ? -1.0f : 0.0f));
    player.setMoveSpeed(input_.sprint ? 0.28f : 0.17f);
  } else if (mode == GameModeType::Creative) {
    player.setNoClip(false);
    player.setFlying(true);
    player.setCrouching(false);
    player.setFlyVerticalInput((input_.jump ? 1.0f : 0.0f) + (input_.crouch ? -1.0f : 0.0f));
    player.setMoveSpeed(input_.sprint ? 0.22f : 0.12f);
  } else {
    player.setNoClip(false);
    player.setFlying(false);
    player.setFlyVerticalInput(0.0f);
    float moveSpeed = 0.1f;
    if (sprinting_) {
      moveSpeed = 0.135f;
    } else if (input_.crouch) {
      moveSpeed = 0.045f;
    }
    player.setMoveSpeed(moveSpeed);
    player.setCrouching(input_.crouch);
  }

  player.setYawRadians(yawRadians_);
  player.setMoveIntent(strafe, forward);
  player.setJumping(input_.jump);
}

float PlayerController::targetFovRadians(GameModeType mode) const {
  const bool boostedFov = (mode == GameModeType::Survival) ? sprinting_ : input_.sprint;
  return boostedFov ? kSprintFovRadians : kBaseFovRadians;
}

}  // namespace mc
