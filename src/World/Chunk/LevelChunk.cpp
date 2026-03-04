#include "World/Chunk/LevelChunk.h"

#include "World/Level/Level.h"

namespace mc {

LevelChunk::LevelChunk(Level* level, int chunkX, int chunkZ)
    : level_(level), chunkX_(chunkX), chunkZ_(chunkZ) {
  blocks_.fill(0);
}

int LevelChunk::getTile(int x, int y, int z) const {
  if (x < 0 || x >= kSizeX || y < 0 || y >= kSizeY || z < 0 || z >= kSizeZ) {
    return 0;
  }
  return blocks_[index(x, y, z)];
}

bool LevelChunk::setTile(int x, int y, int z, std::uint8_t tile) {
  if (x < 0 || x >= kSizeX || y < 0 || y >= kSizeY || z < 0 || z >= kSizeZ) {
    return false;
  }
  blocks_[index(x, y, z)] = tile;
  return true;
}

}  // namespace mc
