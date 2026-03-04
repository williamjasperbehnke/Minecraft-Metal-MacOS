#include "World/Level/Gen/StructureGenerator.h"

namespace mc::gen {

StructureGenerator::StructureGenerator(std::uint32_t seed) : pipeline_(std::make_unique<BiomeDecoratorPipeline>(seed)) {}

void StructureGenerator::applyChunkFeatures(int chunkX, int chunkZ, std::uint8_t* blocks, int chunkHeight,
                                            const std::vector<BiomeSample>& biomes) const {
  if (!pipeline_) {
    return;
  }
  pipeline_->decorateChunk(chunkX, chunkZ, blocks, chunkHeight, biomes);
}

}  // namespace mc::gen
