#pragma once

#include <array>
#include <cstdint>

namespace mc {

class Level;

class LevelChunk {
public:
  static constexpr int kSizeX = 16;
  static constexpr int kSizeY = 128;
  static constexpr int kSizeZ = 16;

  LevelChunk(Level* level, int chunkX, int chunkZ);

  int getTile(int x, int y, int z) const;
  bool setTile(int x, int y, int z, std::uint8_t tile);

  int chunkX() const { return chunkX_; }
  int chunkZ() const { return chunkZ_; }

private:
  static constexpr int index(int x, int y, int z) {
    return (y * kSizeZ + z) * kSizeX + x;
  }

  Level* level_;
  int chunkX_;
  int chunkZ_;
  std::array<std::uint8_t, kSizeX * kSizeY * kSizeZ> blocks_{};
};

}  // namespace mc
