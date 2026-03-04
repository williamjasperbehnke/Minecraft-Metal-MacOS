#include "World/Entity/Player.h"

#include "World/Level/Level.h"

namespace mc {

void LocalPlayer::setMoveIntent(double xDir, double zDir) {
  moveStrafe_ = static_cast<float>(xDir);
  moveForward_ = static_cast<float>(zDir);
}

void LocalPlayer::setYawRadians(float yawRadians) {
  constexpr float kRadToDeg = 180.0f / 3.1415926535f;
  setYawDegrees(yawRadians * kRadToDeg);
}

void LocalPlayer::tick(Level* level, double dtSeconds) {
  setMoveInput(moveStrafe_, moveForward_);
  Mob::tick(level, dtSeconds);
}

}  // namespace mc
