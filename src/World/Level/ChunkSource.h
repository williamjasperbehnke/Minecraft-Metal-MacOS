#pragma once

#include <cstdint>
#include <memory>

namespace mc {

class LevelChunk;
namespace gen {
class OverworldGenerator;
}

class ChunkSource {
public:
  virtual ~ChunkSource() = default;
  virtual void fillChunk(LevelChunk& chunk) = 0;
};

class TerrainChunkSource : public ChunkSource {
public:
  explicit TerrainChunkSource(std::uint32_t seed);
  ~TerrainChunkSource() override;
  void fillChunk(LevelChunk& chunk) override;

private:
  std::unique_ptr<gen::OverworldGenerator> generator_;
};

}  // namespace mc
