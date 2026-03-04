#include "World/Level/ChunkSource.h"

#include "World/Chunk/LevelChunk.h"
#include "World/Level/Gen/OverworldGenerator.h"

namespace mc {

TerrainChunkSource::TerrainChunkSource(std::uint32_t seed) : generator_(std::make_unique<gen::OverworldGenerator>(seed)) {}

TerrainChunkSource::~TerrainChunkSource() = default;

void TerrainChunkSource::fillChunk(LevelChunk& chunk) {
  generator_->fillChunk(chunk);
}

}  // namespace mc
