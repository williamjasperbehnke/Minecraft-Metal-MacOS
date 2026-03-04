#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "World/Level/Gen/BiomeProvider.h"
#include "World/Level/Gen/Decorators.h"

namespace mc::gen {

class StructureGenerator {
public:
  explicit StructureGenerator(std::uint32_t seed);

  void applyChunkFeatures(int chunkX, int chunkZ, std::uint8_t* blocks, int chunkHeight,
                          const std::vector<BiomeSample>& biomes) const;

private:
  std::unique_ptr<BiomeDecoratorPipeline> pipeline_;
};

}  // namespace mc::gen
