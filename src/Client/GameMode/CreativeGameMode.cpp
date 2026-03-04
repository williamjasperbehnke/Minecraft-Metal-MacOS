#include "Client/GameMode/CreativeGameMode.h"

#include "World/Level/Level.h"
#include "World/Tile/Tile.h"

namespace mc {

bool CreativeGameMode::destroyBlockAt(int x, int y, int z) {
  if (!hasLevelAndPlayer() || !withinBlockReach(x, y, z)) {
    return false;
  }

  const int tile = level_->getTile(x, y, z);
  if (!canDestroyTile(tile)) {
    return false;
  }
  return level_->setTile(x, y, z, static_cast<int>(TileId::Air));
}

bool CreativeGameMode::placeBlockAt(int x, int y, int z) {
  if (!hasLevelAndPlayer() || !withinBlockReach(x, y, z) || !inBuildHeight(y)) {
    return false;
  }

  const int existing = level_->getTile(x, y, z);
  if (!canReplaceForPlacement(existing) || blocksPlayerPlacement(x, y, z)) {
    return false;
  }

  return level_->setTile(x, y, z, static_cast<int>(TileId::Grass));
}

}  // namespace mc
