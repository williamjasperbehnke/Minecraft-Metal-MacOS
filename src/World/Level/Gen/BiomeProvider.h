#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include "World/Level/Gen/BiomeLayers.h"
#include "World/Tile/Tile.h"

namespace mc::gen {

enum class BiomeKind {
  Ocean,
  Plains,
  Forest,
  Desert,
  Taiga,
  Mountains,
};

struct BiomeSample {
  double temperature = 0.5;
  double humidity = 0.5;
  double depth = 0.125;
  double scale = 0.05;
  std::uint8_t top = static_cast<std::uint8_t>(TileId::Grass);
  std::uint8_t filler = static_cast<std::uint8_t>(TileId::Dirt);
  BiomeKind kind = BiomeKind::Plains;
};

class BiomeProvider {
public:
  explicit BiomeProvider(std::uint32_t seed);

  void sampleChunkBiomes(int chunkX, int chunkZ, std::vector<BiomeSample>& out) const;
  void sampleDepthScaleGrid(int startX, int startZ, int xSize, int zSize, std::vector<double>& depths,
                            std::vector<double>& scales, int sampleStep = 1) const;

private:
  BiomeSample sampleBiomeAt(int worldX, int worldZ) const;
  std::vector<int> sampleRawBiomeIdsThreadSafe(int x, int z, int w, int h) const;
  std::vector<int> sampleZoomedBiomeIdsThreadSafe(int x, int z, int w, int h) const;

  std::unique_ptr<LayerStack> layers_;
  mutable std::mutex layersMutex_;
};

}  // namespace mc::gen
