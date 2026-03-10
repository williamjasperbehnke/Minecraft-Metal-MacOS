#pragma once

#include <algorithm>
#include <random>
#include <vector>

#include <simd/simd.h>

#include "World/Entity/ItemEntity.h"

namespace mc {

class Inventory;
class Level;
class Player;

class ItemEntitySystem {
public:
  void tick(double dtSeconds, Level* level, const Player* player, Inventory* inventory);
  void spawnDrop(int tile, int count, const simd_float3& position, const simd_float3& velocity, int throwTimeTicks);
  void pushItemsOutOfBlock(Level* level, int x, int y, int z);
  const std::vector<ItemEntity>& items() const { return items_; }
  float renderPartialTicks() const { return static_cast<float>(std::clamp(tickRemainder_, 0.0, 1.0)); }

private:
  void mergeNearbyItems();
  void collectIntoPlayer(const Player* player, Inventory* inventory);
  void resolveItemsInsideSolids(Level* level);

  std::vector<ItemEntity> items_{};
  double tickRemainder_ = 0.0;
  std::mt19937 rng_{0xC0FFEEu};
};

}  // namespace mc
