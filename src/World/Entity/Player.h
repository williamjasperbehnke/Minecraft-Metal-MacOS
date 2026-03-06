#pragma once

#include "Client/Inventory/Inventory.h"
#include "World/Entity/Mob.h"

namespace mc {

class Player : public Mob {
public:
  virtual ~Player() = default;

  double blockReach() const { return blockReach_; }
  Inventory& inventory() { return inventory_; }
  const Inventory& inventory() const { return inventory_; }

private:
  double blockReach_ = 8.0;
  Inventory inventory_{};
};

class LocalPlayer : public Player {
public:
  void setMoveIntent(double xDir, double zDir);
  void setYawRadians(float yawRadians);
  void tick(Level* level, double dtSeconds) override;

private:
  float moveStrafe_ = 0.0f;
  float moveForward_ = 0.0f;
};

}  // namespace mc
