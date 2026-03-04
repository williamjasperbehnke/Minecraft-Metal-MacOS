#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "World/Level/Gen/BiomeProvider.h"

namespace mc {
class LevelChunk;
namespace gen {

struct ChunkDecorationView {
  int chunkX = 0;
  int chunkZ = 0;
  std::uint8_t* blocks = nullptr;
  int chunkHeight = 0;
  const std::vector<BiomeSample>* biomes = nullptr;
};

class ChunkDecorator {
public:
  virtual ~ChunkDecorator() = default;
  virtual void decorate(const ChunkDecorationView& view) const = 0;
};

class OreDecorator final : public ChunkDecorator {
public:
  explicit OreDecorator(std::uint32_t seed);
  void decorate(const ChunkDecorationView& view) const override;

private:
  std::uint32_t seed_ = 0;
};

class VegetationDecorator final : public ChunkDecorator {
public:
  explicit VegetationDecorator(std::uint32_t seed);
  void decorate(const ChunkDecorationView& view) const override;

private:
  std::uint32_t seed_ = 0;
};

class BiomeDecoratorPipeline {
public:
  explicit BiomeDecoratorPipeline(std::uint32_t seed);
  void decorateChunk(int chunkX, int chunkZ, std::uint8_t* blocks, int chunkHeight,
                     const std::vector<BiomeSample>& biomes) const;

private:
  std::vector<std::unique_ptr<ChunkDecorator>> decorators_;
};

// Applies any pending cross-chunk decoration writes to an already existing chunk.
void applyDeferredDecorationWrites(::mc::LevelChunk& chunk);

}  // namespace gen
}  // namespace mc
