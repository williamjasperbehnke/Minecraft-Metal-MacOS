#include "World/Level/Gen/BiomeLayers.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace mc::gen {

namespace {

constexpr std::int64_t kMul = 6364136223846793005LL;
constexpr std::int64_t kAdd = 1442695040888963407LL;

int chooseRandom4(Layer& layer, int a, int b, int c, int d) {
  const int pick = layer.nextRandom(4);
  if (pick == 0) {
    return a;
  }
  if (pick == 1) {
    return b;
  }
  if (pick == 2) {
    return c;
  }
  return d;
}

int chooseModeOrRandom(Layer& layer, int a, int b, int c, int d) {
  if (b == c && c == d) {
    return b;
  }
  if (a == b && a == c) {
    return a;
  }
  if (a == b && a == d) {
    return a;
  }
  if (a == c && a == d) {
    return a;
  }
  if (a == b && c != d) {
    return a;
  }
  if (a == c && b != d) {
    return a;
  }
  if (a == d && b != c) {
    return a;
  }
  if (b == c && a != d) {
    return b;
  }
  if (b == d && a != c) {
    return b;
  }
  if (c == d && a != b) {
    return c;
  }
  return chooseRandom4(layer, a, b, c, d);
}

class IslandLayer final : public Layer {
public:
  explicit IslandLayer(std::int64_t seedMixup) : Layer(seedMixup) {}

protected:
  std::vector<int> getArea(int x, int z, int w, int h) override {
    std::vector<int> out(static_cast<std::size_t>(w) * h, biomeid::Ocean);
    for (int dz = 0; dz < h; ++dz) {
      for (int dx = 0; dx < w; ++dx) {
        initRandom(x + dx, z + dz);
        // Slightly more initial land mass to reduce overall ocean prevalence.
        out[dx + dz * w] = (nextRandom(7) == 0) ? biomeid::Plains : biomeid::Ocean;
      }
    }
    if (x <= 0 && x + w > 0 && z <= 0 && z + h > 0) {
      out[-x + (-z) * w] = biomeid::Plains;
    }
    return out;
  }
};

class FuzzyZoomLayer final : public Layer {
public:
  explicit FuzzyZoomLayer(std::int64_t seedMixup) : Layer(seedMixup) {}

protected:
  std::vector<int> getArea(int x, int z, int w, int h) override {
    const int px = x >> 1;
    const int pz = z >> 1;
    const int pw = (w >> 1) + 2;
    const int ph = (h >> 1) + 2;
    const std::vector<int> parentVals = parent_->area(px, pz, pw, ph);
    const int rw = (pw - 1) * 2;
    const int rh = (ph - 1) * 2;
    std::vector<int> tmp(static_cast<std::size_t>(rw) * rh, biomeid::Ocean);

    for (int zz = 0; zz < ph - 1; ++zz) {
      for (int xx = 0; xx < pw - 1; ++xx) {
        const int a = parentVals[xx + zz * pw];
        const int b = parentVals[(xx + 1) + zz * pw];
        const int c = parentVals[xx + (zz + 1) * pw];
        const int d = parentVals[(xx + 1) + (zz + 1) * pw];
        initRandom((xx + px) << 1, (zz + pz) << 1);

        const int rx = xx * 2;
        const int rz = zz * 2;
        tmp[rx + rz * rw] = a;
        tmp[(rx + 1) + rz * rw] = (nextRandom(2) == 0) ? a : b;
        tmp[rx + (rz + 1) * rw] = (nextRandom(2) == 0) ? a : c;
        tmp[(rx + 1) + (rz + 1) * rw] = chooseRandom4(*this, a, b, c, d);
      }
    }

    std::vector<int> out(static_cast<std::size_t>(w) * h, biomeid::Ocean);
    const int ox = x & 1;
    const int oz = z & 1;
    for (int dz = 0; dz < h; ++dz) {
      for (int dx = 0; dx < w; ++dx) {
        out[dx + dz * w] = tmp[(dx + ox) + (dz + oz) * rw];
      }
    }
    return out;
  }
};

class ZoomLayer final : public Layer {
public:
  explicit ZoomLayer(std::int64_t seedMixup) : Layer(seedMixup) {}

protected:
  std::vector<int> getArea(int x, int z, int w, int h) override {
    const int px = x >> 1;
    const int pz = z >> 1;
    const int pw = (w >> 1) + 2;
    const int ph = (h >> 1) + 2;
    const std::vector<int> parentVals = parent_->area(px, pz, pw, ph);
    const int rw = (pw - 1) * 2;
    const int rh = (ph - 1) * 2;
    std::vector<int> tmp(static_cast<std::size_t>(rw) * rh, biomeid::Ocean);

    for (int zz = 0; zz < ph - 1; ++zz) {
      for (int xx = 0; xx < pw - 1; ++xx) {
        const int a = parentVals[xx + zz * pw];
        const int b = parentVals[(xx + 1) + zz * pw];
        const int c = parentVals[xx + (zz + 1) * pw];
        const int d = parentVals[(xx + 1) + (zz + 1) * pw];
        initRandom((xx + px) << 1, (zz + pz) << 1);

        const int rx = xx * 2;
        const int rz = zz * 2;
        tmp[rx + rz * rw] = a;
        tmp[(rx + 1) + rz * rw] = (nextRandom(2) == 0) ? a : b;
        tmp[rx + (rz + 1) * rw] = (nextRandom(2) == 0) ? a : c;
        tmp[(rx + 1) + (rz + 1) * rw] = chooseModeOrRandom(*this, a, b, c, d);
      }
    }

    std::vector<int> out(static_cast<std::size_t>(w) * h, biomeid::Ocean);
    const int ox = x & 1;
    const int oz = z & 1;
    for (int dz = 0; dz < h; ++dz) {
      for (int dx = 0; dx < w; ++dx) {
        out[dx + dz * w] = tmp[(dx + ox) + (dz + oz) * rw];
      }
    }
    return out;
  }
};

class AddIslandLayer final : public Layer {
public:
  explicit AddIslandLayer(std::int64_t seedMixup) : Layer(seedMixup) {}

protected:
  std::vector<int> getArea(int x, int z, int w, int h) override {
    const std::vector<int> p = parent_->area(x - 1, z - 1, w + 2, h + 2);
    std::vector<int> out(static_cast<std::size_t>(w) * h, biomeid::Ocean);

    for (int dz = 0; dz < h; ++dz) {
      for (int dx = 0; dx < w; ++dx) {
        const int nw = p[dx + (dz)* (w + 2)];
        const int ne = p[(dx + 2) + (dz)* (w + 2)];
        const int sw = p[dx + (dz + 2) * (w + 2)];
        const int se = p[(dx + 2) + (dz + 2) * (w + 2)];
        const int center = p[(dx + 1) + (dz + 1) * (w + 2)];
        initRandom(x + dx, z + dz);

        if (center == biomeid::Ocean && (nw != biomeid::Ocean || ne != biomeid::Ocean || sw != biomeid::Ocean || se != biomeid::Ocean)) {
          int chosen = biomeid::Plains;
          int count = 1;
          if (nw != biomeid::Ocean && nextRandom(count++) == 0) chosen = nw;
          if (ne != biomeid::Ocean && nextRandom(count++) == 0) chosen = ne;
          if (sw != biomeid::Ocean && nextRandom(count++) == 0) chosen = sw;
          if (se != biomeid::Ocean && nextRandom(count++) == 0) chosen = se;
          if (nextRandom(3) == 0) {
            out[dx + dz * w] = chosen;
          } else {
            out[dx + dz * w] = (chosen == biomeid::IcePlains) ? biomeid::FrozenOcean : biomeid::Ocean;
          }
        } else if (center != biomeid::Ocean &&
                   (nw == biomeid::Ocean || ne == biomeid::Ocean || sw == biomeid::Ocean || se == biomeid::Ocean)) {
          // Keep coastlines but avoid over-eroding land into ocean.
          if (nextRandom(9) == 0) {
            out[dx + dz * w] = (center == biomeid::IcePlains) ? biomeid::FrozenOcean : biomeid::Ocean;
          } else {
            out[dx + dz * w] = center;
          }
        } else {
          out[dx + dz * w] = center;
        }
      }
    }
    return out;
  }
};

class AddSnowLayer final : public Layer {
public:
  explicit AddSnowLayer(std::int64_t seedMixup) : Layer(seedMixup) {}

protected:
  std::vector<int> getArea(int x, int z, int w, int h) override {
    std::vector<int> p = parent_->area(x, z, w, h);
    std::vector<int> out(static_cast<std::size_t>(w) * h, biomeid::Ocean);
    for (int dz = 0; dz < h; ++dz) {
      for (int dx = 0; dx < w; ++dx) {
        const int v = p[dx + dz * w];
        if (v == biomeid::Ocean) {
          out[dx + dz * w] = biomeid::Ocean;
          continue;
        }
        initRandom(x + dx, z + dz);
        const int r = nextRandom(6);
        out[dx + dz * w] = (r == 0) ? 4 : ((r <= 2) ? 3 : 1);
      }
    }
    return out;
  }
};

class AddMushroomIslandLayer final : public Layer {
public:
  explicit AddMushroomIslandLayer(std::int64_t seedMixup) : Layer(seedMixup) {}

protected:
  std::vector<int> getArea(int x, int z, int w, int h) override {
    std::vector<int> p = parent_->area(x - 1, z - 1, w + 2, h + 2);
    std::vector<int> out(static_cast<std::size_t>(w) * h, biomeid::Ocean);
    for (int dz = 0; dz < h; ++dz) {
      for (int dx = 0; dx < w; ++dx) {
        const int n1 = p[(dx + 0) + (dz + 0) * (w + 2)];
        const int n2 = p[(dx + 2) + (dz + 0) * (w + 2)];
        const int n3 = p[(dx + 0) + (dz + 2) * (w + 2)];
        const int n4 = p[(dx + 2) + (dz + 2) * (w + 2)];
        const int c = p[(dx + 1) + (dz + 1) * (w + 2)];
        initRandom(x + dx, z + dz);
        if (c == biomeid::Ocean && (n1 == biomeid::Ocean && n2 == biomeid::Ocean && n3 == biomeid::Ocean && n4 == biomeid::Ocean) &&
            nextRandom(100) == 0) {
          out[dx + dz * w] = biomeid::MushroomIsland;
        } else {
          out[dx + dz * w] = c;
        }
      }
    }
    return out;
  }
};

class GrowMushroomIslandLayer final : public Layer {
public:
  explicit GrowMushroomIslandLayer(std::int64_t seedMixup) : Layer(seedMixup) {}

protected:
  std::vector<int> getArea(int x, int z, int w, int h) override {
    std::vector<int> p = parent_->area(x - 1, z - 1, w + 2, h + 2);
    std::vector<int> out(static_cast<std::size_t>(w) * h, biomeid::Ocean);
    for (int dz = 0; dz < h; ++dz) {
      for (int dx = 0; dx < w; ++dx) {
        const int n1 = p[(dx + 0) + (dz + 0) * (w + 2)];
        const int n2 = p[(dx + 2) + (dz + 0) * (w + 2)];
        const int n3 = p[(dx + 0) + (dz + 2) * (w + 2)];
        const int n4 = p[(dx + 2) + (dz + 2) * (w + 2)];
        const int c = p[(dx + 1) + (dz + 1) * (w + 2)];
        if (n1 == biomeid::MushroomIsland || n2 == biomeid::MushroomIsland || n3 == biomeid::MushroomIsland ||
            n4 == biomeid::MushroomIsland) {
          out[dx + dz * w] = biomeid::MushroomIsland;
        } else {
          out[dx + dz * w] = c;
        }
      }
    }
    return out;
  }
};

class BiomeInitLayer final : public Layer {
public:
  explicit BiomeInitLayer(std::int64_t seedMixup) : Layer(seedMixup) {}

protected:
  std::vector<int> getArea(int x, int z, int w, int h) override {
    std::vector<int> p = parent_->area(x, z, w, h);
    std::vector<int> out(static_cast<std::size_t>(w) * h, biomeid::Ocean);
    constexpr std::array<int, 8> kWarmBiomes = {
        biomeid::Desert, biomeid::Desert, biomeid::Plains, biomeid::Plains,
        biomeid::Forest, biomeid::Swampland, biomeid::Jungle, biomeid::ExtremeHills};
    constexpr std::array<int, 6> kCoolBiomes = {
        biomeid::Forest, biomeid::Forest, biomeid::Plains, biomeid::Taiga, biomeid::ExtremeHills, biomeid::Swampland};
    constexpr std::array<int, 4> kColdBiomes = {
        biomeid::Taiga, biomeid::Taiga, biomeid::Forest, biomeid::ExtremeHills};
    constexpr std::array<int, 3> kIceBiomes = {
        biomeid::IcePlains, biomeid::IcePlains, biomeid::Taiga};
    for (int dz = 0; dz < h; ++dz) {
      for (int dx = 0; dx < w; ++dx) {
        initRandom(x + dx, z + dz);
        const int old = p[dx + dz * w];
        int outBiome = old;
        if (old == biomeid::Ocean) {
          outBiome = biomeid::Ocean;
        } else if (old == biomeid::MushroomIsland) {
          outBiome = biomeid::MushroomIsland;
        } else if (old == 1) {
          outBiome = kWarmBiomes[static_cast<std::size_t>(nextRandom(static_cast<int>(kWarmBiomes.size())))];
        } else if (old == 2) {
          outBiome = kCoolBiomes[static_cast<std::size_t>(nextRandom(static_cast<int>(kCoolBiomes.size())))];
        } else if (old == 3) {
          outBiome = kColdBiomes[static_cast<std::size_t>(nextRandom(static_cast<int>(kColdBiomes.size())))];
        } else if (old == 4) {
          outBiome = kIceBiomes[static_cast<std::size_t>(nextRandom(static_cast<int>(kIceBiomes.size())))];
        } else {
          outBiome = kCoolBiomes[static_cast<std::size_t>(nextRandom(static_cast<int>(kCoolBiomes.size())))];
        }
        out[dx + dz * w] = outBiome;
      }
    }
    return out;
  }
};

class HillsLayer final : public Layer {
public:
  explicit HillsLayer(std::int64_t seedMixup) : Layer(seedMixup) {}

protected:
  std::vector<int> getArea(int x, int z, int w, int h) override {
    std::vector<int> p = parent_->area(x - 1, z - 1, w + 2, h + 2);
    std::vector<int> out(static_cast<std::size_t>(w) * h, biomeid::Ocean);
    for (int dz = 0; dz < h; ++dz) {
      for (int dx = 0; dx < w; ++dx) {
        initRandom(x + dx, z + dz);
        const int old = p[(dx + 1) + (dz + 1) * (w + 2)];
        int next = old;
        if (nextRandom(3) == 0) {
          if (old == biomeid::Desert) next = biomeid::DesertHills;
          else if (old == biomeid::Forest) next = biomeid::ForestHills;
          else if (old == biomeid::Taiga) next = biomeid::TaigaHills;
          else if (old == biomeid::Plains) next = biomeid::Forest;
          else if (old == biomeid::IcePlains) next = biomeid::ExtremeHillsEdge;
          else if (old == biomeid::Jungle) next = biomeid::JungleHills;
        }
        if (next != old) {
          const int n = p[(dx + 1) + (dz)* (w + 2)];
          const int e = p[(dx + 2) + (dz + 1) * (w + 2)];
          const int s = p[(dx + 1) + (dz + 2) * (w + 2)];
          const int wv = p[(dx) + (dz + 1) * (w + 2)];
          if (!(n == old && e == old && s == old && wv == old)) {
            next = old;
          }
        }
        out[dx + dz * w] = next;
      }
    }
    return out;
  }
};

class RiverInitLayer final : public Layer {
public:
  explicit RiverInitLayer(std::int64_t seedMixup) : Layer(seedMixup) {}

protected:
  std::vector<int> getArea(int x, int z, int w, int h) override {
    std::vector<int> p = parent_->area(x, z, w, h);
    std::vector<int> out(static_cast<std::size_t>(w) * h, biomeid::Ocean);
    for (int dz = 0; dz < h; ++dz) {
      for (int dx = 0; dx < w; ++dx) {
        initRandom(x + dx, z + dz);
        const int v = p[dx + dz * w];
        out[dx + dz * w] = (v > biomeid::Ocean) ? (2 + nextRandom(299999)) : biomeid::Ocean;
      }
    }
    return out;
  }
};

class RiverLayer final : public Layer {
public:
  explicit RiverLayer(std::int64_t seedMixup) : Layer(seedMixup) {}

protected:
  std::vector<int> getArea(int x, int z, int w, int h) override {
    std::vector<int> p = parent_->area(x - 1, z - 1, w + 2, h + 2);
    std::vector<int> out(static_cast<std::size_t>(w) * h, biomeid::Ocean);
    auto riverFilter = [](int v) { return (v >= 2) ? (2 + (v & 1)) : v; };
    for (int dz = 0; dz < h; ++dz) {
      for (int dx = 0; dx < w; ++dx) {
        const int c = riverFilter(p[(dx + 1) + (dz + 1) * (w + 2)]);
        const int n = riverFilter(p[(dx + 1) + (dz)* (w + 2)]);
        const int e = riverFilter(p[(dx + 2) + (dz + 1) * (w + 2)]);
        const int s = riverFilter(p[(dx + 1) + (dz + 2) * (w + 2)]);
        const int wv = riverFilter(p[(dx) + (dz + 1) * (w + 2)]);
        out[dx + dz * w] = (c == n && c == e && c == s && c == wv) ? biomeid::Ocean : biomeid::River;
      }
    }
    return out;
  }
};

class SwampRiversLayer final : public Layer {
public:
  explicit SwampRiversLayer(std::int64_t seedMixup) : Layer(seedMixup) {}

protected:
  std::vector<int> getArea(int x, int z, int w, int h) override {
    std::vector<int> p = parent_->area(x - 1, z - 1, w + 2, h + 2);
    std::vector<int> out(static_cast<std::size_t>(w) * h, biomeid::Ocean);
    for (int dz = 0; dz < h; ++dz) {
      for (int dx = 0; dx < w; ++dx) {
        initRandom(x + dx, z + dz);
        const int old = p[(dx + 1) + (dz + 1) * (w + 2)];
        if ((old == biomeid::Swampland && nextRandom(6) == 0) ||
            ((old == biomeid::Jungle || old == biomeid::JungleHills) && nextRandom(8) == 0)) {
          out[dx + dz * w] = biomeid::River;
        } else {
          out[dx + dz * w] = old;
        }
      }
    }
    return out;
  }
};

class SmoothLayer final : public Layer {
public:
  explicit SmoothLayer(std::int64_t seedMixup) : Layer(seedMixup) {}

protected:
  std::vector<int> getArea(int x, int z, int w, int h) override {
    std::vector<int> p = parent_->area(x - 1, z - 1, w + 2, h + 2);
    std::vector<int> out(static_cast<std::size_t>(w) * h, biomeid::Ocean);
    for (int dz = 0; dz < h; ++dz) {
      for (int dx = 0; dx < w; ++dx) {
        const int c = p[(dx + 1) + (dz + 1) * (w + 2)];
        const int n = p[(dx + 1) + (dz)* (w + 2)];
        const int e = p[(dx + 2) + (dz + 1) * (w + 2)];
        const int s = p[(dx + 1) + (dz + 2) * (w + 2)];
        const int wv = p[(dx) + (dz + 1) * (w + 2)];
        int outVal = c;
        if (n == s && wv == e) {
          initRandom(x + dx, z + dz);
          outVal = (nextRandom(2) == 0) ? n : wv;
        } else {
          if (n == s) outVal = n;
          if (wv == e) outVal = wv;
        }
        out[dx + dz * w] = outVal;
      }
    }
    return out;
  }
};

class ShoreLayer final : public Layer {
public:
  explicit ShoreLayer(std::int64_t seedMixup) : Layer(seedMixup) {}

protected:
  std::vector<int> getArea(int x, int z, int w, int h) override {
    std::vector<int> p = parent_->area(x - 1, z - 1, w + 2, h + 2);
    std::vector<int> out(static_cast<std::size_t>(w) * h, biomeid::Ocean);
    for (int dz = 0; dz < h; ++dz) {
      for (int dx = 0; dx < w; ++dx) {
        const int c = p[(dx + 1) + (dz + 1) * (w + 2)];
        if (c == biomeid::MushroomIsland) {
          const int n = p[(dx + 1) + (dz)* (w + 2)];
          const int e = p[(dx + 2) + (dz + 1) * (w + 2)];
          const int s = p[(dx + 1) + (dz + 2) * (w + 2)];
          const int wv = p[(dx) + (dz + 1) * (w + 2)];
          if (n == biomeid::Ocean || e == biomeid::Ocean || s == biomeid::Ocean || wv == biomeid::Ocean) {
            out[dx + dz * w] = biomeid::MushroomIslandShore;
          } else {
            out[dx + dz * w] = c;
          }
          continue;
        }
        if (c == biomeid::Ocean || c == biomeid::River || c == biomeid::Swampland) {
          out[dx + dz * w] = c;
          continue;
        }
        const int n = p[(dx + 1) + (dz)* (w + 2)];
        const int e = p[(dx + 2) + (dz + 1) * (w + 2)];
        const int s = p[(dx + 1) + (dz + 2) * (w + 2)];
        const int wv = p[(dx) + (dz + 1) * (w + 2)];
        if (c == biomeid::ExtremeHills) {
          const bool isEdge =
              (n != biomeid::ExtremeHills || e != biomeid::ExtremeHills || s != biomeid::ExtremeHills || wv != biomeid::ExtremeHills);
          out[dx + dz * w] = isEdge ? biomeid::ExtremeHillsEdge : c;
        } else {
          const bool edgeToOcean = (n == biomeid::Ocean || e == biomeid::Ocean || s == biomeid::Ocean || wv == biomeid::Ocean);
          out[dx + dz * w] = edgeToOcean ? biomeid::Beach : c;
        }
      }
    }
    return out;
  }
};

class RiverMixerLayer final : public Layer {
public:
  explicit RiverMixerLayer(std::int64_t seedMixup, Layer* biomeParent, Layer* riverParent) : Layer(seedMixup), riverParent_(riverParent) {
    setParent(biomeParent);
  }

  void initSeed(std::int64_t seed) override {
    Layer::initSeed(seed);
    if (riverParent_) {
      riverParent_->initSeed(seed);
    }
  }

protected:
  std::vector<int> getArea(int x, int z, int w, int h) override {
    const std::vector<int> biome = parent_->area(x, z, w, h);
    const std::vector<int> river = riverParent_->area(x, z, w, h);
    std::vector<int> out(static_cast<std::size_t>(w) * h, biomeid::Ocean);
    for (int i = 0; i < w * h; ++i) {
      const int b = biome[i];
      const int r = river[i];
      if (b == biomeid::Ocean) {
        out[i] = b;
      } else if (b == biomeid::MushroomIsland || b == biomeid::MushroomIslandShore) {
        out[i] = biomeid::MushroomIsland;
      } else if (r == biomeid::River) {
        out[i] = (b == biomeid::IcePlains) ? biomeid::FrozenRiver : biomeid::River;
      } else {
        out[i] = b;
      }
    }
    return out;
  }

private:
  Layer* riverParent_ = nullptr;
};

class VoronoiZoomLayer final : public Layer {
public:
  explicit VoronoiZoomLayer(std::int64_t seedMixup) : Layer(seedMixup) {}

protected:
  std::vector<int> getArea(int x, int z, int w, int h) override {
    x -= 2;
    z -= 2;
    constexpr int bits = 2;
    constexpr int ss = 1 << bits;
    const int px = x >> bits;
    const int pz = z >> bits;
    const int pw = (w >> bits) + 3;
    const int ph = (h >> bits) + 3;
    const std::vector<int> p = parent_->area(px, pz, pw, ph);

    const int ww = pw << bits;
    const int hh = ph << bits;
    std::vector<int> tmp(static_cast<std::size_t>(ww) * hh, biomeid::Ocean);

    for (int yy = 0; yy < ph - 1; ++yy) {
      int ul = p[(0 + 0) + (yy + 0) * pw];
      int dl = p[(0 + 0) + (yy + 1) * pw];
      for (int xx = 0; xx < pw - 1; ++xx) {
        const double s = ss * 0.9;
        initRandom((xx + px) << bits, (yy + pz) << bits);
        const double x0 = (nextRandom(1024) / 1024.0 - 0.5) * s;
        const double y0 = (nextRandom(1024) / 1024.0 - 0.5) * s;
        initRandom((xx + px + 1) << bits, (yy + pz) << bits);
        const double x1 = (nextRandom(1024) / 1024.0 - 0.5) * s + ss;
        const double y1 = (nextRandom(1024) / 1024.0 - 0.5) * s;
        initRandom((xx + px) << bits, (yy + pz + 1) << bits);
        const double x2 = (nextRandom(1024) / 1024.0 - 0.5) * s;
        const double y2 = (nextRandom(1024) / 1024.0 - 0.5) * s + ss;
        initRandom((xx + px + 1) << bits, (yy + pz + 1) << bits);
        const double x3 = (nextRandom(1024) / 1024.0 - 0.5) * s + ss;
        const double y3 = (nextRandom(1024) / 1024.0 - 0.5) * s + ss;

        const int ur = p[(xx + 1) + (yy + 0) * pw];
        const int dr = p[(xx + 1) + (yy + 1) * pw];
        for (int sy = 0; sy < ss; ++sy) {
          int pp = ((yy << bits) + sy) * ww + (xx << bits);
          for (int sx = 0; sx < ss; ++sx) {
            const double d0 = ((sy - y0) * (sy - y0) + (sx - x0) * (sx - x0));
            const double d1 = ((sy - y1) * (sy - y1) + (sx - x1) * (sx - x1));
            const double d2 = ((sy - y2) * (sy - y2) + (sx - x2) * (sx - x2));
            const double d3 = ((sy - y3) * (sy - y3) + (sx - x3) * (sx - x3));
            if (d0 < d1 && d0 < d2 && d0 < d3) {
              tmp[pp++] = ul;
            } else if (d1 < d0 && d1 < d2 && d1 < d3) {
              tmp[pp++] = ur;
            } else if (d2 < d0 && d2 < d1 && d2 < d3) {
              tmp[pp++] = dl;
            } else {
              tmp[pp++] = dr;
            }
          }
        }

        ul = ur;
        dl = dr;
      }
    }

    std::vector<int> out(static_cast<std::size_t>(w) * h, biomeid::Ocean);
    const int ox = x & (ss - 1);
    const int oz = z & (ss - 1);
    for (int dz = 0; dz < h; ++dz) {
      for (int dx = 0; dx < w; ++dx) {
        out[dx + dz * w] = tmp[(dx + ox) + (dz + oz) * ww];
      }
    }
    return out;
  }
};

Layer* applyZoom(LayerStack& stack, Layer* input, std::int64_t baseSeed, int count) {
  Layer* current = input;
  for (int i = 0; i < count; ++i) {
    auto* zoom = stack.emplaceLayer<ZoomLayer>(baseSeed + i);
    zoom->setParent(current);
    current = zoom;
  }
  return current;
}

}  // namespace

Layer::Layer(std::int64_t seedMixup) {
  seedMixup_ = seedMixup;
  seedMixup_ = seedMixup_ * (seedMixup_ * kMul + kAdd) + seedMixup;
  seedMixup_ = seedMixup_ * (seedMixup_ * kMul + kAdd) + seedMixup;
  seedMixup_ = seedMixup_ * (seedMixup_ * kMul + kAdd) + seedMixup;
}

void Layer::setParent(Layer* parent) {
  parent_ = parent;
}

void Layer::initSeed(std::int64_t seed) {
  seed_ = seed;
  if (parent_) {
    parent_->initSeed(seed);
  }
  seed_ = seed_ * (seed_ * kMul + kAdd) + seedMixup_;
  seed_ = seed_ * (seed_ * kMul + kAdd) + seedMixup_;
  seed_ = seed_ * (seed_ * kMul + kAdd) + seedMixup_;
}

std::vector<int> Layer::area(int x, int z, int w, int h) {
  return getArea(x, z, w, h);
}

void Layer::initRandom(std::int64_t x, std::int64_t z) {
  rval_ = seed_;
  rval_ = rval_ * (rval_ * kMul + kAdd) + x;
  rval_ = rval_ * (rval_ * kMul + kAdd) + z;
  rval_ = rval_ * (rval_ * kMul + kAdd) + x;
  rval_ = rval_ * (rval_ * kMul + kAdd) + z;
}

int Layer::nextRandom(int max) {
  if (max <= 1) {
    return 0;
  }
  int value = static_cast<int>((rval_ >> 24) % max);
  if (value < 0) {
    value += max;
  }
  rval_ = rval_ * (rval_ * kMul + kAdd) + seed_;
  return value;
}

LayerStack::LayerStack(std::uint32_t seed) {
  Layer* island = emplaceLayer<IslandLayer>(1);
  Layer* fuzzy = emplaceLayer<FuzzyZoomLayer>(2000);
  fuzzy->setParent(island);
  Layer* add1 = emplaceLayer<AddIslandLayer>(1);
  add1->setParent(fuzzy);
  Layer* z1 = emplaceLayer<ZoomLayer>(2001);
  z1->setParent(add1);
  Layer* add2 = emplaceLayer<AddIslandLayer>(2);
  add2->setParent(z1);
  Layer* snow = emplaceLayer<AddSnowLayer>(2);
  snow->setParent(add2);
  Layer* z2 = emplaceLayer<ZoomLayer>(2002);
  z2->setParent(snow);
  Layer* add3 = emplaceLayer<AddIslandLayer>(3);
  add3->setParent(z2);
  Layer* z3 = emplaceLayer<ZoomLayer>(2003);
  z3->setParent(add3);
  Layer* add4 = emplaceLayer<AddIslandLayer>(4);
  add4->setParent(z3);
  Layer* islandRoot = add4;

  const int zoomLevel = 4;

  Layer* river = applyZoom(*this, islandRoot, 1000, 0);
  Layer* riverInit = emplaceLayer<RiverInitLayer>(100);
  riverInit->setParent(river);
  river = applyZoom(*this, riverInit, 1000, zoomLevel + 2);
  Layer* riverLayer = emplaceLayer<RiverLayer>(1);
  riverLayer->setParent(river);
  Layer* riverSmooth = emplaceLayer<SmoothLayer>(1000);
  riverSmooth->setParent(riverLayer);

  Layer* biome = applyZoom(*this, islandRoot, 1000, 0);
  Layer* biomeInit = emplaceLayer<BiomeInitLayer>(200);
  biomeInit->setParent(biome);
  biome = applyZoom(*this, biomeInit, 1000, 2);
  Layer* hills = emplaceLayer<HillsLayer>(1000);
  hills->setParent(biome);
  biome = hills;

  for (int i = 0; i < zoomLevel; ++i) {
    Layer* zoom = emplaceLayer<ZoomLayer>(1000 + i);
    zoom->setParent(biome);
    biome = zoom;
    if (i == 0) {
      Layer* add = emplaceLayer<AddIslandLayer>(3);
      add->setParent(biome);
      biome = add;
      // PS3 layer order: mushroom islands are injected at the first zoom stage.
      Layer* addMush = emplaceLayer<AddMushroomIslandLayer>(5);
      addMush->setParent(biome);
      biome = addMush;
    }
    if (i == 1) {
      Layer* growMush = emplaceLayer<GrowMushroomIslandLayer>(5);
      growMush->setParent(biome);
      biome = growMush;
      Layer* shore = emplaceLayer<ShoreLayer>(1000);
      shore->setParent(biome);
      biome = shore;
      Layer* swampRivers = emplaceLayer<SwampRiversLayer>(1000);
      swampRivers->setParent(biome);
      biome = swampRivers;
    }
  }

  Layer* biomeSmooth = emplaceLayer<SmoothLayer>(1000);
  biomeSmooth->setParent(biome);
  auto* mixer = emplaceLayer<RiverMixerLayer>(100, biomeSmooth, riverSmooth);
  rawBiomeLayer_ = mixer;
  auto* voronoi = emplaceLayer<VoronoiZoomLayer>(10);
  voronoi->setParent(rawBiomeLayer_);
  zoomedBiomeLayer_ = voronoi;
  zoomedBiomeLayer_->initSeed(static_cast<std::int64_t>(seed));
}

std::vector<int> LayerStack::sampleRawBiomeIds(int x, int z, int w, int h) const {
  if (!rawBiomeLayer_) {
    return std::vector<int>(static_cast<std::size_t>(w) * h, biomeid::Plains);
  }
  return const_cast<Layer*>(rawBiomeLayer_)->area(x, z, w, h);
}

std::vector<int> LayerStack::sampleZoomedBiomeIds(int x, int z, int w, int h) const {
  if (!zoomedBiomeLayer_) {
    return std::vector<int>(static_cast<std::size_t>(w) * h, biomeid::Plains);
  }
  return const_cast<Layer*>(zoomedBiomeLayer_)->area(x, z, w, h);
}

}  // namespace mc::gen
