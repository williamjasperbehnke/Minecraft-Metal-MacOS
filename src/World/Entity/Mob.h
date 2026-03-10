#pragma once

#include "World/Entity/Entity.h"

namespace mc {

class Mob : public Entity {
public:
  Mob();
  virtual ~Mob() = default;

  void tick(Level* level, double dtSeconds) override;

  void setMoveInput(float xxa, float yya);
  void setJumping(bool jumping) { jumping_ = jumping; }
  void setCrouching(bool crouching) { crouching_ = crouching; }
  void setFlying(bool flying) { flying_ = flying; }
  bool isFlying() const { return flying_; }
  void setNoClip(bool noClip) { noClip_ = noClip; }
  bool noClip() const { return noClip_; }
  void setFlyVerticalInput(float vertical) { flyVertical_ = vertical; }
  void setYawDegrees(float yawDegrees);
  float yawDegrees() const { return yawDegrees_; }
  void setMoveSpeed(float speed) { walkingSpeed_ = speed; }
  float moveSpeed() const { return walkingSpeed_; }
  bool onGround() const { return onGround_; }

private:
  void travel(Level* level, float xa, float ya, double dtSeconds);
  void moveRelative(float xa, float za, float speedPerTick);
  bool collidesAt(Level* level, double x, double y, double z) const;
  bool isInWater(Level* level) const;
  void clipCrouchEdgeMotion(Level* level, double* dx, double* dz) const;
  double moveAxisWithCollision(Level* level, double delta, int axis, bool* collided) const;
  float groundFriction(Level* level) const;

private:
  float xxa_ = 0.0f;
  float yya_ = 0.0f;
  float yawDegrees_ = 0.0f;
  bool jumping_ = false;
  bool crouching_ = false;
  bool flying_ = false;
  bool noClip_ = false;
  bool onGround_ = false;
  int jumpGraceTicks_ = 0;
  float flyVertical_ = 0.0f;

  double xd_ = 0.0;
  double yd_ = 0.0;
  double zd_ = 0.0;

  float walkingSpeed_ = 0.1f;
  float flyingSpeed_ = 0.02f;
  float stepHeight_ = 0.6f;
};

}  // namespace mc
