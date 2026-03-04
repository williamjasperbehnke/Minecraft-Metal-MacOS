#include "World/Level/Gen/BiomeProvider.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>

#include "World/Tile/Tile.h"

namespace mc::gen {

namespace {

struct BiomeInfo {
  int id;
  double temperature;
  double downfall;
  double depth;
  double scale;
  std::uint8_t top;
  std::uint8_t filler;
  BiomeKind kind;
};

constexpr BiomeInfo kBiomeTable[] = {
    {biomeid::Ocean, -1.0, 0.5, -1.0, 0.4, static_cast<std::uint8_t>(TileId::Sand), static_cast<std::uint8_t>(TileId::Sand),
     BiomeKind::Ocean},
    {biomeid::Plains, 0.8, 0.4, 0.125, 0.10, static_cast<std::uint8_t>(TileId::Grass), static_cast<std::uint8_t>(TileId::Dirt),
     BiomeKind::Plains},
    {biomeid::Desert, 1.0, 0.0, 0.20, 0.16, static_cast<std::uint8_t>(TileId::Sand), static_cast<std::uint8_t>(TileId::Sand),
     BiomeKind::Desert},
    {biomeid::ExtremeHills, 0.2, 0.3, 0.45, 1.2, static_cast<std::uint8_t>(TileId::Grass), static_cast<std::uint8_t>(TileId::Dirt),
     BiomeKind::Mountains},
    {biomeid::Forest, 0.7, 0.8, 0.18, 0.20, static_cast<std::uint8_t>(TileId::Grass), static_cast<std::uint8_t>(TileId::Dirt),
     BiomeKind::Forest},
    {biomeid::Taiga, 0.05, 0.8, 0.20, 0.30, static_cast<std::uint8_t>(TileId::Grass), static_cast<std::uint8_t>(TileId::Dirt),
     BiomeKind::Taiga},
    {biomeid::Swampland, 0.8, 0.9, -0.2, 0.1, static_cast<std::uint8_t>(TileId::Grass), static_cast<std::uint8_t>(TileId::Dirt),
     BiomeKind::Forest},
    {biomeid::River, 0.5, 0.5, -0.5, 0.0, static_cast<std::uint8_t>(TileId::Sand), static_cast<std::uint8_t>(TileId::Sand),
     BiomeKind::River},
    {biomeid::FrozenOcean, 0.0, 0.5, -1.0, 0.35, static_cast<std::uint8_t>(TileId::Ice), static_cast<std::uint8_t>(TileId::Sand),
     BiomeKind::Ocean},
    {biomeid::FrozenRiver, 0.0, 0.5, -0.5, 0.0, static_cast<std::uint8_t>(TileId::Ice), static_cast<std::uint8_t>(TileId::Sand),
     BiomeKind::River},
    {biomeid::IcePlains, 0.0, 0.5, 0.22, 0.26, static_cast<std::uint8_t>(TileId::Snow), static_cast<std::uint8_t>(TileId::Dirt),
     BiomeKind::Taiga},
    {biomeid::MushroomIsland, 0.9, 1.0, 0.2, 1.0, static_cast<std::uint8_t>(TileId::Grass),
     static_cast<std::uint8_t>(TileId::Dirt), BiomeKind::Forest},
    {biomeid::MushroomIslandShore, 0.9, 1.0, -1.0, 0.1, static_cast<std::uint8_t>(TileId::Grass),
     static_cast<std::uint8_t>(TileId::Dirt), BiomeKind::Forest},
    {biomeid::Beach, 0.8, 0.4, 0.0, 0.1, static_cast<std::uint8_t>(TileId::Sand), static_cast<std::uint8_t>(TileId::Sand),
     BiomeKind::Plains},
    {biomeid::DesertHills, 1.0, 0.0, 0.55, 0.85, static_cast<std::uint8_t>(TileId::Sand), static_cast<std::uint8_t>(TileId::Sandstone),
     BiomeKind::Desert},
    {biomeid::ForestHills, 0.7, 0.8, 0.3, 0.7, static_cast<std::uint8_t>(TileId::Grass), static_cast<std::uint8_t>(TileId::Dirt),
     BiomeKind::Forest},
    {biomeid::TaigaHills, 0.05, 0.8, 0.3, 0.8, static_cast<std::uint8_t>(TileId::Grass), static_cast<std::uint8_t>(TileId::Dirt),
     BiomeKind::Taiga},
    {biomeid::ExtremeHillsEdge, 0.2, 0.3, 0.2, 0.8, static_cast<std::uint8_t>(TileId::Grass),
     static_cast<std::uint8_t>(TileId::Dirt), BiomeKind::Mountains},
    {biomeid::Jungle, 1.0, 0.9, 0.2, 0.4, static_cast<std::uint8_t>(TileId::Grass), static_cast<std::uint8_t>(TileId::Dirt),
     BiomeKind::Forest},
    {biomeid::JungleHills, 1.0, 0.9, 1.8, 0.5, static_cast<std::uint8_t>(TileId::Grass), static_cast<std::uint8_t>(TileId::Dirt),
     BiomeKind::Mountains},
};

const BiomeInfo& biomeInfo(int id) {
  for (const auto& info : kBiomeTable) {
    if (info.id == id) {
      return info;
    }
  }
  return kBiomeTable[1];
}

}  // namespace

BiomeProvider::BiomeProvider(std::uint32_t seed) : layers_(std::make_unique<LayerStack>(seed)) {}

BiomeSample BiomeProvider::sampleBiomeAt(int worldX, int worldZ) const {
  const std::vector<int> ids = sampleZoomedBiomeIdsThreadSafe(worldX, worldZ, 1, 1);
  const BiomeInfo& info = biomeInfo(ids.empty() ? biomeid::Plains : ids[0]);

  BiomeSample out;
  out.temperature = std::clamp(info.temperature, 0.0, 1.0);
  out.humidity = std::clamp(info.downfall, 0.0, 1.0);
  out.depth = info.depth;
  out.scale = info.scale;
  out.top = info.top;
  out.filler = info.filler;
  out.kind = info.kind;
  return out;
}

void BiomeProvider::sampleChunkBiomes(int chunkX, int chunkZ, std::vector<BiomeSample>& out) const {
  out.assign(16 * 16, BiomeSample{});
  const std::vector<int> ids = sampleZoomedBiomeIdsThreadSafe(chunkX * 16, chunkZ * 16, 16, 16);
  for (int i = 0; i < 16 * 16; ++i) {
    const BiomeInfo& info = biomeInfo(ids[i]);
    BiomeSample b;
    b.temperature = std::clamp(info.temperature, 0.0, 1.0);
    b.humidity = std::clamp(info.downfall, 0.0, 1.0);
    b.depth = info.depth;
    b.scale = info.scale;
    b.top = info.top;
    b.filler = info.filler;
    b.kind = info.kind;
    out[i] = b;
  }
}

void BiomeProvider::sampleDepthScaleGrid(int startX, int startZ, int xSize, int zSize, std::vector<double>& depths,
                                         std::vector<double>& scales, int sampleStep) const {
  if (sampleStep < 1) {
    sampleStep = 1;
  }

  const int size = xSize * zSize;
  depths.assign(size, 0.0);
  scales.assign(size, 0.0);

  const std::vector<int> ids = sampleRawBiomeIdsThreadSafe(startX * sampleStep, startZ * sampleStep, xSize, zSize);
  for (int i = 0; i < size; ++i) {
    const BiomeInfo& info = biomeInfo(ids[i]);
    depths[i] = info.depth;
    scales[i] = info.scale;
  }
}

std::vector<int> BiomeProvider::sampleRawBiomeIdsThreadSafe(int x, int z, int w, int h) const {
  std::lock_guard<std::mutex> lock(layersMutex_);
  return layers_->sampleRawBiomeIds(x, z, w, h);
}

std::vector<int> BiomeProvider::sampleZoomedBiomeIdsThreadSafe(int x, int z, int w, int h) const {
  std::lock_guard<std::mutex> lock(layersMutex_);
  return layers_->sampleZoomedBiomeIds(x, z, w, h);
}

}  // namespace mc::gen
