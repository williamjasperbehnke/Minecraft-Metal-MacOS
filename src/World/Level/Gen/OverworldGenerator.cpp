#include "World/Level/Gen/OverworldGenerator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <random>
#include <vector>

#include "World/Chunk/LevelChunk.h"
#include "World/Level/Gen/BiomeProvider.h"
#include "World/Level/Gen/Noise.h"
#include "World/Level/Gen/StructureGenerator.h"
#include "World/Tile/Tile.h"

namespace mc::gen {

namespace {

constexpr int kChunkSize = 16;
constexpr int kChunkHeight = LevelChunk::kSizeY;
constexpr int kChunkWidthCell = 4;   // PS3 RandomLevelSource CHUNK_WIDTH
constexpr int kChunkHeightCell = 8;  // PS3 RandomLevelSource CHUNK_HEIGHT
constexpr int kSeaLevel = 63;
constexpr double kPi = 3.14159265358979323846;

inline int blockIndex(int x, int y, int z) {
  return (z * kChunkSize + x) * kChunkHeight + y;
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

struct NoiseState {
  explicit NoiseState(std::uint32_t seed)
      : rng(seed),
        lperlin1(rng, 16),
        lperlin2(rng, 16),
        perlin1(rng, 8),
        perlin3(rng, 4),
        scaleNoise(rng, 10),
        depthNoise(rng, 16) {}

  std::mt19937_64 rng;
  PerlinNoise lperlin1;
  PerlinNoise lperlin2;
  PerlinNoise perlin1;
  PerlinNoise perlin3;
  PerlinNoise scaleNoise;
  PerlinNoise depthNoise;
};

struct TunnelBounds {
  int x0 = 0;
  int x1 = 0;
  int y0 = 0;
  int y1 = 0;
  int z0 = 0;
  int z1 = 0;
};

inline bool isCarvableSolid(std::uint8_t t) {
  return t == static_cast<std::uint8_t>(TileId::Stone) || t == static_cast<std::uint8_t>(TileId::Dirt) ||
         t == static_cast<std::uint8_t>(TileId::Grass);
}

TunnelBounds computeTunnelBounds(double xCave, double yCave, double zCave, double rad, double yRad, int xOffs, int zOffs) {
  TunnelBounds b;
  b.x0 = static_cast<int>(std::floor(xCave - rad)) - xOffs * 16 - 1;
  b.x1 = static_cast<int>(std::floor(xCave + rad)) - xOffs * 16 + 1;
  b.y0 = static_cast<int>(std::floor(yCave - yRad)) - 1;
  b.y1 = static_cast<int>(std::floor(yCave + yRad)) + 1;
  b.z0 = static_cast<int>(std::floor(zCave - rad)) - zOffs * 16 - 1;
  b.z1 = static_cast<int>(std::floor(zCave + rad)) - zOffs * 16 + 1;

  b.x0 = std::max(0, b.x0);
  b.x1 = std::min(16, b.x1);
  b.y0 = std::max(1, b.y0);
  b.y1 = std::min(kChunkHeight - 8, b.y1);
  b.z0 = std::max(0, b.z0);
  b.z1 = std::min(16, b.z1);
  return b;
}

bool tunnelHitsWater(const std::uint8_t* blocks, const TunnelBounds& b) {
  bool detectedWater = false;
  for (int xx = b.x0; !detectedWater && xx < b.x1; ++xx) {
    for (int zz = b.z0; !detectedWater && zz < b.z1; ++zz) {
      for (int yy = b.y1 + 1; !detectedWater && yy >= b.y0 - 1; --yy) {
        if (yy < 0 || yy >= kChunkHeight) {
          continue;
        }
        const std::uint8_t t = blocks[blockIndex(xx, yy, zz)];
        if (t == static_cast<std::uint8_t>(TileId::Water)) {
          detectedWater = true;
        }
        // Skip directly to floor for interior columns: this keeps the original
        // early-out behavior while avoiding unnecessary vertical scans.
        if (yy != b.y0 - 1 && xx != b.x0 && xx != b.x1 - 1 && zz != b.z0 && zz != b.z1 - 1) {
          yy = b.y0;
        }
      }
    }
  }
  return detectedWater;
}

template <typename InsideFn>
void carveTunnelVoxels(std::uint8_t* blocks, const std::vector<BiomeSample>& biomes, int xOffs, int zOffs, const TunnelBounds& b,
                       double xCave, double yCave, double zCave, double rad, double yRad, InsideFn&& isInside) {
  for (int xx = b.x0; xx < b.x1; ++xx) {
    const double xd = ((xx + xOffs * 16 + 0.5) - xCave) / rad;
    for (int zz = b.z0; zz < b.z1; ++zz) {
      const double zd = ((zz + zOffs * 16 + 0.5) - zCave) / rad;
      if (xd * xd + zd * zd >= 1.0) {
        continue;
      }

      // NOTE: starts at y1 (not y1-1) to preserve legacy PS3-like carve indexing.
      int p = blockIndex(xx, b.y1, zz);
      bool hasGrass = false;
      for (int yy = b.y1 - 1; yy >= b.y0; --yy) {
        const double yd = (yy + 0.5 - yCave) / yRad;
        if (isInside(xd, yd, zd, yy)) {
          const std::uint8_t block = blocks[p];
          if (block == static_cast<std::uint8_t>(TileId::Grass)) {
            hasGrass = true;
          }
          if (isCarvableSolid(block)) {
            blocks[p] = static_cast<std::uint8_t>(TileId::Air);
            if (hasGrass && yy > 0 && blocks[p - 1] == static_cast<std::uint8_t>(TileId::Dirt)) {
              blocks[p - 1] = biomes[xx + zz * 16].top;
            }
          }
        }
        --p;
      }
    }
  }
}

class TerrainSurfacePass {
public:
  TerrainSurfacePass(std::uint32_t seed, NoiseState& noise) : seed_(seed), noise_(noise) {}

  void buildSurfaces(int chunkX, int chunkZ, std::uint8_t* blocks, const std::vector<BiomeSample>& biomes) const {
    std::uint64_t chunkSeed = static_cast<std::uint64_t>(seed_);
    chunkSeed ^= static_cast<std::uint64_t>(chunkX) * 341873128712ULL;
    chunkSeed ^= static_cast<std::uint64_t>(chunkZ) * 132897987541ULL;
    std::mt19937_64 chunkRng(chunkSeed);

    std::vector<double> depthBuffer;
    noise_.perlin3.getRegion2D(depthBuffer, chunkX * 16, chunkZ * 16, 16, 16, (1.0 / 32.0) * 2.0, (1.0 / 32.0) * 2.0);

    for (int x = 0; x < 16; ++x) {
      for (int z = 0; z < 16; ++z) {
        const int runDepth = static_cast<int>(depthBuffer[x + z * 16] / 3.0 + 3.0 + nextFloat01(chunkRng) * 0.25);
        int run = -1;
        const BiomeSample biome = biomes[x + z * 16];
        std::uint8_t top = biome.top;
        std::uint8_t material = biome.filler;
        const double temp = biome.temperature;

        for (int y = kChunkHeight - 1; y >= 0; --y) {
          const int offs = blockIndex(x, y, z);

          // PS3 style bedrock: y <= 1 + random(2)
          if (y <= 1 + nextInt(chunkRng, 2)) {
            blocks[offs] = static_cast<std::uint8_t>(TileId::Bedrock);
            continue;
          }

          const std::uint8_t old = blocks[offs];
          if (old == static_cast<std::uint8_t>(TileId::Air)) {
            run = -1;
          } else if (old == static_cast<std::uint8_t>(TileId::Stone)) {
            if (run == -1) {
              if (runDepth <= 0) {
                top = static_cast<std::uint8_t>(TileId::Air);
                material = static_cast<std::uint8_t>(TileId::Stone);
              } else if (y >= kSeaLevel - 4 && y <= kSeaLevel + 1) {
                top = biomes[x + z * 16].top;
                material = biomes[x + z * 16].filler;
              }

              if (y < kSeaLevel && top == static_cast<std::uint8_t>(TileId::Air)) {
                top = (temp < 0.15) ? static_cast<std::uint8_t>(TileId::Ice) : static_cast<std::uint8_t>(TileId::Water);
              }

              // Ported-style biome surface tweaks:
              // - deserts keep a thicker sand cap before sandstone,
              // - cold/high terrain prefers snow tops.
              int localRunDepth = runDepth;
              if (biome.kind == BiomeKind::Desert) {
                localRunDepth += 2;
              }
              if ((biome.kind == BiomeKind::Taiga || temp < 0.20) && y > kSeaLevel + 1 &&
                  top == static_cast<std::uint8_t>(TileId::Grass)) {
                top = static_cast<std::uint8_t>(TileId::Snow);
                material = static_cast<std::uint8_t>(TileId::Dirt);
              }
              if (biome.kind == BiomeKind::Mountains && y > kSeaLevel + 26) {
                top = static_cast<std::uint8_t>(TileId::Snow);
              }

              run = localRunDepth;
              blocks[offs] = (y >= kSeaLevel - 1) ? top : material;
            } else if (run > 0) {
              --run;
              blocks[offs] = material;
              // PS3 behavior: place a short sandstone run below sand.
              if (run == 0 && material == static_cast<std::uint8_t>(TileId::Sand)) {
                run = nextInt(chunkRng, 4);
                material = static_cast<std::uint8_t>(TileId::Sandstone);
              }
            }
          }
        }
      }
    }
  }

  void shapeOceanFloor(int chunkX, int chunkZ, std::uint8_t* blocks) const {
    auto floorSignal = [&](int wx, int wz) {
      const double n0 = noise_.scaleNoise.sample2D(static_cast<double>(wx) / 180.0, static_cast<double>(wz) / 180.0);
      const double n1 = noise_.perlin3.sample2D(static_cast<double>(wx) / 70.0, static_cast<double>(wz) / 70.0);
      const double n2 = noise_.depthNoise.sample2D(static_cast<double>(wx) / 36.0, static_cast<double>(wz) / 36.0);
      return n0 * 3.2 + n1 * 1.1 + n2 * 0.45;
    };

    std::array<int, 16 * 16> topSolid{};
    for (int x = 0; x < 16; ++x) {
      for (int z = 0; z < 16; ++z) {
        int topSolidY = -1;
        for (int y = kChunkHeight - 1; y >= 0; --y) {
          const std::uint8_t t = blocks[blockIndex(x, y, z)];
          if (t != static_cast<std::uint8_t>(TileId::Air) && t != static_cast<std::uint8_t>(TileId::Water)) {
            topSolidY = y;
            break;
          }
        }
        topSolid[x + z * 16] = topSolidY;
      }
    }

    for (int x = 0; x < 16; ++x) {
      for (int z = 0; z < 16; ++z) {
        const int worldX = chunkX * 16 + x;
        const int worldZ = chunkZ * 16 + z;
        int topSolidY = topSolid[x + z * 16];
        if (topSolidY < 0 || topSolidY >= kSeaLevel - 3) {
          continue;
        }

        const double s = floorSignal(worldX, worldZ) * 0.22 +
                         floorSignal(worldX - 1, worldZ) * 0.12 +
                         floorSignal(worldX + 1, worldZ) * 0.12 +
                         floorSignal(worldX, worldZ - 1) * 0.12 +
                         floorSignal(worldX, worldZ + 1) * 0.12 +
                         floorSignal(worldX - 1, worldZ - 1) * 0.07 +
                         floorSignal(worldX + 1, worldZ - 1) * 0.07 +
                         floorSignal(worldX - 1, worldZ + 1) * 0.07 +
                         floorSignal(worldX + 1, worldZ + 1) * 0.07 +
                         floorSignal(worldX - 2, worldZ) * 0.04 +
                         floorSignal(worldX + 2, worldZ) * 0.04 +
                         floorSignal(worldX, worldZ - 2) * 0.04 +
                         floorSignal(worldX, worldZ + 2) * 0.04;
        const double macro = noise_.scaleNoise.sample2D(static_cast<double>(worldX) / 420.0, static_cast<double>(worldZ) / 420.0);
        const double micro = noise_.depthNoise.sample2D(static_cast<double>(worldX) / 22.0, static_cast<double>(worldZ) / 22.0);
        const double dither =
            noise_.perlin3.sample2D(static_cast<double>(worldX) / 11.0, static_cast<double>(worldZ) / 11.0) * 0.20;
        int desired = std::clamp(kSeaLevel - 8 + static_cast<int>(std::lround(s * 0.75 + macro * 1.8 + micro * 0.35 + dither)), 2,
                                 kSeaLevel - 2);

        int nearLandCount = 0;
        int sampleCount = 0;
        for (int ox = -4; ox <= 4; ++ox) {
          for (int oz = -4; oz <= 4; ++oz) {
            if (ox == 0 && oz == 0) {
              continue;
            }
            const int nx = x + ox;
            const int nz = z + oz;
            if (nx < 0 || nx >= 16 || nz < 0 || nz >= 16) {
              continue;
            }
            ++sampleCount;
            if (topSolid[nx + nz * 16] >= kSeaLevel - 1) {
              ++nearLandCount;
            }
          }
        }
        if (nearLandCount > 0) {
          double shoreBlend = std::min(1.0, static_cast<double>(nearLandCount) / std::max(1, sampleCount / 2));
          shoreBlend = shoreBlend * shoreBlend * (3.0 - 2.0 * shoreBlend);
          const int shoreTarget = kSeaLevel - 2 + static_cast<int>(std::lround(macro * 0.45));
          desired = static_cast<int>(std::lround(desired * (1.0 - shoreBlend * 0.90) + shoreTarget * (shoreBlend * 0.90)));
          const int minAllowed = std::max(2, topSolidY);
          desired = std::max(desired, minAllowed);
        }

        if (topSolidY < kSeaLevel) {
          const int depthBelowSea = kSeaLevel - topSolidY;
          if (depthBelowSea <= 20) {
            const double t = 1.0 - (static_cast<double>(depthBelowSea) / 20.0);
            const double shelfBlend = t * t;
            const int shelfTarget = kSeaLevel - (2 + depthBelowSea / 6);
            desired = static_cast<int>(std::lround(desired * (1.0 - shelfBlend * 0.80) + shelfTarget * (shelfBlend * 0.80)));
          }
        }

        const double matA = noise_.scaleNoise.sample2D(static_cast<double>(worldX) / 13.0, static_cast<double>(worldZ) / 13.0);
        const double matB = noise_.depthNoise.sample2D(static_cast<double>(worldX) / 7.0, static_cast<double>(worldZ) / 7.0);
        const std::uint32_t h = static_cast<std::uint32_t>(worldX * 73428767) ^ static_cast<std::uint32_t>(worldZ * 912931);
        const double jitter = static_cast<double>(h & 1023u) / 1023.0;
        const double matMix = matA * 0.55 + matB * 0.45 + (jitter - 0.5) * 0.22;
        const bool gravelPatch = matMix > 0.58;
        const std::uint8_t seabedMat = gravelPatch ? static_cast<std::uint8_t>(TileId::Gravel)
                                                   : static_cast<std::uint8_t>(TileId::Sand);

        if (desired < topSolidY) {
          for (int y = topSolidY; y > desired; --y) {
            blocks[blockIndex(x, y, z)] = static_cast<std::uint8_t>(TileId::Water);
          }
          topSolidY = desired;
        } else if (desired > topSolidY) {
          for (int y = topSolidY + 1; y <= desired; ++y) {
            blocks[blockIndex(x, y, z)] = seabedMat;
          }
          topSolidY = desired;
        }

        blocks[blockIndex(x, topSolidY, z)] = seabedMat;
        if (topSolidY > 1) {
          blocks[blockIndex(x, topSolidY - 1, z)] = seabedMat;
        }
      }
    }
  }

private:
  std::uint32_t seed_ = 0;
  NoiseState& noise_;
};

class CaveCarver {
public:
  explicit CaveCarver(std::uint32_t seed) : seed_(seed) {}

  void carve(int chunkX, int chunkZ, std::uint8_t* blocks, const std::vector<BiomeSample>& biomes) const {
    constexpr int radius = 8;
    std::mt19937_64 seedRng(static_cast<std::uint64_t>(seed_));
    const std::int64_t xScale = nextLong(seedRng);
    const std::int64_t zScale = nextLong(seedRng);
    const std::uint64_t worldSeed = static_cast<std::uint64_t>(seed_);

    for (int x = chunkX - radius; x <= chunkX + radius; ++x) {
      for (int z = chunkZ - radius; z <= chunkZ + radius; ++z) {
        const std::uint64_t xx = static_cast<std::uint64_t>(static_cast<std::int64_t>(x) * xScale);
        const std::uint64_t zz = static_cast<std::uint64_t>(static_cast<std::int64_t>(z) * zScale);
        std::mt19937_64 rng(xx ^ zz ^ worldSeed);
        addCaveFeature(rng, x, z, chunkX, chunkZ, blocks, biomes);
      }
    }
  }

private:
  void addCaveFeature(std::mt19937_64& rng, int x, int z, int xOffs, int zOffs, std::uint8_t* blocks,
                      const std::vector<BiomeSample>& biomes) const {
    int caves = nextInt(rng, nextInt(rng, nextInt(rng, 40) + 1) + 1);
    if (nextInt(rng, 15) != 0) {
      caves = 0;
    }

    for (int cave = 0; cave < caves; ++cave) {
      const double xCave = x * 16 + nextInt(rng, 16);
      const double yCave = nextInt(rng, nextInt(rng, kChunkHeight - 8) + 8);
      const double zCave = z * 16 + nextInt(rng, 16);

      int tunnels = 1;
      if (nextInt(rng, 4) == 0) {
        const std::int64_t roomSeed = nextLong(rng);
        addCaveTunnel(roomSeed, xOffs, zOffs, blocks, biomes, xCave, yCave, zCave, 1.0 + nextFloat01(rng) * 6.0, 0.0f, 0.0f, -1,
                      -1, 0.5);
        tunnels += nextInt(rng, 4);
      }

      for (int i = 0; i < tunnels; ++i) {
        const float yRot = static_cast<float>(nextFloat01(rng) * kPi * 2.0);
        const float xRot = static_cast<float>(((nextFloat01(rng) - 0.5) * 2.0) / 8.0);
        float thickness = static_cast<float>(nextFloat01(rng) * 2.0 + nextFloat01(rng));
        if (nextInt(rng, 10) == 0) {
          thickness *= static_cast<float>(nextFloat01(rng) * nextFloat01(rng) * 3.0 + 1.0);
        }
        addCaveTunnel(nextLong(rng), xOffs, zOffs, blocks, biomes, xCave, yCave, zCave, thickness, yRot, xRot, 0, 0, 1.0);
      }
    }
  }

  void addCaveTunnel(std::int64_t seedValue, int xOffs, int zOffs, std::uint8_t* blocks, const std::vector<BiomeSample>& biomes,
                     double xCave, double yCave, double zCave, float thickness, float yRot, float xRot, int step, int dist,
                     double yScale) const {
    std::mt19937_64 rng(static_cast<std::uint64_t>(seedValue));
    constexpr int radius = 8;

    const double xMid = xOffs * 16 + 8.0;
    const double zMid = zOffs * 16 + 8.0;
    float yRota = 0.0f;
    float xRota = 0.0f;

    if (dist <= 0) {
      const int max = radius * 16 - 16;
      dist = max - nextInt(rng, max / 4);
    }

    bool singleStep = false;
    if (step == -1) {
      step = dist / 2;
      singleStep = true;
    }

    const int splitPoint = nextInt(rng, dist / 2) + dist / 4;
    const bool steep = nextInt(rng, 6) == 0;

    for (; step < dist; ++step) {
      const double rad = 1.5 + (std::sin(step * kPi / static_cast<double>(dist)) * thickness);
      const double yRad = rad * yScale;

      const float xc = std::cos(xRot);
      const float xs = std::sin(xRot);
      xCave += std::cos(yRot) * xc;
      yCave += xs;
      zCave += std::sin(yRot) * xc;

      xRot *= steep ? 0.92f : 0.7f;
      xRot += xRota * 0.1f;
      yRot += yRota * 0.1f;
      xRota *= 0.90f;
      yRota *= 0.75f;
      xRota += static_cast<float>((nextFloat01(rng) - nextFloat01(rng)) * nextFloat01(rng) * 2.0);
      yRota += static_cast<float>((nextFloat01(rng) - nextFloat01(rng)) * nextFloat01(rng) * 4.0);

      // Legacy cave behavior: split thick tunnels into two lateral branches.
      if (!singleStep && step == splitPoint && thickness > 1.0f && dist > 0) {
        addCaveTunnel(nextLong(rng), xOffs, zOffs, blocks, biomes, xCave, yCave, zCave,
                      static_cast<float>(nextFloat01(rng) * 0.5 + 0.5), yRot - static_cast<float>(kPi / 2.0), xRot / 3.0f, step,
                      dist, 1.0);
        addCaveTunnel(nextLong(rng), xOffs, zOffs, blocks, biomes, xCave, yCave, zCave,
                      static_cast<float>(nextFloat01(rng) * 0.5 + 0.5), yRot + static_cast<float>(kPi / 2.0), xRot / 3.0f, step,
                      dist, 1.0);
        return;
      }
      if (!singleStep && nextInt(rng, 4) == 0) {
        continue;
      }

      const double xdMid = xCave - xMid;
      const double zdMid = zCave - zMid;
      const double remaining = dist - step;
      const double rr = (thickness + 2.0) + 16.0;
      if (xdMid * xdMid + zdMid * zdMid - (remaining * remaining) > rr * rr) {
        return;
      }
      if (xCave < xMid - 16.0 - rad * 2.0 || zCave < zMid - 16.0 - rad * 2.0 || xCave > xMid + 16.0 + rad * 2.0 ||
          zCave > zMid + 16.0 + rad * 2.0) {
        continue;
      }

      const TunnelBounds bounds = computeTunnelBounds(xCave, yCave, zCave, rad, yRad, xOffs, zOffs);
      if (tunnelHitsWater(blocks, bounds)) {
        continue;
      }
      carveTunnelVoxels(blocks, biomes, xOffs, zOffs, bounds, xCave, yCave, zCave, rad, yRad,
                        [](double xd, double yd, double zd, int /*yy*/) { return yd > -0.7 && xd * xd + yd * yd + zd * zd < 1.0; });

      if (singleStep) {
        break;
      }
    }
  }

  std::uint32_t seed_ = 0;
};

class CanyonCarver {
public:
  explicit CanyonCarver(std::uint32_t seed) : seed_(seed) {}

  void carve(int chunkX, int chunkZ, std::uint8_t* blocks, const std::vector<BiomeSample>& biomes) const {
    constexpr int radius = 8;
    std::mt19937_64 seedRng(static_cast<std::uint64_t>(seed_));
    const std::int64_t xScale = nextLong(seedRng);
    const std::int64_t zScale = nextLong(seedRng);
    const std::uint64_t worldSeed = static_cast<std::uint64_t>(seed_);

    for (int x = chunkX - radius; x <= chunkX + radius; ++x) {
      for (int z = chunkZ - radius; z <= chunkZ + radius; ++z) {
        const std::uint64_t xx = static_cast<std::uint64_t>(static_cast<std::int64_t>(x) * xScale);
        const std::uint64_t zz = static_cast<std::uint64_t>(static_cast<std::int64_t>(z) * zScale);
        std::mt19937_64 rng(xx ^ zz ^ worldSeed);
        addCanyonFeature(rng, x, z, chunkX, chunkZ, blocks, biomes);
      }
    }
  }

private:
  void addCanyonFeature(std::mt19937_64& rng, int x, int z, int xOffs, int zOffs, std::uint8_t* blocks,
                        const std::vector<BiomeSample>& biomes) const {
    if (nextInt(rng, 50) != 0) {
      return;
    }

    const double xCave = x * 16 + nextInt(rng, 16);
    const double yCave = nextInt(rng, nextInt(rng, 40) + 8) + 20;
    const double zCave = z * 16 + nextInt(rng, 16);
    const float yRot = static_cast<float>(nextFloat01(rng) * kPi * 2.0);
    const float xRot = static_cast<float>(((nextFloat01(rng) - 0.5) * 2.0) / 8.0);
    const float thickness = static_cast<float>((nextFloat01(rng) * 2.0 + nextFloat01(rng)) * 2.0);

    addCanyonTunnel(nextLong(rng), xOffs, zOffs, blocks, biomes, xCave, yCave, zCave, thickness, yRot, xRot, 0, 0, 3.0);
  }

  void addCanyonTunnel(std::int64_t seedValue, int xOffs, int zOffs, std::uint8_t* blocks, const std::vector<BiomeSample>& biomes,
                       double xCave, double yCave, double zCave, float thickness, float yRot, float xRot, int step, int dist,
                       double yScale) const {
    std::mt19937_64 rng(static_cast<std::uint64_t>(seedValue));
    constexpr int radius = 8;

    const double xMid = xOffs * 16 + 8.0;
    const double zMid = zOffs * 16 + 8.0;
    float yRota = 0.0f;
    float xRota = 0.0f;

    if (dist <= 0) {
      const int max = radius * 16 - 16;
      dist = max - nextInt(rng, max / 4);
    }

    bool singleStep = false;
    if (step == -1) {
      step = dist / 2;
      singleStep = true;
    }

    std::array<float, 1024> rs{};
    float f = 1.0f;
    for (int i = 0; i < kChunkHeight; ++i) {
      if (i == 0 || nextInt(rng, 3) == 0) {
        f = 1.0f + static_cast<float>(nextFloat01(rng) * nextFloat01(rng));
      }
      // Per-y stretch profile: produces canyon walls that vary by altitude.
      rs[i] = f * f;
    }

    for (; step < dist; ++step) {
      double rad = 1.5 + (std::sin(step * kPi / static_cast<double>(dist)) * thickness);
      double yRad = rad * yScale;
      rad *= (nextFloat01(rng) * 0.25 + 0.75);
      yRad *= (nextFloat01(rng) * 0.25 + 0.75);

      const float xc = std::cos(xRot);
      const float xs = std::sin(xRot);
      xCave += std::cos(yRot) * xc;
      yCave += xs;
      zCave += std::sin(yRot) * xc;

      xRot *= 0.7f;
      xRot += xRota * 0.05f;
      yRot += yRota * 0.05f;
      xRota *= 0.80f;
      yRota *= 0.50f;
      xRota += static_cast<float>((nextFloat01(rng) - nextFloat01(rng)) * nextFloat01(rng) * 2.0);
      yRota += static_cast<float>((nextFloat01(rng) - nextFloat01(rng)) * nextFloat01(rng) * 4.0);

      if (!singleStep && nextInt(rng, 4) == 0) {
        continue;
      }

      const double xdMid = xCave - xMid;
      const double zdMid = zCave - zMid;
      const double remaining = dist - step;
      const double rr = (thickness + 2.0) + 16.0;
      if (xdMid * xdMid + zdMid * zdMid - (remaining * remaining) > rr * rr) {
        return;
      }
      if (xCave < xMid - 16.0 - rad * 2.0 || zCave < zMid - 16.0 - rad * 2.0 || xCave > xMid + 16.0 + rad * 2.0 ||
          zCave > zMid + 16.0 + rad * 2.0) {
        continue;
      }

      const TunnelBounds bounds = computeTunnelBounds(xCave, yCave, zCave, rad, yRad, xOffs, zOffs);
      if (tunnelHitsWater(blocks, bounds)) {
        continue;
      }
      carveTunnelVoxels(blocks, biomes, xOffs, zOffs, bounds, xCave, yCave, zCave, rad, yRad,
                        [&rs](double xd, double yd, double zd, int yy) {
                          return (xd * xd + zd * zd) * rs[yy] + (yd * yd / 6.0) < 1.0;
                        });

      if (singleStep) {
        break;
      }
    }
  }

  std::uint32_t seed_ = 0;
};

}  // namespace

struct OverworldGenerator::Impl {
  explicit Impl(std::uint32_t seedIn)
      : seed(seedIn), biomeProvider(seedIn), structureGenerator(seedIn), noise(std::make_unique<NoiseState>(seedIn)) {}

  std::uint32_t seed;
  BiomeProvider biomeProvider;
  StructureGenerator structureGenerator;
  std::unique_ptr<NoiseState> noise;

  void fillChunk(LevelChunk& chunk) {
    std::vector<std::uint8_t> blocks(static_cast<std::size_t>(kChunkSize) * kChunkSize * kChunkHeight,
                                     static_cast<std::uint8_t>(TileId::Air));
    std::vector<BiomeSample> biomes;
    biomeProvider.sampleChunkBiomes(chunk.chunkX(), chunk.chunkZ(), biomes);
    TerrainSurfacePass surfacePass(seed, *noise);
    CaveCarver caveCarver(seed);
    CanyonCarver canyonCarver(seed);

    prepareHeights(chunk.chunkX(), chunk.chunkZ(), blocks.data());
    surfacePass.buildSurfaces(chunk.chunkX(), chunk.chunkZ(), blocks.data(), biomes);
    surfacePass.shapeOceanFloor(chunk.chunkX(), chunk.chunkZ(), blocks.data());
    caveCarver.carve(chunk.chunkX(), chunk.chunkZ(), blocks.data(), biomes);
    canyonCarver.carve(chunk.chunkX(), chunk.chunkZ(), blocks.data(), biomes);
    structureGenerator.applyChunkFeatures(chunk.chunkX(), chunk.chunkZ(), blocks.data(), kChunkHeight, biomes);

    for (int x = 0; x < kChunkSize; ++x) {
      for (int z = 0; z < kChunkSize; ++z) {
        for (int y = 0; y < kChunkHeight; ++y) {
          chunk.setTile(x, y, z, blocks[blockIndex(x, y, z)]);
        }
      }
    }
  }

  void prepareHeights(int chunkX, int chunkZ, std::uint8_t* blocks) {
    const int xChunks = kChunkSize / kChunkWidthCell;
    const int yChunks = kChunkHeight / kChunkHeightCell;
    const int xSize = xChunks + 1;
    const int ySize = yChunks + 1;
    const int zSize = xChunks + 1;

    std::vector<double> buffer;
    getHeights(buffer, chunkX * xChunks, 0, chunkZ * xChunks, xSize, ySize, zSize);

    for (int xc = 0; xc < xChunks; ++xc) {
      for (int zc = 0; zc < xChunks; ++zc) {
        for (int yc = 0; yc < yChunks; ++yc) {
          const double yStep = 1.0 / static_cast<double>(kChunkHeightCell);
          double s0 = buffer[((xc + 0) * zSize + (zc + 0)) * ySize + (yc + 0)];
          double s1 = buffer[((xc + 0) * zSize + (zc + 1)) * ySize + (yc + 0)];
          double s2 = buffer[((xc + 1) * zSize + (zc + 0)) * ySize + (yc + 0)];
          double s3 = buffer[((xc + 1) * zSize + (zc + 1)) * ySize + (yc + 0)];

          const double s0a = (buffer[((xc + 0) * zSize + (zc + 0)) * ySize + (yc + 1)] - s0) * yStep;
          const double s1a = (buffer[((xc + 0) * zSize + (zc + 1)) * ySize + (yc + 1)] - s1) * yStep;
          const double s2a = (buffer[((xc + 1) * zSize + (zc + 0)) * ySize + (yc + 1)] - s2) * yStep;
          const double s3a = (buffer[((xc + 1) * zSize + (zc + 1)) * ySize + (yc + 1)] - s3) * yStep;

          for (int y = 0; y < kChunkHeightCell; ++y) {
            const double xStep = 1.0 / static_cast<double>(kChunkWidthCell);
            double s0Inner = s0;
            double s1Inner = s1;
            const double s0InnerA = (s2 - s0) * xStep;
            const double s1InnerA = (s3 - s1) * xStep;

            for (int x = 0; x < kChunkWidthCell; ++x) {
              const double zStep = 1.0 / static_cast<double>(kChunkWidthCell);
              double val = s0Inner;
              const double valA = (s1Inner - s0Inner) * zStep;

              const int bx = x + xc * kChunkWidthCell;
              const int by = yc * kChunkHeightCell + y;
              for (int z = 0; z < kChunkWidthCell; ++z) {
                const int bz = z + zc * kChunkWidthCell;
                std::uint8_t tile = static_cast<std::uint8_t>(TileId::Air);
                if (val > 0.0) {
                  tile = static_cast<std::uint8_t>(TileId::Stone);
                } else if (by < kSeaLevel) {
                  tile = static_cast<std::uint8_t>(TileId::Water);
                }
                blocks[blockIndex(bx, by, bz)] = tile;
                val += valA;
              }

              s0Inner += s0InnerA;
              s1Inner += s1InnerA;
            }

            s0 += s0a;
            s1 += s1a;
            s2 += s2a;
            s3 += s3a;
          }
        }
      }
    }
  }

  void getHeights(std::vector<double>& buffer, int x, int y, int z, int xSize, int ySize, int zSize) {
    std::vector<double> dr;
    std::vector<double> pnr;
    std::vector<double> ar;
    std::vector<double> br;

    const double s = 684.412;
    const double hs = 684.412;

    noise->depthNoise.getRegion2D(dr, x, z, xSize, zSize, 200.0, 200.0);
    noise->perlin1.getRegion3D(pnr, x, y, z, xSize, ySize, zSize, s / 80.0, hs / 160.0, s / 80.0);
    noise->lperlin1.getRegion3D(ar, x, y, z, xSize, ySize, zSize, s, hs, s);
    noise->lperlin2.getRegion3D(br, x, y, z, xSize, ySize, zSize, s, hs, s);

    const int bxSize = xSize + 5;
    const int bzSize = zSize + 5;
    std::vector<double> biomeDepth;
    std::vector<double> biomeScale;
    biomeProvider.sampleDepthScaleGrid(x - 2, z - 2, bxSize, bzSize, biomeDepth, biomeScale, 1);

    buffer.assign(static_cast<std::size_t>(xSize) * ySize * zSize, 0.0);

    std::array<float, 25> pows{};
    for (int xb = -2; xb <= 2; ++xb) {
      for (int zb = -2; zb <= 2; ++zb) {
        pows[(xb + 2) + (zb + 2) * 5] = 10.0f / std::sqrt(static_cast<float>(xb * xb + zb * zb) + 0.2f);
      }
    }

    auto biomeDepthScale = [&](int bx, int bz, double* depth, double* scale) {
      const int idx = bx + bz * bxSize;
      *depth = biomeDepth[idx];
      *scale = biomeScale[idx];
    };

    int p = 0;
    int pp = 0;
    for (int xx = 0; xx < xSize; ++xx) {
      for (int zz = 0; zz < zSize; ++zz) {
        double mbDepth = 0.0;
        double mbScale = 0.0;
        biomeDepthScale(xx + 2, zz + 2, &mbDepth, &mbScale);

        double sss = 0.0;
        double ddd = 0.0;
        double pow = 0.0;
        for (int xb = -2; xb <= 2; ++xb) {
          for (int zb = -2; zb <= 2; ++zb) {
            double bDepth = 0.0;
            double bScale = 0.0;
            biomeDepthScale(xx + xb + 2, zz + zb + 2, &bDepth, &bScale);
            double ppp = pows[(xb + 2) + (zb + 2) * 5] / (bDepth + 2.0);
            if (bDepth > mbDepth) {
              ppp /= 2.0;
            }
            sss += bScale * ppp;
            ddd += bDepth * ppp;
            pow += ppp;
          }
        }
        sss /= pow;
        ddd /= pow;
        sss = sss * 0.9 + 0.1;
        ddd = (ddd * 4.0 - 1.0) / 8.0;

        // Add extra large- and medium-scale relief so terrain feels less flat.
        // These are sampled in world space (coarse height grid) so chunk borders stay seamless.
        const double wx = static_cast<double>(x + xx);
        const double wz = static_cast<double>(z + zz);
        const double continental = noise->scaleNoise.sample2D(wx / 28.0, wz / 28.0);
        const double rolling = noise->depthNoise.sample2D(wx / 11.0, wz / 11.0);
        ddd += continental * 0.24 + rolling * 0.10;
        sss += std::abs(rolling) * 0.06;
        if (sss < 0.08) {
          sss = 0.08;
        }

        double rdepth = dr[pp] / 8000.0;
        if (rdepth < 0.0) {
          rdepth = -rdepth * 0.3;
        }
        rdepth = rdepth * 3.0 - 2.0;
        if (rdepth < 0.0) {
          rdepth /= 2.0;
          if (rdepth < -1.0) {
            rdepth = -1.0;
          }
          rdepth /= 1.4;
          rdepth /= 2.0;
        } else {
          if (rdepth > 1.0) {
            rdepth = 1.0;
          }
          rdepth /= 8.0;
        }
        ++pp;

        for (int yy = 0; yy < ySize; ++yy) {
          double depth = ddd;
          double scale = sss;

          depth += rdepth * 0.24;
          depth = depth * ySize / 16.0;
          const double yCenter = ySize / 2.0 + depth * 5.2;

          double yOffs = (yy - yCenter) * 12.0 * 128.0 / static_cast<double>(kChunkHeight) / scale;
          if (yOffs < 0.0) {
            yOffs *= 4.0;
          }

          const double bb = ar[p] / 512.0;
          const double cc = br[p] / 512.0;
          const double v = (pnr[p] / 10.0 + 1.0) / 2.0;

          double val = 0.0;
          if (v < 0.0) {
            val = bb;
          } else if (v > 1.0) {
            val = cc;
          } else {
            // Blend between two octave fields via the control noise `v`.
            val = bb + (cc - bb) * v;
          }
          val -= yOffs;

          if (yy > ySize - 4) {
            const double slide = (yy - (ySize - 4)) / 3.0;
            val = val * (1.0 - slide) + (-10.0) * slide;
          }

          buffer[p++] = val;
        }
      }
    }
  }
};

OverworldGenerator::OverworldGenerator(std::uint32_t seed) : impl_(std::make_unique<Impl>(seed)) {}

OverworldGenerator::~OverworldGenerator() = default;

void OverworldGenerator::fillChunk(LevelChunk& chunk) {
  impl_->fillChunk(chunk);
}

}  // namespace mc::gen
