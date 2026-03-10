#include "World/Entity/Entity.h"

namespace mc {

void Entity::tick(Level* /*level*/, double /*dtSeconds*/) {
}

void Entity::setPosition(double x, double y, double z) {
  x_ = x;
  y_ = y;
  z_ = z;
}

void Entity::setSize(double width, double height) {
  hitboxWidth_ = width;
  hitboxHeight_ = height;
}

AABB Entity::aabb() const {
  return aabbAt(x_, y_, z_);
}

AABB Entity::aabbAt(double x, double y, double z) const {
  const double half = hitboxWidth_ * 0.5;
  return {
      .minX = x - half,
      .minY = y,
      .minZ = z - half,
      .maxX = x + half,
      .maxY = y + hitboxHeight_,
      .maxZ = z + half,
  };
}

}  // namespace mc
