#include "World/Entity/Mob.h"

#include <algorithm>
#include <cmath>

#include "World/Level/Level.h"
#include "World/Tile/Tile.h"

namespace mc {

namespace {

constexpr float kPi = 3.1415926535f;
constexpr float kTicksPerSecond = 20.0f;
constexpr float kGravityPerTick = 0.08f;
constexpr float kDragPerTick = 0.98f;
constexpr float kGroundFrictionBase = 0.6f * 0.91f;
constexpr float kAirFriction = 0.91f;
constexpr float kJumpVelocity = 0.48f;
constexpr float kWaterSwimSpeed = 0.03f;
constexpr float kWaterJumpImpulse = 0.04f;
constexpr float kWaterSink = 0.02f;
constexpr float kWaterDrag = 0.80f;
constexpr float kFlyAccelPerTick = 0.08f;
constexpr float kFlyVerticalDrag = 0.85f;
constexpr float kFlyMoveDrag = 0.91f;

}  // namespace

void Mob::setMoveInput(float xxa, float yya) {
  xxa_ = xxa;
  yya_ = yya;
}

void Mob::setYawDegrees(float yawDegrees) {
  yawDegrees_ = yawDegrees;
}

void Mob::tick(Level* level, double dtSeconds) {
  xxa_ *= 0.98f;
  yya_ *= 0.98f;

  if (flying_) {
    jumpGraceTicks_ = 0;
    onGround_ = false;
    travel(level, xxa_, yya_, dtSeconds);
    return;
  }

  const bool inWater = isInWater(level);

  if (onGround_) {
    jumpGraceTicks_ = 2;
  } else if (jumpGraceTicks_ > 0) {
    --jumpGraceTicks_;
  }

  if (!inWater && jumping_ && (onGround_ || jumpGraceTicks_ > 0)) {
    yd_ = kJumpVelocity;
    onGround_ = false;
    jumpGraceTicks_ = 0;
  }

  travel(level, xxa_, yya_, dtSeconds);
}

void Mob::travel(Level* level, float xa, float ya, double dtSeconds) {
  const float tickScale = static_cast<float>(dtSeconds * kTicksPerSecond);
  if (tickScale <= 0.0f) {
    return;
  }

  if (noClip_) {
    const float speed = walkingSpeed_;
    moveRelative(xa, ya, speed);
    yd_ += static_cast<double>(flyVertical_) * static_cast<double>(kFlyAccelPerTick * tickScale);

    x_ += xd_ * tickScale;
    y_ += yd_ * tickScale;
    z_ += zd_ * tickScale;
    onGround_ = false;

    const float moveDrag = std::pow(kFlyMoveDrag, tickScale);
    xd_ *= moveDrag;
    zd_ *= moveDrag;
    yd_ *= std::pow(kFlyVerticalDrag, tickScale);
    return;
  }

  if (flying_) {
    const float speed = walkingSpeed_;
    moveRelative(xa, ya, speed);
    yd_ += static_cast<double>(flyVertical_) * static_cast<double>(kFlyAccelPerTick * tickScale);

    bool collidedY = false;
    bool collidedX = false;
    bool collidedZ = false;

    const double dx = xd_ * tickScale;
    const double dy = yd_ * tickScale;
    const double dz = zd_ * tickScale;

    const double movedY = moveAxisWithCollision(level, dy, 1, &collidedY);
    y_ += movedY;
    if (collidedY) {
      yd_ = 0.0;
      onGround_ = dy < 0.0;
    } else {
      onGround_ = false;
    }

    const double movedX = moveAxisWithCollision(level, dx, 0, &collidedX);
    x_ += movedX;
    if (collidedX) {
      xd_ = 0.0;
    }

    const double movedZ = moveAxisWithCollision(level, dz, 2, &collidedZ);
    z_ += movedZ;
    if (collidedZ) {
      zd_ = 0.0;
    }

    const float moveDrag = std::pow(kFlyMoveDrag, tickScale);
    xd_ *= moveDrag;
    zd_ *= moveDrag;
    yd_ *= std::pow(kFlyVerticalDrag, tickScale);
    return;
  }

  const bool inWater = isInWater(level);
  if (inWater) {
    moveRelative(xa, ya, kWaterSwimSpeed);
    if (jumping_) {
      yd_ += kWaterJumpImpulse * tickScale;
    } else {
      yd_ -= kWaterSink * tickScale;
    }

    bool collidedY = false;
    bool collidedX = false;
    bool collidedZ = false;

    const double dx = xd_ * tickScale;
    const double dy = yd_ * tickScale;
    const double dz = zd_ * tickScale;

    const double movedY = moveAxisWithCollision(level, dy, 1, &collidedY);
    y_ += movedY;
    if (collidedY) {
      yd_ = 0.0;
      onGround_ = dy < 0.0;
    } else {
      onGround_ = false;
    }

    const double movedX = moveAxisWithCollision(level, dx, 0, &collidedX);
    x_ += movedX;
    if (collidedX) {
      xd_ = 0.0;
    }

    const double movedZ = moveAxisWithCollision(level, dz, 2, &collidedZ);
    z_ += movedZ;
    if (collidedZ) {
      zd_ = 0.0;
    }

    const float drag = std::pow(kWaterDrag, tickScale);
    xd_ *= drag;
    yd_ *= drag;
    zd_ *= drag;
    return;
  }

  const float friction = onGround_ ? groundFriction(level) : kAirFriction;
  const float friction2 = (kGroundFrictionBase * kGroundFrictionBase * kGroundFrictionBase) /
                          (friction * friction * friction);
  const float speed = onGround_ ? (walkingSpeed_ * friction2) : flyingSpeed_;

  // PS3 flow: travel(xxa, yya) -> moveRelative(...) -> move(...) with friction.
  moveRelative(xa, ya, speed);

  yd_ -= kGravityPerTick * tickScale;

  bool collidedY = false;
  bool collidedX = false;
  bool collidedZ = false;

  const double dx = xd_ * tickScale;
  const double dy = yd_ * tickScale;
  const double dz = zd_ * tickScale;
  double clippedDx = dx;
  double clippedDz = dz;

  if (crouching_ && onGround_) {
    clipCrouchEdgeMotion(level, &clippedDx, &clippedDz);
  }

  const double movedY = moveAxisWithCollision(level, dy, 1, &collidedY);
  y_ += movedY;
  if (collidedY) {
    onGround_ = dy < 0.0;
    yd_ = 0.0;
  } else if (dy < 0.0) {
    onGround_ = false;
  }

  const double preHorizX = x_;
  const double preHorizY = y_;
  const double preHorizZ = z_;

  const double movedX = moveAxisWithCollision(level, clippedDx, 0, &collidedX);
  x_ += movedX;
  if (collidedX) {
    xd_ = 0.0;
  }

  const double movedZ = moveAxisWithCollision(level, clippedDz, 2, &collidedZ);
  z_ += movedZ;
  if (collidedZ) {
    zd_ = 0.0;
  }

  // PS3-like step assist: if horizontal motion was blocked on ground, try stepping up.
  if ((collidedX || collidedZ) && onGround_) {
    const double flatDistSq = movedX * movedX + movedZ * movedZ;
    const double stepY = preHorizY + static_cast<double>(stepHeight_);

    if (!collidesAt(level, preHorizX, stepY, preHorizZ)) {
      x_ = preHorizX;
      y_ = stepY;
      z_ = preHorizZ;

      bool stepCollidedX = false;
      bool stepCollidedZ = false;
      const double stepMovedX = moveAxisWithCollision(level, clippedDx, 0, &stepCollidedX);
      x_ += stepMovedX;
      const double stepMovedZ = moveAxisWithCollision(level, clippedDz, 2, &stepCollidedZ);
      z_ += stepMovedZ;

      const double stepDistSq = stepMovedX * stepMovedX + stepMovedZ * stepMovedZ;
      if (stepDistSq + 1e-6 < flatDistSq) {
        x_ = preHorizX + movedX;
        y_ = preHorizY;
        z_ = preHorizZ + movedZ;
      } else {
        bool downCollided = false;
        const double down = moveAxisWithCollision(level, -static_cast<double>(stepHeight_), 1, &downCollided);
        y_ += down;
        onGround_ = downCollided;
      }
    }
  }

  const float motionFriction = std::pow(onGround_ ? groundFriction(level) : kAirFriction, tickScale);
  xd_ *= motionFriction;
  zd_ *= motionFriction;
  yd_ *= std::pow(kDragPerTick, tickScale);

  // Keep grounded state stable when vertical delta is tiny but feet are on a block.
  if (!onGround_ && collidesAt(level, x_, y_ - 0.05, z_)) {
    onGround_ = true;
    if (yd_ < 0.0) {
      yd_ = 0.0;
    }
  }
}

void Mob::moveRelative(float xa, float za, float speedPerTick) {
  float dist = xa * xa + za * za;
  if (dist < 0.01f * 0.01f) {
    return;
  }

  dist = std::sqrt(dist);
  if (dist < 1.0f) {
    dist = 1.0f;
  }
  const float scaled = speedPerTick / dist;
  xa *= scaled;
  za *= scaled;

  const float rad = yawDegrees_ * (kPi / 180.0f);
  const float s = std::sin(rad);
  const float c = std::cos(rad);

  xd_ += xa * c + za * s;
  zd_ += za * c - xa * s;
}

bool Mob::collidesAt(Level* level, double x, double y, double z) const {
  if (!level) {
    return false;
  }

  const double half = static_cast<double>(width_) * 0.5;
  const double minX = x - half;
  const double maxX = x + half;
  const double minY = y;
  const double maxY = y + static_cast<double>(height_);
  const double minZ = z - half;
  const double maxZ = z + half;

  const int x0 = static_cast<int>(std::floor(minX));
  const int x1 = static_cast<int>(std::floor(maxX - 1e-6));
  const int y0 = static_cast<int>(std::floor(minY));
  const int y1 = static_cast<int>(std::floor(maxY - 1e-6));
  const int z0 = static_cast<int>(std::floor(minZ));
  const int z1 = static_cast<int>(std::floor(maxZ - 1e-6));

  for (int by = y0; by <= y1; ++by) {
    for (int bz = z0; bz <= z1; ++bz) {
      for (int bx = x0; bx <= x1; ++bx) {
        if (isSolidTileId(level->getTile(bx, by, bz))) {
          return true;
        }
      }
    }
  }

  return false;
}

bool Mob::isInWater(Level* level) const {
  if (!level) {
    return false;
  }

  const double half = static_cast<double>(width_) * 0.5;
  const double minX = x_ - half;
  const double maxX = x_ + half;
  const double minY = y_ + 0.05;
  const double maxY = y_ + static_cast<double>(height_) * 0.9;
  const double minZ = z_ - half;
  const double maxZ = z_ + half;

  const int x0 = static_cast<int>(std::floor(minX));
  const int x1 = static_cast<int>(std::floor(maxX - 1e-6));
  const int y0 = static_cast<int>(std::floor(minY));
  const int y1 = static_cast<int>(std::floor(maxY - 1e-6));
  const int z0 = static_cast<int>(std::floor(minZ));
  const int z1 = static_cast<int>(std::floor(maxZ - 1e-6));

  for (int by = y0; by <= y1; ++by) {
    for (int bz = z0; bz <= z1; ++bz) {
      for (int bx = x0; bx <= x1; ++bx) {
        if (level->getTile(bx, by, bz) == static_cast<int>(TileId::Water)) {
          return true;
        }
      }
    }
  }
  return false;
}

void Mob::clipCrouchEdgeMotion(Level* level, double* dx, double* dz) const {
  if (!level || !dx || !dz) {
    return;
  }

  constexpr double kStep = 0.05;
  double mx = *dx;
  double mz = *dz;

  while (std::abs(mx) > 1e-6 && !collidesAt(level, x_ + mx, y_ - 0.2, z_)) {
    if (std::abs(mx) <= kStep) {
      mx = 0.0;
    } else {
      mx += (mx > 0.0) ? -kStep : kStep;
    }
  }

  while (std::abs(mz) > 1e-6 && !collidesAt(level, x_, y_ - 0.2, z_ + mz)) {
    if (std::abs(mz) <= kStep) {
      mz = 0.0;
    } else {
      mz += (mz > 0.0) ? -kStep : kStep;
    }
  }

  while (std::abs(mx) > 1e-6 && std::abs(mz) > 1e-6 && !collidesAt(level, x_ + mx, y_ - 0.2, z_ + mz)) {
    if (std::abs(mx) <= kStep) {
      mx = 0.0;
    } else {
      mx += (mx > 0.0) ? -kStep : kStep;
    }
    if (std::abs(mz) <= kStep) {
      mz = 0.0;
    } else {
      mz += (mz > 0.0) ? -kStep : kStep;
    }
  }

  *dx = mx;
  *dz = mz;
}

double Mob::moveAxisWithCollision(Level* level, double delta, int axis, bool* collided) const {
  if (collided) {
    *collided = false;
  }
  if (std::abs(delta) < 1e-9) {
    return 0.0;
  }

  constexpr double kStep = 0.05;
  const int steps = std::max(1, static_cast<int>(std::ceil(std::abs(delta) / kStep)));
  const double step = delta / static_cast<double>(steps);
  double moved = 0.0;

  for (int i = 0; i < steps; ++i) {
    double nx = x_;
    double ny = y_;
    double nz = z_;
    if (axis == 0) {
      nx += moved + step;
    } else if (axis == 1) {
      ny += moved + step;
    } else {
      nz += moved + step;
    }

    if (collidesAt(level, nx, ny, nz)) {
      if (collided) {
        *collided = true;
      }
      break;
    }
    moved += step;
  }

  return moved;
}

float Mob::groundFriction(Level* level) const {
  if (!level) {
    return kGroundFrictionBase;
  }
  const int belowX = static_cast<int>(std::floor(x_));
  const int belowY = static_cast<int>(std::floor(y_ - 0.01));
  const int belowZ = static_cast<int>(std::floor(z_));
  return level->isEmptyTile(belowX, belowY, belowZ) ? kGroundFrictionBase : kGroundFrictionBase;
}

}  // namespace mc
