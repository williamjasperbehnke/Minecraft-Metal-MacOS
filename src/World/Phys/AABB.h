#pragma once

namespace mc {

struct AABB {
  double minX = 0.0;
  double minY = 0.0;
  double minZ = 0.0;
  double maxX = 0.0;
  double maxY = 0.0;
  double maxZ = 0.0;

  bool intersects(const AABB& other) const {
    return !(maxX <= other.minX || minX >= other.maxX || maxY <= other.minY || minY >= other.maxY || maxZ <= other.minZ ||
             minZ >= other.maxZ);
  }

  AABB moved(double dx, double dy, double dz) const {
    return {
        .minX = minX + dx,
        .minY = minY + dy,
        .minZ = minZ + dz,
        .maxX = maxX + dx,
        .maxY = maxY + dy,
        .maxZ = maxZ + dz,
    };
  }
};

}  // namespace mc
