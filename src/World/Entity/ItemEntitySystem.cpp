#include "World/Entity/ItemEntitySystem.h"

#include <algorithm>
#include <cmath>

#include "Client/Inventory/Inventory.h"
#include "World/Entity/Player.h"
#include "World/Level/Level.h"
#include "World/Tile/Tile.h"

namespace mc {

namespace {

constexpr double kItemTicksPerSecond = 20.0;
constexpr int kMaxItemTickStepsPerFrame = 8;
constexpr double kItemHalfWidth = 0.125;
constexpr double kItemHalfHeight = 0.125;
constexpr double kPlayerHalfWidth = 0.85;
constexpr double kPlayerHeight = 2.2;

}  // namespace

void ItemEntitySystem::tick(double dtSeconds, Level* level, const Player* player, Inventory* inventory) {
  if (!level || dtSeconds <= 0.0) {
    return;
  }
  if (items_.empty()) {
    tickRemainder_ = 0.0;
    return;
  }

  tickRemainder_ += dtSeconds * kItemTicksPerSecond;
  int steps = static_cast<int>(std::floor(tickRemainder_));
  if (steps <= 0) {
    return;
  }
  steps = std::min(steps, kMaxItemTickStepsPerFrame);
  tickRemainder_ -= static_cast<double>(steps);

  for (int i = 0; i < steps; ++i) {
    for (ItemEntity& item : items_) {
      item.tick(level);
    }
    resolveItemsInsideSolids(level);
    mergeNearbyItems();
    collectIntoPlayer(player, inventory);
    items_.erase(std::remove_if(items_.begin(), items_.end(), [](const ItemEntity& item) { return !item.isAlive(); }),
                 items_.end());
  }
}

void ItemEntitySystem::spawnDrop(int tile, int count, const simd_float3& position, const simd_float3& velocity, int throwTimeTicks) {
  if (tile <= static_cast<int>(TileId::Air) || tile == static_cast<int>(TileId::Water) || count <= 0) {
    return;
  }
  int remaining = count;
  while (remaining > 0) {
    const int stackCount = std::min(ItemEntity::kMaxStackSize, remaining);
    ItemEntity item(tile, stackCount, position.x, position.y, position.z, rng_);
    item.setMotion(velocity.x, velocity.y, velocity.z);
    item.setThrowTimeTicks(throwTimeTicks);
    items_.push_back(std::move(item));
    remaining -= stackCount;
  }
}

void ItemEntitySystem::pushItemsOutOfBlock(Level* level, int x, int y, int z) {
  if (!level || items_.empty()) {
    return;
  }
  const double minX = static_cast<double>(x);
  const double maxX = static_cast<double>(x + 1);
  const double minY = static_cast<double>(y);
  const double maxY = static_cast<double>(y + 1);
  const double minZ = static_cast<double>(z);
  const double maxZ = static_cast<double>(z + 1);
  constexpr double eps = 0.001;

  for (ItemEntity& item : items_) {
    if (!item.isAlive()) {
      continue;
    }
    const double itemMinX = item.x() - kItemHalfWidth;
    const double itemMaxX = item.x() + kItemHalfWidth;
    const double itemMinY = item.y() - kItemHalfHeight;
    const double itemMaxY = item.y() + kItemHalfHeight;
    const double itemMinZ = item.z() - kItemHalfWidth;
    const double itemMaxZ = item.z() + kItemHalfWidth;
    const bool intersects = !(itemMaxX <= minX || itemMinX >= maxX || itemMaxY <= minY || itemMinY >= maxY || itemMaxZ <= minZ || itemMinZ >= maxZ);
    if (!intersects) {
      continue;
    }

    const double moveLeft = minX - itemMaxX - eps;
    const double moveRight = maxX - itemMinX + eps;
    const double moveUp = maxY - itemMinY + eps;
    const double moveNorth = minZ - itemMaxZ - eps;
    const double moveSouth = maxZ - itemMinZ + eps;

    struct Push {
      double dx;
      double dy;
      double dz;
      double mag;
      int priority;
    };
    const std::array<Push, 6> pushes = {{
        {moveLeft, 0.0, 0.0, std::abs(moveLeft), 2},
        {moveRight, 0.0, 0.0, std::abs(moveRight), 2},
        {0.0, moveUp, 0.0, std::abs(moveUp), 1},
        {0.0, 0.0, moveNorth, std::abs(moveNorth), 2},
        {0.0, 0.0, moveSouth, std::abs(moveSouth), 2},
        // Fallback: allow downward only as last resort.
        {0.0, (minY - itemMaxY - eps), 0.0, std::abs(minY - itemMaxY - eps), 4},
    }};

    Push best = pushes[0];
    for (std::size_t i = 1; i < pushes.size(); ++i) {
      const Push& p = pushes[i];
      if (p.mag < best.mag - 1e-6 || (std::abs(p.mag - best.mag) <= 1e-6 && p.priority < best.priority)) {
        best = p;
      }
    }

    auto isSolidAtCenter = [&](double cx, double cy, double cz) {
      const int tx = static_cast<int>(std::floor(cx));
      const int ty = static_cast<int>(std::floor(cy));
      const int tz = static_cast<int>(std::floor(cz));
      return isSolidTileId(level->getTile(tx, ty, tz));
    };

    bool applied = false;
    std::array<Push, 6> ordered = pushes;
    std::sort(ordered.begin(), ordered.end(), [](const Push& a, const Push& b) {
      if (a.priority != b.priority) return a.priority < b.priority;
      return a.mag < b.mag;
    });

    for (const Push& p : ordered) {
      const double nx = item.x() + p.dx;
      const double ny = item.y() + p.dy;
      const double nz = item.z() + p.dz;
      const double nMinX = nx - kItemHalfWidth;
      const double nMaxX = nx + kItemHalfWidth;
      const double nMinY = ny - kItemHalfHeight;
      const double nMaxY = ny + kItemHalfHeight;
      const double nMinZ = nz - kItemHalfWidth;
      const double nMaxZ = nz + kItemHalfWidth;
      const bool stillIntersecting =
          !(nMaxX <= minX || nMinX >= maxX || nMaxY <= minY || nMinY >= maxY || nMaxZ <= minZ || nMinZ >= maxZ);
      if (stillIntersecting || isSolidAtCenter(nx, ny, nz)) {
        continue;
      }
      constexpr double kNudgeFraction = 0.38;
      const double dx = p.dx * kNudgeFraction;
      const double dy = p.dy * kNudgeFraction;
      const double dz = p.dz * kNudgeFraction;
      item.setPosition(item.x() + dx, item.y() + dy, item.z() + dz);
      item.setMotion(item.motionX() + dx * 0.45, std::max(item.motionY() + dy * 0.45, (p.dy > 0.0 ? 0.05 : item.motionY())),
                     item.motionZ() + dz * 0.45);
      applied = true;
      break;
    }

    if (!applied) {
      // Emergency fallback: gently nudge upward instead of instant pop.
      item.setPosition(item.x(), item.y() + 0.12, item.z());
      item.setMotion(item.motionX(), std::max(item.motionY(), 0.08), item.motionZ());
    }
  }
}

void ItemEntitySystem::resolveItemsInsideSolids(Level* level) {
  if (!level) {
    return;
  }
  for (ItemEntity& item : items_) {
    if (!item.isAlive()) {
      continue;
    }
    const int tx = static_cast<int>(std::floor(item.x()));
    const int ty = static_cast<int>(std::floor(item.y()));
    const int tz = static_cast<int>(std::floor(item.z()));
    if (!isSolidTileId(level->getTile(tx, ty, tz))) {
      continue;
    }
    pushItemsOutOfBlock(level, tx, ty, tz);
  }
}

void ItemEntitySystem::mergeNearbyItems() {
  for (std::size_t i = 0; i < items_.size(); ++i) {
    ItemEntity& a = items_[i];
    if (!a.isAlive() || a.count() >= ItemEntity::kMaxStackSize) {
      continue;
    }
    for (std::size_t j = i + 1; j < items_.size(); ++j) {
      ItemEntity& b = items_[j];
      if (!b.isAlive() || a.tile() != b.tile()) {
        continue;
      }
      const double dx = a.x() - b.x();
      const double dy = a.y() - b.y();
      const double dz = a.z() - b.z();
      if ((dx * dx + dy * dy + dz * dz) > 0.25) {
        continue;
      }
      if (a.tryMerge(b) && a.count() >= ItemEntity::kMaxStackSize) {
        break;
      }
    }
  }
}

void ItemEntitySystem::collectIntoPlayer(const Player* player, Inventory* inventory) {
  if (!player || !inventory) {
    return;
  }

  const double px = player->x();
  const double py = player->y();
  const double pz = player->z();
  const double minX = px - kPlayerHalfWidth;
  const double maxX = px + kPlayerHalfWidth;
  const double minY = py;
  const double maxY = py + kPlayerHeight;
  const double minZ = pz - kPlayerHalfWidth;
  const double maxZ = pz + kPlayerHalfWidth;

  for (ItemEntity& item : items_) {
    if (!item.isAlive() || item.throwTimeTicks() > 0) {
      continue;
    }
    if (!item.intersectsAabb(minX, minY, minZ, maxX, maxY, maxZ)) {
      continue;
    }
    item.setCount(inventory->addItem(item.tile(), item.count()));
  }
}

}  // namespace mc
