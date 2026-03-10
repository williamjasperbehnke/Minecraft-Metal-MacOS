#include "World/Entity/ItemEntity.h"

#include <algorithm>
#include <cmath>

#include "World/Level/Level.h"
#include "World/Tile/Tile.h"

namespace mc {

namespace {

constexpr double kGravityPerTick = 0.04;
constexpr double kAirFriction = 0.98;
constexpr double kGroundFriction = 0.6 * 0.98;
constexpr double kGroundBounce = -0.5;
constexpr double kWaterFriction = 0.80;
constexpr double kWaterBuoyancy = 0.0425;

}  // namespace

ItemEntity::ItemEntity(int tile, int count, double x, double y, double z, std::mt19937& rng)
    : tile_(tile), count_(count) {
  setPosition(x, y, z);
  setSize(kHalfWidth * 2.0, kHalfHeight * 2.0);
  std::uniform_real_distribution<float> unit01(0.0f, 1.0f);
  std::uniform_real_distribution<double> vel(-0.1, 0.1);
  bobOffs_ = unit01(rng) * 6.283185307f;
  yawRadians_ = unit01(rng) * 6.283185307f;
  xd_ = vel(rng);
  yd_ = 0.2;
  zd_ = vel(rng);
}

void ItemEntity::tick(Level* level, double /*dtSeconds*/) {
  tick(level);
}

void ItemEntity::tick(Level* level) {
  if (!isAlive()) {
    return;
  }

  if (throwTimeTicks_ > 0) {
    --throwTimeTicks_;
  }

  const bool inWater = isInWater(level);

  yd_ -= kGravityPerTick;
  if (inWater) {
    yd_ += kWaterBuoyancy;
    if (yd_ < -0.02) {
      yd_ = -0.02;
    }
  }

  bool collidedY = false;
  bool collidedX = false;
  bool collidedZ = false;
  moveAxis(level, 1, yd_, &collidedY);
  moveAxis(level, 0, xd_, &collidedX);
  moveAxis(level, 2, zd_, &collidedZ);

  if (collidedX) {
    xd_ = 0.0;
  }
  if (collidedZ) {
    zd_ = 0.0;
  }

  if (collidedY) {
    onGround_ = (yd_ < 0.0);
    if (onGround_) {
      yd_ *= kGroundBounce;
    } else {
      yd_ = 0.0;
    }
  } else {
    onGround_ = false;
  }

  const double friction = onGround_ ? kGroundFriction : kAirFriction;
  if (inWater) {
    xd_ *= kWaterFriction;
    yd_ *= kWaterFriction;
    zd_ *= kWaterFriction;
  } else {
    xd_ *= friction;
    yd_ *= kAirFriction;
    zd_ *= friction;
  }
  yawRadians_ += static_cast<float>(0.06);
  if (yawRadians_ > 6.283185307f) {
    yawRadians_ -= 6.283185307f;
  }

  ++ageTicks_;
  if (ageTicks_ >= kLifetimeTicks) {
    kill();
  }
}

bool ItemEntity::tryMerge(ItemEntity& target) {
  if (&target == this || !isAlive() || !target.isAlive()) {
    return false;
  }
  if (tile_ != target.tile_) {
    return false;
  }
  if (target.count_ >= count_) {
    // keep merge direction stable with larger/equal target first
  } else {
    return target.tryMerge(*this);
  }
  if (target.count_ + count_ > kMaxStackSize) {
    return false;
  }

  target.count_ += count_;
  target.throwTimeTicks_ = std::max(target.throwTimeTicks_, throwTimeTicks_);
  target.ageTicks_ = std::min(target.ageTicks_, ageTicks_);
  kill();
  return true;
}

bool ItemEntity::intersectsAabb(double minX, double minY, double minZ, double maxX, double maxY, double maxZ) const {
  return aabbAt(x_, y_, z_).intersects({minX, minY, minZ, maxX, maxY, maxZ});
}

void ItemEntity::setCount(int count) {
  count_ = count;
  if (count_ <= 0) {
    kill();
  }
}

float ItemEntity::bob() const {
  return bob(0.0f);
}

float ItemEntity::bob(float partialTicks) const {
  const float pt = std::clamp(partialTicks, 0.0f, 1.0f);
  // Keep a small classic bobbing offset so world drops don't look static.
  return std::sin((static_cast<float>(ageTicks_) + pt) * 0.18f + bobOffs_) * 0.06f + 0.02f;
}

float ItemEntity::yawRadians(float partialTicks) const {
  const float pt = std::clamp(partialTicks, 0.0f, 1.0f);
  float yaw = yawRadians_ + 0.06f * pt;
  if (yaw > 6.283185307f) {
    yaw -= 6.283185307f;
  }
  return yaw;
}

void ItemEntity::setThrowTimeTicks(int ticks) {
  throwTimeTicks_ = std::max(0, ticks);
}

void ItemEntity::setMotion(double xd, double yd, double zd) {
  xd_ = xd;
  yd_ = yd;
  zd_ = zd;
}

bool ItemEntity::isInWater(Level* level) const {
  if (!level) {
    return false;
  }
  const AABB box = aabbAt(x_, y_, z_);
  const int x0 = static_cast<int>(std::floor(box.minX));
  const int x1 = static_cast<int>(std::floor(box.maxX - 1e-6));
  const int y0 = static_cast<int>(std::floor(box.minY));
  const int y1 = static_cast<int>(std::floor(box.maxY - 1e-6));
  const int z0 = static_cast<int>(std::floor(box.minZ));
  const int z1 = static_cast<int>(std::floor(box.maxZ - 1e-6));

  for (int by = y0; by <= y1; ++by) {
    for (int bz = z0; bz <= z1; ++bz) {
      for (int bx = x0; bx <= x1; ++bx) {
        if (level->getTile(bx, by, bz) != static_cast<int>(TileId::Water)) {
          continue;
        }
        const AABB tileBox{
            .minX = static_cast<double>(bx),
            .minY = static_cast<double>(by),
            .minZ = static_cast<double>(bz),
            .maxX = static_cast<double>(bx + 1),
            .maxY = static_cast<double>(by + 1),
            .maxZ = static_cast<double>(bz + 1),
        };
        if (box.intersects(tileBox)) {
          return true;
        }
      }
    }
  }
  return false;
}

bool ItemEntity::collidesAt(Level* level, double x, double y, double z) const {
  if (!level) {
    return false;
  }
  const AABB box = aabbAt(x, y, z);
  const int x0 = static_cast<int>(std::floor(box.minX));
  const int x1 = static_cast<int>(std::floor(box.maxX - 1e-6));
  const int y0 = static_cast<int>(std::floor(box.minY));
  const int y1 = static_cast<int>(std::floor(box.maxY - 1e-6));
  const int z0 = static_cast<int>(std::floor(box.minZ));
  const int z1 = static_cast<int>(std::floor(box.maxZ - 1e-6));

  for (int by = y0; by <= y1; ++by) {
    for (int bz = z0; bz <= z1; ++bz) {
      for (int bx = x0; bx <= x1; ++bx) {
        if (!isSolidTileId(level->getTile(bx, by, bz))) {
          continue;
        }
        const AABB tileBox{
            .minX = static_cast<double>(bx),
            .minY = static_cast<double>(by),
            .minZ = static_cast<double>(bz),
            .maxX = static_cast<double>(bx + 1),
            .maxY = static_cast<double>(by + 1),
            .maxZ = static_cast<double>(bz + 1),
        };
        if (box.intersects(tileBox)) {
          return true;
        }
      }
    }
  }
  return false;
}

AABB ItemEntity::aabbAt(double x, double y, double z) const {
  const double halfW = hitboxWidth() * 0.5;
  const double halfH = hitboxHeight() * 0.5;
  return {
      .minX = x - halfW,
      .minY = y - halfH,
      .minZ = z - halfW,
      .maxX = x + halfW,
      .maxY = y + halfH,
      .maxZ = z + halfW,
  };
}

void ItemEntity::moveAxis(Level* level, int axis, double delta, bool* collided) {
  if (collided) {
    *collided = false;
  }
  if (std::abs(delta) < 1e-9) {
    return;
  }

  constexpr double kStep = 0.05;
  const int steps = std::max(1, static_cast<int>(std::ceil(std::abs(delta) / kStep)));
  const double stepDelta = delta / static_cast<double>(steps);
  for (int i = 0; i < steps; ++i) {
    double nx = x_;
    double ny = y_;
    double nz = z_;
    if (axis == 0) {
      nx += stepDelta;
    } else if (axis == 1) {
      ny += stepDelta;
    } else {
      nz += stepDelta;
    }
    if (collidesAt(level, nx, ny, nz)) {
      if (collided) {
        *collided = true;
      }
      return;
    }
    x_ = nx;
    y_ = ny;
    z_ = nz;
  }
}

}  // namespace mc
