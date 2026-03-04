#include "World/Entity/Entity.h"

namespace mc {

void Entity::tick(Level* /*level*/, double /*dtSeconds*/) {
}

void Entity::setPosition(double x, double y, double z) {
  x_ = x;
  y_ = y;
  z_ = z;
}

}  // namespace mc
