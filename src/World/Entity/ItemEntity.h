#pragma once

#include <random>

#include "World/Entity/Entity.h"
#include "World/Phys/AABB.h"

namespace mc {

class Level;

class ItemEntity : public Entity {
public:
  static constexpr int kMaxStackSize = 64;
  static constexpr int kLifetimeTicks = 5 * 60 * 20;
  static constexpr double kHalfWidth = 0.125;
  static constexpr double kHalfHeight = 0.125;

  ItemEntity(int tile, int count, double x, double y, double z, std::mt19937& rng);

  void tick(Level* level, double dtSeconds) override;
  void tick(Level* level);
  bool tryMerge(ItemEntity& target);
  bool intersectsAabb(double minX, double minY, double minZ, double maxX, double maxY, double maxZ) const;

  int tile() const { return tile_; }
  int count() const { return count_; }
  void setCount(int count);

  float yawRadians() const { return yawRadians_; }
  float yawRadians(float partialTicks) const;
  float bob() const;
  float bob(float partialTicks) const;

  int ageTicks() const { return ageTicks_; }
  int throwTimeTicks() const { return throwTimeTicks_; }
  void setThrowTimeTicks(int ticks);

  void setMotion(double xd, double yd, double zd);
  double motionX() const { return xd_; }
  double motionY() const { return yd_; }
  double motionZ() const { return zd_; }
  bool isAlive() const { return alive_ && tile_ > 0 && count_ > 0; }
  void kill() { alive_ = false; }

private:
  AABB aabbAt(double x, double y, double z) const;
  bool isInWater(Level* level) const;
  bool collidesAt(Level* level, double x, double y, double z) const;
  void moveAxis(Level* level, int axis, double delta, bool* collided);

  int tile_ = 0;
  int count_ = 0;
  int ageTicks_ = 0;
  int throwTimeTicks_ = 0;
  bool alive_ = true;
  bool onGround_ = false;

  double xd_ = 0.0;
  double yd_ = 0.0;
  double zd_ = 0.0;

  float bobOffs_ = 0.0f;
  float yawRadians_ = 0.0f;
};

}  // namespace mc
