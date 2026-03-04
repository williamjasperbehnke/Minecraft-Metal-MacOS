#include "World/Level/Gen/Decorators.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <random>
#include <unordered_map>
#include <utility>
#include <vector>

#include "World/Chunk/LevelChunk.h"
#include "World/Tile/Tile.h"

namespace mc::gen {

namespace {

constexpr int kChunkSize = 16;
constexpr double kPi = 3.14159265358979323846;

inline int blockIndex(int x, int y, int z, int chunkHeight) {
  return (z * kChunkSize + x) * chunkHeight + y;
}

int nextInt(std::mt19937_64& rng, int bound) {
  if (bound <= 1) {
    return 0;
  }
  std::uniform_int_distribution<int> dist(0, bound - 1);
  return dist(rng);
}

double nextFloat01(std::mt19937_64& rng) {
  std::uniform_real_distribution<double> dist(0.0, 1.0);
  return dist(rng);
}

std::int64_t nextLong(std::mt19937_64& rng) {
  return static_cast<std::int64_t>(rng());
}

std::uint64_t ps3ChunkSeed(std::uint32_t worldSeed, int chunkX, int chunkZ) {
  std::mt19937_64 worldRng(static_cast<std::uint64_t>(worldSeed));
  const std::int64_t xScale = nextLong(worldRng) / 2 * 2 + 1;
  const std::int64_t zScale = nextLong(worldRng) / 2 * 2 + 1;
  return static_cast<std::uint64_t>((static_cast<std::int64_t>(chunkX) * xScale + static_cast<std::int64_t>(chunkZ) * zScale) ^
                                    static_cast<std::int64_t>(worldSeed));
}

std::int64_t chunkKey(int chunkX, int chunkZ) {
  return (static_cast<std::int64_t>(chunkX) << 32) ^ static_cast<std::uint32_t>(chunkZ);
}

bool isPlantTile(std::uint8_t t) {
  switch (static_cast<TileId>(t)) {
    case TileId::TallGrass:
    case TileId::Fern:
    case TileId::DeadBush:
    case TileId::FlowerYellow:
    case TileId::FlowerRed:
    case TileId::MushroomBrown:
    case TileId::MushroomRed:
    case TileId::SugarCane:
      return true;
    default:
      return false;
  }
}

bool isLeafTile(std::uint8_t t) {
  switch (static_cast<TileId>(t)) {
    case TileId::Leaves:
    case TileId::SpruceLeaves:
    case TileId::BirchLeaves:
      return true;
    default:
      return false;
  }
}

bool canReplaceDeferred(int tile) {
  const auto t = static_cast<TileId>(tile);
  switch (t) {
    case TileId::Air:
    case TileId::TallGrass:
    case TileId::Fern:
    case TileId::DeadBush:
    case TileId::FlowerYellow:
    case TileId::FlowerRed:
    case TileId::MushroomBrown:
    case TileId::MushroomRed:
    case TileId::SugarCane:
      return true;
    default:
      return false;
  }
}

struct DeferredBlockWrite {
  int x = 0;
  int y = 0;
  int z = 0;
  std::uint8_t tile = 0;
};

class DeferredWriteStore {
public:
  void queueWrite(int chunkX, int chunkZ, int x, int y, int z, std::uint8_t tile) {
    std::lock_guard<std::mutex> lock(mutex_);
    writes_[chunkKey(chunkX, chunkZ)].push_back({x, y, z, tile});
  }

  void applyToChunk(LevelChunk& chunk) {
    const std::int64_t key = chunkKey(chunk.chunkX(), chunk.chunkZ());
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = writes_.find(key);
    if (it == writes_.end()) {
      return;
    }
    for (const DeferredBlockWrite& w : it->second) {
      if (w.x < 0 || w.x >= kChunkSize || w.z < 0 || w.z >= kChunkSize || w.y < 1 || w.y >= LevelChunk::kSizeY) {
        continue;
      }
      if (canReplaceDeferred(chunk.getTile(w.x, w.y, w.z))) {
        chunk.setTile(w.x, w.y, w.z, w.tile);
      }
    }
    writes_.erase(it);
  }

  template <typename CanReplaceFn, typename SetTileFn>
  void applyToView(int chunkX, int chunkZ, int chunkHeight, CanReplaceFn&& canReplace, SetTileFn&& setTile) {
    const std::int64_t key = chunkKey(chunkX, chunkZ);
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = writes_.find(key);
    if (it == writes_.end()) {
      return;
    }
    for (const DeferredBlockWrite& w : it->second) {
      if (w.x < 0 || w.x >= kChunkSize || w.z < 0 || w.z >= kChunkSize || w.y < 1 || w.y >= chunkHeight) {
        continue;
      }
      if (canReplace(w.x, w.y, w.z)) {
        setTile(w.x, w.y, w.z, w.tile);
      }
    }
    writes_.erase(it);
  }

private:
  std::unordered_map<std::int64_t, std::vector<DeferredBlockWrite>> writes_;
  std::mutex mutex_;
};

DeferredWriteStore gDeferredWrites;

class ChunkDecorationContext {
public:
  explicit ChunkDecorationContext(const ChunkDecorationView& view) : view_(view) {
    gDeferredWrites.applyToView(
        view_.chunkX, view_.chunkZ, view_.chunkHeight,
        [this](int x, int y, int z) { return canReplaceAt(x, y, z); },
        [this](int x, int y, int z, std::uint8_t tile) { setLocalTile(x, y, z, tile); });
  }

  int topSolidY(int x, int z) const {
    for (int y = view_.chunkHeight - 1; y >= 0; --y) {
      const std::uint8_t t = tile(x, y, z);
      if (t != static_cast<std::uint8_t>(TileId::Air) && t != static_cast<std::uint8_t>(TileId::Water) && !isLeafTile(t) &&
          !isPlantTile(t)) {
        return y;
      }
    }
    return -1;
  }

  bool canReplaceAt(int x, int y, int z) const {
    const std::uint8_t t = tile(x, y, z);
    return t == static_cast<std::uint8_t>(TileId::Air) || isPlantTile(t);
  }

  std::uint8_t tile(int x, int y, int z) const {
    return view_.blocks[blockIndex(x, y, z, view_.chunkHeight)];
  }

  void setLocalTile(int x, int y, int z, std::uint8_t tile) {
    view_.blocks[blockIndex(x, y, z, view_.chunkHeight)] = tile;
  }

  void placeCrossChunk(int lx, int y, int lz, std::uint8_t tile) {
    if (y < 1 || y >= view_.chunkHeight) {
      return;
    }

    int targetChunkX = view_.chunkX;
    int targetChunkZ = view_.chunkZ;
    int tx = lx;
    int tz = lz;
    while (tx < 0) {
      tx += kChunkSize;
      --targetChunkX;
    }
    while (tx >= kChunkSize) {
      tx -= kChunkSize;
      ++targetChunkX;
    }
    while (tz < 0) {
      tz += kChunkSize;
      --targetChunkZ;
    }
    while (tz >= kChunkSize) {
      tz -= kChunkSize;
      ++targetChunkZ;
    }

    if (targetChunkX == view_.chunkX && targetChunkZ == view_.chunkZ) {
      if (canReplaceAt(tx, y, tz)) {
        setLocalTile(tx, y, tz, tile);
      }
      return;
    }

    gDeferredWrites.queueWrite(targetChunkX, targetChunkZ, tx, y, tz, tile);
  }

  const ChunkDecorationView& view() const { return view_; }

private:
  const ChunkDecorationView& view_;
};

struct BiomeColumnStats {
  int forest = 0;
  int taiga = 0;
  int cold = 0;
  int plains = 0;
  int mountains = 0;
  int desert = 0;
  int ocean = 0;
};

BiomeColumnStats computeBiomeStats(const std::vector<BiomeSample>& biomes) {
  BiomeColumnStats out;
  for (const BiomeSample& b : biomes) {
    switch (b.kind) {
      case BiomeKind::Forest: ++out.forest; break;
      case BiomeKind::Taiga: ++out.taiga; break;
      case BiomeKind::Plains: ++out.plains; break;
      case BiomeKind::Mountains: ++out.mountains; break;
      case BiomeKind::Desert: ++out.desert; break;
      case BiomeKind::Ocean: ++out.ocean; break;
    }
    if (b.temperature < 0.30) {
      ++out.cold;
    }
  }
  return out;
}

struct VegetationBudget {
  int tree = 0;
  int cactus = 0;
  int tallGrass = 0;
  int fern = 0;
  int deadBush = 0;
  int flower = 0;
  int mushroom = 0;
  int sugarCane = 0;
};

VegetationBudget computeVegetationBudget(const BiomeColumnStats& stats) {
  VegetationBudget out;
  out.tree = (stats.forest * 7 + stats.taiga * 6 + stats.cold * 2 + stats.plains * 1 + stats.mountains * 2) / 256;
  out.cactus = (stats.desert * 3) / 256;
  out.tallGrass = (stats.forest * 8 + stats.plains * 14 + stats.mountains * 2 + stats.taiga * 2) / 256;
  out.fern = (stats.taiga * 7 + stats.cold * 4 + stats.mountains * 2) / 256;
  out.deadBush = (stats.desert * 5) / 256;
  out.flower = (stats.forest * 4 + stats.plains * 7) / 256;
  out.mushroom = (stats.forest * 2 + stats.taiga * 3 + stats.cold * 2) / 256;
  out.sugarCane = (stats.forest * 2 + stats.plains * 2 + stats.ocean * 1) / 256;

  out.tree = std::max(0, out.tree);
  out.cactus = std::max(0, out.cactus);
  out.tallGrass = std::max(0, out.tallGrass);
  out.fern = std::max(0, out.fern);
  out.deadBush = std::max(0, out.deadBush);
  out.flower = std::max(0, out.flower);
  out.mushroom = std::max(0, out.mushroom);
  out.sugarCane = std::max(0, out.sugarCane);
  return out;
}

}  // namespace

void applyDeferredDecorationWrites(LevelChunk& chunk) {
  gDeferredWrites.applyToChunk(chunk);
}

OreDecorator::OreDecorator(std::uint32_t seed) : seed_(seed) {}

void OreDecorator::decorate(const ChunkDecorationView& view) const {
  if (!view.blocks || view.chunkHeight <= 0) {
    return;
  }

  auto oreVein = [&](std::mt19937_64& rng, std::uint8_t oreTile, int veinSize, int yMin, int yMax) {
    if (yMax <= yMin) {
      return;
    }

    const double angle = nextFloat01(rng) * kPi;
    const double x0 = nextInt(rng, 16) + std::sin(angle) * veinSize / 8.0;
    const double x1 = nextInt(rng, 16) - std::sin(angle) * veinSize / 8.0;
    const double z0 = nextInt(rng, 16) + std::cos(angle) * veinSize / 8.0;
    const double z1 = nextInt(rng, 16) - std::cos(angle) * veinSize / 8.0;
    const double y0 = nextInt(rng, yMax - yMin) + yMin;
    const double y1 = nextInt(rng, yMax - yMin) + yMin;

    for (int i = 0; i <= veinSize; ++i) {
      const double t = static_cast<double>(i) / static_cast<double>(veinSize);
      const double cx = x0 + (x1 - x0) * t;
      const double cy = y0 + (y1 - y0) * t;
      const double cz = z0 + (z1 - z0) * t;
      const double rr = ((std::sin(t * kPi) + 1.0) * nextFloat01(rng) + 1.0) / 2.0;

      const int ix0 = std::max(0, static_cast<int>(std::floor(cx - rr)));
      const int ix1 = std::min(15, static_cast<int>(std::floor(cx + rr)));
      const int iy0 = std::max(1, static_cast<int>(std::floor(cy - rr)));
      const int iy1 = std::min(view.chunkHeight - 2, static_cast<int>(std::floor(cy + rr)));
      const int iz0 = std::max(0, static_cast<int>(std::floor(cz - rr)));
      const int iz1 = std::min(15, static_cast<int>(std::floor(cz + rr)));

      for (int x = ix0; x <= ix1; ++x) {
        const double nx = (x + 0.5 - cx) / rr;
        const double nx2 = nx * nx;
        if (nx2 >= 1.0) {
          continue;
        }
        for (int y = iy0; y <= iy1; ++y) {
          const double ny = (y + 0.5 - cy) / rr;
          const double nxy = nx2 + ny * ny;
          if (nxy >= 1.0) {
            continue;
          }
          for (int z = iz0; z <= iz1; ++z) {
            const double nz = (z + 0.5 - cz) / rr;
            if (nxy + nz * nz >= 1.0) {
              continue;
            }
            const int idx = blockIndex(x, y, z, view.chunkHeight);
            if (view.blocks[idx] == static_cast<std::uint8_t>(TileId::Stone)) {
              view.blocks[idx] = oreTile;
            }
          }
        }
      }
    }
  };

  std::mt19937_64 rng(ps3ChunkSeed(seed_, view.chunkX, view.chunkZ));
  for (int i = 0; i < 20; ++i) {
    oreVein(rng, static_cast<std::uint8_t>(TileId::CoalOre), 16, 0, view.chunkHeight);
  }
  for (int i = 0; i < 20; ++i) {
    oreVein(rng, static_cast<std::uint8_t>(TileId::IronOre), 8, 0, view.chunkHeight / 2);
  }
  for (int i = 0; i < 2; ++i) {
    oreVein(rng, static_cast<std::uint8_t>(TileId::GoldOre), 8, 0, view.chunkHeight / 4);
  }
  for (int i = 0; i < 1; ++i) {
    oreVein(rng, static_cast<std::uint8_t>(TileId::DiamondOre), 7, 0, view.chunkHeight / 8);
  }
}

VegetationDecorator::VegetationDecorator(std::uint32_t seed) : seed_(seed) {}

void VegetationDecorator::decorate(const ChunkDecorationView& view) const {
  if (!view.blocks || view.chunkHeight <= 0 || !view.biomes || view.biomes->size() < 16 * 16) {
    return;
  }

  ChunkDecorationContext ctx(view);
  std::mt19937_64 rng(ps3ChunkSeed(seed_, view.chunkX, view.chunkZ));
  const BiomeColumnStats stats = computeBiomeStats(*view.biomes);
  const VegetationBudget budget = computeVegetationBudget(stats);

  int treeAttempts = budget.tree;
  if (nextInt(rng, 10) == 0) {
    ++treeAttempts;
  }

  struct TreePos {
    int x = 0;
    int z = 0;
  };
  std::vector<TreePos> placedTrees;

  auto placeBroadleafTree = [&](int x, int y, int z, std::uint8_t woodTile, std::uint8_t leafTile) {
    const int trunk = 4 + nextInt(rng, 2);
    if (y + trunk + 5 >= view.chunkHeight) {
      return;
    }
    for (int yy = 1; yy <= trunk; ++yy) {
      ctx.setLocalTile(x, y + yy, z, woodTile);
    }

    const int canopyBase = y + trunk - 2;
    for (int layer = 0; layer < 5; ++layer) {
      const int ay = canopyBase + layer;
      int radius = 2;
      if (layer == 0 || layer == 4) {
        radius = 1;
      }
      if (layer == 2 && nextInt(rng, 5) == 0) {
        radius = 2;
      }
      for (int ox = -radius; ox <= radius; ++ox) {
        for (int oz = -radius; oz <= radius; ++oz) {
          const int dist2 = ox * ox + oz * oz;
          const int limit = radius * radius;
          if (dist2 > limit || (dist2 == limit && nextInt(rng, 2) == 0)) {
            continue;
          }
          ctx.placeCrossChunk(x + ox, ay, z + oz, leafTile);
        }
      }
    }
  };

  auto placeSpruceTree = [&](int x, int y, int z) {
    const int trunk = 6 + nextInt(rng, 4);
    if (y + trunk + 3 >= view.chunkHeight) {
      return;
    }
    for (int yy = 1; yy < trunk; ++yy) {
      ctx.setLocalTile(x, y + yy, z, static_cast<std::uint8_t>(TileId::SpruceWood));
    }

    int radius = 0;
    int maxRadius = 2 + nextInt(rng, 2);
    for (int ay = y + trunk; ay >= y + trunk - 5; --ay) {
      if (ay < 1 || ay >= view.chunkHeight) {
        continue;
      }
      if (radius < maxRadius && ay <= y + trunk - 1) {
        ++radius;
      } else if (radius > 1 && nextInt(rng, 2) == 0) {
        --radius;
      }
      for (int ox = -radius; ox <= radius; ++ox) {
        for (int oz = -radius; oz <= radius; ++oz) {
          if ((ox * ox + oz * oz) > radius * radius + 1) {
            continue;
          }
          ctx.placeCrossChunk(x + ox, ay, z + oz, static_cast<std::uint8_t>(TileId::SpruceLeaves));
        }
      }
    }

    const int tipY = y + trunk + 1;
    if (tipY >= 1 && tipY < view.chunkHeight && ctx.canReplaceAt(x, tipY, z)) {
      ctx.setLocalTile(x, tipY, z, static_cast<std::uint8_t>(TileId::SpruceLeaves));
    }
  };

  for (int i = 0; i < treeAttempts; ++i) {
    const int x = nextInt(rng, 16);
    const int z = nextInt(rng, 16);
    const int y = ctx.topSolidY(x, z);
    if (y < 1 || y + 8 >= view.chunkHeight) {
      continue;
    }

    const std::uint8_t ground = ctx.tile(x, y, z);
    if (ground != static_cast<std::uint8_t>(TileId::Grass) && ground != static_cast<std::uint8_t>(TileId::Snow)) {
      continue;
    }
    if (!ctx.canReplaceAt(x, y + 1, z)) {
      continue;
    }

    bool tooClose = false;
    for (const TreePos& tp : placedTrees) {
      const int dx = x - tp.x;
      const int dz = z - tp.z;
      if (dx * dx + dz * dz < 16) {
        tooClose = true;
        break;
      }
    }
    if (tooClose) {
      continue;
    }

    const BiomeSample& b = (*view.biomes)[x + z * 16];
    if (b.kind == BiomeKind::Taiga || b.temperature < 0.22) {
      placeSpruceTree(x, y, z);
    } else {
      const bool birch = (b.kind == BiomeKind::Forest && nextInt(rng, 100) < 35);
      placeBroadleafTree(x, y, z,
                         birch ? static_cast<std::uint8_t>(TileId::BirchWood) : static_cast<std::uint8_t>(TileId::Wood),
                         birch ? static_cast<std::uint8_t>(TileId::BirchLeaves) : static_cast<std::uint8_t>(TileId::Leaves));
    }
    placedTrees.push_back({x, z});
  }

  for (int i = 0; i < budget.cactus; ++i) {
    const int x = nextInt(rng, 16);
    const int z = nextInt(rng, 16);
    const int y = ctx.topSolidY(x, z);
    if (y < 1 || y + 4 >= view.chunkHeight) {
      continue;
    }
    if (ctx.tile(x, y, z) != static_cast<std::uint8_t>(TileId::Sand)) {
      continue;
    }
    if (!ctx.canReplaceAt(x, y + 1, z)) {
      continue;
    }
    if (x <= 0 || x >= 15 || z <= 0 || z >= 15) {
      continue;
    }
    if (ctx.tile(x - 1, y + 1, z) != static_cast<std::uint8_t>(TileId::Air) ||
        ctx.tile(x + 1, y + 1, z) != static_cast<std::uint8_t>(TileId::Air) ||
        ctx.tile(x, y + 1, z - 1) != static_cast<std::uint8_t>(TileId::Air) ||
        ctx.tile(x, y + 1, z + 1) != static_cast<std::uint8_t>(TileId::Air)) {
      continue;
    }

    const int height = 1 + nextInt(rng, 3);
    for (int yy = 1; yy <= height; ++yy) {
      if (ctx.tile(x, y + yy, z) == static_cast<std::uint8_t>(TileId::Air)) {
        ctx.setLocalTile(x, y + yy, z, static_cast<std::uint8_t>(TileId::Cactus));
      }
    }
  }

  auto placeSimplePlant = [&](std::uint8_t plantTile, int attempts, auto&& groundAllowed) {
    for (int i = 0; i < attempts; ++i) {
      const int x = nextInt(rng, 16);
      const int z = nextInt(rng, 16);
      const int y = ctx.topSolidY(x, z);
      if (y < 1 || y + 1 >= view.chunkHeight) {
        continue;
      }
      if (!groundAllowed(ctx.tile(x, y, z))) {
        continue;
      }
      if (!ctx.canReplaceAt(x, y + 1, z)) {
        continue;
      }
      ctx.setLocalTile(x, y + 1, z, plantTile);
    }
  };

  placeSimplePlant(static_cast<std::uint8_t>(TileId::TallGrass), budget.tallGrass,
                   [](std::uint8_t g) { return g == static_cast<std::uint8_t>(TileId::Grass); });
  placeSimplePlant(static_cast<std::uint8_t>(TileId::Fern), budget.fern,
                   [](std::uint8_t g) {
                     return g == static_cast<std::uint8_t>(TileId::Grass) || g == static_cast<std::uint8_t>(TileId::Snow);
                   });
  placeSimplePlant(static_cast<std::uint8_t>(TileId::DeadBush), budget.deadBush,
                   [](std::uint8_t g) { return g == static_cast<std::uint8_t>(TileId::Sand); });

  for (int i = 0; i < budget.flower; ++i) {
    const std::uint8_t flower =
        (nextInt(rng, 2) == 0) ? static_cast<std::uint8_t>(TileId::FlowerYellow) : static_cast<std::uint8_t>(TileId::FlowerRed);
    placeSimplePlant(flower, 1, [](std::uint8_t g) { return g == static_cast<std::uint8_t>(TileId::Grass); });
  }

  for (int i = 0; i < budget.mushroom; ++i) {
    const std::uint8_t mush =
        (nextInt(rng, 2) == 0) ? static_cast<std::uint8_t>(TileId::MushroomBrown) : static_cast<std::uint8_t>(TileId::MushroomRed);
    placeSimplePlant(mush, 1, [](std::uint8_t g) {
      return g == static_cast<std::uint8_t>(TileId::Grass) || g == static_cast<std::uint8_t>(TileId::Dirt) ||
             g == static_cast<std::uint8_t>(TileId::Snow);
    });
  }

  for (int i = 0; i < budget.sugarCane; ++i) {
    const int x = nextInt(rng, 16);
    const int z = nextInt(rng, 16);
    const int y = ctx.topSolidY(x, z);
    if (y < 1 || y + 3 >= view.chunkHeight) {
      continue;
    }
    if (x <= 0 || x >= 15 || z <= 0 || z >= 15) {
      continue;
    }

    const std::uint8_t ground = ctx.tile(x, y, z);
    if (ground != static_cast<std::uint8_t>(TileId::Sand) && ground != static_cast<std::uint8_t>(TileId::Grass) &&
        ground != static_cast<std::uint8_t>(TileId::Dirt)) {
      continue;
    }

    const bool nearWater =
        ctx.tile(x - 1, y, z) == static_cast<std::uint8_t>(TileId::Water) ||
        ctx.tile(x + 1, y, z) == static_cast<std::uint8_t>(TileId::Water) ||
        ctx.tile(x, y, z - 1) == static_cast<std::uint8_t>(TileId::Water) ||
        ctx.tile(x, y, z + 1) == static_cast<std::uint8_t>(TileId::Water);
    if (!nearWater || !ctx.canReplaceAt(x, y + 1, z)) {
      continue;
    }

    const int height = 1 + nextInt(rng, 3);
    for (int yy = 1; yy <= height; ++yy) {
      if (!ctx.canReplaceAt(x, y + yy, z)) {
        break;
      }
      ctx.setLocalTile(x, y + yy, z, static_cast<std::uint8_t>(TileId::SugarCane));
    }
  }
}

BiomeDecoratorPipeline::BiomeDecoratorPipeline(std::uint32_t seed) {
  decorators_.emplace_back(std::make_unique<OreDecorator>(seed));
  decorators_.emplace_back(std::make_unique<VegetationDecorator>(seed));
}

void BiomeDecoratorPipeline::decorateChunk(int chunkX, int chunkZ, std::uint8_t* blocks, int chunkHeight,
                                           const std::vector<BiomeSample>& biomes) const {
  ChunkDecorationView view;
  view.chunkX = chunkX;
  view.chunkZ = chunkZ;
  view.blocks = blocks;
  view.chunkHeight = chunkHeight;
  view.biomes = &biomes;

  for (const auto& decorator : decorators_) {
    decorator->decorate(view);
  }
}

}  // namespace mc::gen
