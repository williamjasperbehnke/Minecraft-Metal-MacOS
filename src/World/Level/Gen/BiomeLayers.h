#pragma once

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace mc::gen {

namespace biomeid {
constexpr int Ocean = 0;
constexpr int Plains = 1;
constexpr int Desert = 2;
constexpr int ExtremeHills = 3;
constexpr int Forest = 4;
constexpr int Taiga = 5;
constexpr int Swampland = 6;
constexpr int River = 7;
constexpr int FrozenOcean = 10;
constexpr int FrozenRiver = 11;
constexpr int IcePlains = 12;
constexpr int MushroomIsland = 14;
constexpr int MushroomIslandShore = 15;
constexpr int Beach = 16;
constexpr int DesertHills = 17;
constexpr int ForestHills = 18;
constexpr int TaigaHills = 19;
constexpr int ExtremeHillsEdge = 20;
constexpr int Jungle = 21;
constexpr int JungleHills = 22;
}  // namespace biomeid

class Layer {
public:
  explicit Layer(std::int64_t seedMixup);
  virtual ~Layer() = default;

  void setParent(Layer* parent);
  virtual void initSeed(std::int64_t seed);
  std::vector<int> area(int x, int z, int w, int h);
  int nextRandom(int max);

protected:
  virtual std::vector<int> getArea(int x, int z, int w, int h) = 0;
  void initRandom(std::int64_t x, std::int64_t z);

  Layer* parent_ = nullptr;

private:
  std::int64_t seed_ = 0;
  std::int64_t rval_ = 0;
  std::int64_t seedMixup_ = 0;
};

class LayerStack {
public:
  explicit LayerStack(std::uint32_t seed);

  std::vector<int> sampleRawBiomeIds(int x, int z, int w, int h) const;
  std::vector<int> sampleZoomedBiomeIds(int x, int z, int w, int h) const;

  template <typename T, typename... Args>
  T* emplaceLayer(Args&&... args) {
    auto layer = std::make_unique<T>(std::forward<Args>(args)...);
    T* raw = layer.get();
    owned_.push_back(std::move(layer));
    return raw;
  }

private:
  std::vector<std::unique_ptr<Layer>> owned_;
  Layer* rawBiomeLayer_ = nullptr;
  Layer* zoomedBiomeLayer_ = nullptr;
};

}  // namespace mc::gen
