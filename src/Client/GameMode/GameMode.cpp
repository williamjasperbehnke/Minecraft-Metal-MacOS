#include "Client/GameMode/GameMode.h"

#include <algorithm>

#include "Client/Core/Minecraft.h"
#include "World/Entity/Player.h"
#include "World/Level/Level.h"
#include "World/Tile/Tile.h"

namespace mc {

GameMode::GameMode(Minecraft* minecraft) : minecraft_(minecraft) {}

bool GameMode::hasLevelAndPlayer() const {
  return level_ && player_;
}

bool GameMode::withinBlockReach(int x, int y, int z) const {
  if (!player_) {
    return false;
  }

  constexpr double kEyeHeight = 1.62;
  const double eyeX = player_->x();
  const double eyeY = player_->y() + kEyeHeight;
  const double eyeZ = player_->z();

  const double bMinX = static_cast<double>(x);
  const double bMaxX = bMinX + 1.0;
  const double bMinY = static_cast<double>(y);
  const double bMaxY = bMinY + 1.0;
  const double bMinZ = static_cast<double>(z);
  const double bMaxZ = bMinZ + 1.0;

  const double closestX = std::clamp(eyeX, bMinX, bMaxX);
  const double closestY = std::clamp(eyeY, bMinY, bMaxY);
  const double closestZ = std::clamp(eyeZ, bMinZ, bMaxZ);

  const double dx = eyeX - closestX;
  const double dy = eyeY - closestY;
  const double dz = eyeZ - closestZ;
  const double reach = player_->blockReach();
  return (dx * dx + dy * dy + dz * dz) <= (reach * reach);
}

bool GameMode::inBuildHeight(int y) const {
  return y >= Level::minBuildHeight && y < Level::maxBuildHeight;
}

bool GameMode::canReplaceForPlacement(int tile) const {
  return tile == static_cast<int>(TileId::Air) || tile == static_cast<int>(TileId::Water);
}

bool GameMode::blocksPlayerPlacement(int x, int y, int z) const {
  if (!player_) {
    return true;
  }

  constexpr double kPlayerHalfWidth = 0.3;
  constexpr double kPlayerHeight = 1.8;
  const double pMinX = player_->x() - kPlayerHalfWidth;
  const double pMaxX = player_->x() + kPlayerHalfWidth;
  const double pMinY = player_->y();
  const double pMaxY = player_->y() + kPlayerHeight;
  const double pMinZ = player_->z() - kPlayerHalfWidth;
  const double pMaxZ = player_->z() + kPlayerHalfWidth;

  const double bMinX = static_cast<double>(x);
  const double bMaxX = static_cast<double>(x) + 1.0;
  const double bMinY = static_cast<double>(y);
  const double bMaxY = static_cast<double>(y) + 1.0;
  const double bMinZ = static_cast<double>(z);
  const double bMaxZ = static_cast<double>(z) + 1.0;

  const bool xOverlap = pMinX < bMaxX && pMaxX > bMinX;
  const bool yOverlap = pMinY < bMaxY && pMaxY > bMinY;
  const bool zOverlap = pMinZ < bMaxZ && pMaxZ > bMinZ;
  return xOverlap && yOverlap && zOverlap;
}

bool GameMode::canDestroyTile(int tile) const {
  return tile != static_cast<int>(TileId::Water) && tile != static_cast<int>(TileId::Bedrock);
}

bool GameMode::destroyBlockAt(int x, int y, int z) {
  if (!hasLevelAndPlayer() || !withinBlockReach(x, y, z)) {
    return false;
  }

  const int tile = level_->getTile(x, y, z);
  if (!canDestroyTile(tile)) {
    return false;
  }
  return level_->setTile(x, y, z, static_cast<int>(TileId::Air));
}

bool GameMode::placeBlockAt(int x, int y, int z) {
  if (!hasLevelAndPlayer() || !withinBlockReach(x, y, z) || !inBuildHeight(y)) {
    return false;
  }

  const int existing = level_->getTile(x, y, z);
  if (!canReplaceForPlacement(existing) || blocksPlayerPlacement(x, y, z)) {
    return false;
  }
  return level_->setTile(x, y, z, placeTile());
}

int GameMode::placeTile() const {
  return minecraft_ ? minecraft_->selectedPlaceTile() : static_cast<int>(TileId::Grass);
}

}  // namespace mc
