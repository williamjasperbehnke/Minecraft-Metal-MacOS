#pragma once

#include "World/Phys/AABB.h"

namespace mc {

class Level;

class Entity {
public:
  Entity() = default;
  virtual ~Entity() = default;

  virtual void tick(Level* level, double dtSeconds);

  void setPosition(double x, double y, double z);
  void setSize(double width, double height);
  AABB aabb() const;
  AABB aabbAt(double x, double y, double z) const;

  double x() const { return x_; }
  double y() const { return y_; }
  double z() const { return z_; }
  double hitboxWidth() const { return hitboxWidth_; }
  double hitboxHeight() const { return hitboxHeight_; }

protected:
  double x_ = 0.0;
  double y_ = 0.0;
  double z_ = 0.0;
  double hitboxWidth_ = 0.6;
  double hitboxHeight_ = 1.8;
};

}  // namespace mc
