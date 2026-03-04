#pragma once

namespace mc {

class Level;

class Entity {
public:
  Entity() = default;
  virtual ~Entity() = default;

  virtual void tick(Level* level, double dtSeconds);

  void setPosition(double x, double y, double z);

  double x() const { return x_; }
  double y() const { return y_; }
  double z() const { return z_; }

protected:
  double x_ = 0.0;
  double y_ = 0.0;
  double z_ = 0.0;
};

}  // namespace mc
