#include "Client/Render/Metal/LevelRenderer.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>

#include "Client/Render/Metal/ChunkMeshers.h"
#include "World/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Tile/Tile.h"

namespace mc {

namespace {

constexpr int kTexCactusTop = 69;      // (5,4) in the 16x16 terrain atlas
constexpr int kTexCactusSide = 70;     // (6,4)
constexpr int kTexCactusBottom = 71;   // (7,4)

int floorDiv16(int v) {
  return v >= 0 ? v / 16 : (v - 15) / 16;
}

bool isOverlayRenderableTile(int tile) {
  return tile > static_cast<int>(TileId::Air) && tile != static_cast<int>(TileId::Water);
}

bool isCactusRenderTile(int tile) {
  return tile == static_cast<int>(TileId::Cactus);
}

bool isWaterRenderTile(int tile) {
  return tile == static_cast<int>(TileId::Water);
}

simd_float3 applyCactusCutoutTint(int tile, simd_float3 color) {
  if (isCactusRenderTile(tile)) {
    // Negative red channel marks cutout geometry in the fragment shader.
    color.x = -std::abs(color.x);
  }
  return color;
}

float cactusSideInsetForFace(int tile, int faceOrdinal) {
  if (!isCactusRenderTile(tile)) {
    return 0.0f;
  }
  // LevelRenderer::Face ordinal order: Top(0), Bottom(1), North(2), South(3), West(4), East(5).
  if (faceOrdinal >= 2 && faceOrdinal <= 5) {
    return 1.0f / 16.0f;
  }
  return 0.0f;
}

float faceTopYForTile(int tile, float y, float inflate, bool waterHasSameAbove) {
  if (isWaterRenderTile(tile)) {
    if (waterHasSameAbove) {
      return y + 1.0f + inflate;
    }
    return y + (14.0f / 16.0f) + inflate;
  }
  return y + 1.0f + inflate;
}

struct FaceBounds {
  float x0 = 0.0f;
  float x1 = 1.0f;
  float z0 = 0.0f;
  float z1 = 1.0f;
  float minY = 0.0f;
  float maxY = 1.0f;
  float topX0 = 0.0f;
  float topX1 = 1.0f;
  float topZ0 = 0.0f;
  float topZ1 = 1.0f;
  float sideX0 = 0.0f;
  float sideX1 = 1.0f;
  float sideZ0 = 0.0f;
  float sideZ1 = 1.0f;
  float northZ = 0.0f;
  float southZ = 1.0f;
  float westX = 0.0f;
  float eastX = 1.0f;
};

FaceBounds computeFaceBounds(int tile, int faceOrdinal, float x, float y, float z, float inflate, bool waterHasSameAbove) {
  const bool isCactus = isCactusRenderTile(tile);
  const float cactusSideInset = cactusSideInsetForFace(tile, faceOrdinal);
  const float cactusCornerTrim = isCactus ? 0.016 : 0.0f;

  FaceBounds bounds;
  bounds.x0 = x - inflate;
  bounds.x1 = x + 1.0f + inflate;
  bounds.minY = y - inflate;
  bounds.maxY = faceTopYForTile(tile, y, inflate, waterHasSameAbove);
  bounds.z0 = z - inflate;
  bounds.z1 = z + 1.0f + inflate;
  bounds.sideX0 = bounds.x0 + cactusCornerTrim;
  bounds.sideX1 = bounds.x1 - cactusCornerTrim;
  bounds.sideZ0 = bounds.z0 + cactusCornerTrim;
  bounds.sideZ1 = bounds.z1 - cactusCornerTrim;
  bounds.topX0 = isCactus ? bounds.sideX0 : bounds.x0;
  bounds.topX1 = isCactus ? bounds.sideX1 : bounds.x1;
  bounds.topZ0 = isCactus ? bounds.sideZ0 : bounds.z0;
  bounds.topZ1 = isCactus ? bounds.sideZ1 : bounds.z1;
  bounds.northZ = bounds.z0 + cactusSideInset;
  bounds.southZ = bounds.z1 - cactusSideInset;
  bounds.westX = bounds.x0 + cactusSideInset;
  bounds.eastX = bounds.x1 - cactusSideInset;
  return bounds;
}

}  // namespace

LevelRenderer::LevelRenderer(MetalRenderer* metalRenderer) : metalRenderer_(metalRenderer) {
}

std::int64_t LevelRenderer::chunkKey(int chunkX, int chunkZ) {
  return (static_cast<std::int64_t>(chunkX) << 32) ^ static_cast<std::uint32_t>(chunkZ);
}

void LevelRenderer::setLevel(Level* level) {
  if (level_ == level) {
    return;
  }
  if (level_) {
    level_->removeListener(this);
  }
  level_ = level;
  if (level_) {
    level_->addListener(this);
  }
  chunkMeshes_.clear();
  chunkTransparentMeshes_.clear();
  visibleChunks_.clear();
  dirtyChunks_.clear();
  urgentDirtyChunks_.clear();
  metalRenderer_->clearChunkMeshes();
  metalRenderer_->setChunkDrawList({});
  visibleSetDirty_ = true;
  rebuildTerrain();
}

void LevelRenderer::setRenderCenter(int x, int z) {
  const int chunkX = floorDiv16(x);
  const int chunkZ = floorDiv16(z);
  if (centerInitialized_ && chunkX == centerChunkX_ && chunkZ == centerChunkZ_) {
    return;
  }
  centerInitialized_ = true;
  centerChunkX_ = chunkX;
  centerChunkZ_ = chunkZ;
  visibleSetDirty_ = true;
  rebuildPending_ = true;
}

void LevelRenderer::setRenderDistanceChunks(int radiusChunks) {
  const int clamped = std::clamp(radiusChunks, 2, 32);
  if (renderRadiusChunks_ == clamped) {
    return;
  }
  renderRadiusChunks_ = clamped;
  visibleSetDirty_ = true;
  rebuildPending_ = true;
  frustumDirty_ = true;
}

void LevelRenderer::setCameraPosition(float x, float y, float z) {
  const float dx = cameraX_ - x;
  const float dy = cameraY_ - y;
  const float dz = cameraZ_ - z;
  cameraX_ = x;
  cameraY_ = y;
  cameraZ_ = z;
  if ((dx * dx + dy * dy + dz * dz) > 0.04f) {
    transparentSortPending_ = true;
  }
}

void LevelRenderer::setViewProj(const simd_float4x4& viewProj) {
  // Extract clip planes from row-major form of view-projection matrix.
  const simd_float4 r0{viewProj.columns[0].x, viewProj.columns[1].x, viewProj.columns[2].x, viewProj.columns[3].x};
  const simd_float4 r1{viewProj.columns[0].y, viewProj.columns[1].y, viewProj.columns[2].y, viewProj.columns[3].y};
  const simd_float4 r2{viewProj.columns[0].z, viewProj.columns[1].z, viewProj.columns[2].z, viewProj.columns[3].z};
  const simd_float4 r3{viewProj.columns[0].w, viewProj.columns[1].w, viewProj.columns[2].w, viewProj.columns[3].w};

  frustumPlanes_[0] = r3 + r0;  // left
  frustumPlanes_[1] = r3 - r0;  // right
  frustumPlanes_[2] = r3 + r1;  // bottom
  frustumPlanes_[3] = r3 - r1;  // top
  frustumPlanes_[4] = r3 + r2;  // near
  frustumPlanes_[5] = r3 - r2;  // far

  for (simd_float4& p : frustumPlanes_) {
    const float len = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
    if (len > 1e-6f) {
      p /= len;
    }
  }
  hasFrustum_ = true;
  frustumDirty_ = true;
}

void LevelRenderer::tick() {
  if (rebuildPending_) {
    rebuildPending_ = false;
    bool visibleChanged = false;
    if (visibleSetDirty_) {
      refreshVisibleChunks();
      visibleChanged = true;
    }

    int rebuilt = 0;
    bool rebuiltUrgent = false;
    const int kMaxChunkRebuildPerFrame =
        (dirtyChunks_.size() > 768) ? 6 : ((dirtyChunks_.size() > 256) ? 4 : 2);
    const double kChunkRebuildTimeBudgetMs =
        (dirtyChunks_.size() > 768) ? 2.5 : ((dirtyChunks_.size() > 256) ? 1.8 : 1.2);
    const auto rebuildStart = std::chrono::steady_clock::now();
    const auto rebuildChunkKey = [&](std::int64_t key) {
      dirtyChunks_.erase(key);
      urgentDirtyChunks_.erase(key);
      if (visibleChunks_.find(key) == visibleChunks_.end()) {
        return false;
      }
      const bool firstMeshBuild = (chunkMeshes_.find(key) == chunkMeshes_.end());
      const int chunkX = static_cast<int>(key >> 32);
      const int chunkZ = static_cast<int>(static_cast<std::int32_t>(key & 0xffffffff));
      std::vector<TerrainVertex> chunkVertices;
      std::vector<TerrainVertex> transparentChunkVertices;
      buildChunkMesh(chunkX, chunkZ, chunkVertices, transparentChunkVertices);
      metalRenderer_->upsertChunkMesh(key, chunkVertices, transparentChunkVertices);
      chunkMeshes_[key] = std::move(chunkVertices);
      chunkTransparentMeshes_[key] = std::move(transparentChunkVertices);
      if (firstMeshBuild) {
        // When a chunk becomes available, neighbors can now safely emit border faces
        // that were previously suppressed while adjacency was unknown.
        markChunkAndNeighborsDirty(chunkX, chunkZ, false, false);
      }
      return true;
    };

    // Process direct block-edit chunks first so border-adjacent chunk pairs are
    // rebuilt together, preventing one-frame seam flashes during mining/placing.
    struct DirtyCandidate {
      std::int64_t key = 0;
      int dist2 = 0;
    };
    std::vector<DirtyCandidate> urgentCandidates;
    urgentCandidates.reserve(urgentDirtyChunks_.size());
    for (const std::int64_t key : urgentDirtyChunks_) {
      if (dirtyChunks_.find(key) == dirtyChunks_.end() || visibleChunks_.find(key) == visibleChunks_.end()) {
        continue;
      }
      const int chunkX = static_cast<int>(key >> 32);
      const int chunkZ = static_cast<int>(static_cast<std::int32_t>(key & 0xffffffff));
      const int dx = chunkX - centerChunkX_;
      const int dz = chunkZ - centerChunkZ_;
      urgentCandidates.push_back({key, dx * dx + dz * dz});
    }
    std::sort(urgentCandidates.begin(), urgentCandidates.end(), [](const DirtyCandidate& a, const DirtyCandidate& b) {
      return a.dist2 < b.dist2;
    });
    const bool smallUrgentBatch = urgentCandidates.size() <= 3;
    const int kMaxUrgentRebuildPerFrame = smallUrgentBatch
                                              ? static_cast<int>(urgentCandidates.size())
                                              : ((dirtyChunks_.size() > 768) ? 4 : ((dirtyChunks_.size() > 256) ? 3 : 2));
    const double kUrgentRebuildTimeBudgetMs = smallUrgentBatch
                                                  ? 2.0
                                                  : ((dirtyChunks_.size() > 768) ? 1.0 : ((dirtyChunks_.size() > 256) ? 0.75 : 0.5));
    for (const DirtyCandidate& c : urgentCandidates) {
      if (rebuilt >= kMaxUrgentRebuildPerFrame) {
        break;
      }
      if (!smallUrgentBatch && rebuilt > 0) {
        const auto now = std::chrono::steady_clock::now();
        const double elapsedMs =
            std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(now - rebuildStart).count();
        if (elapsedMs >= kUrgentRebuildTimeBudgetMs) {
          break;
        }
      }
      if (rebuildChunkKey(c.key)) {
        ++rebuilt;
        rebuiltUrgent = true;
      }
    }

    std::vector<DirtyCandidate> candidates;
    candidates.reserve(dirtyChunks_.size());
    for (const std::int64_t key : dirtyChunks_) {
      if (visibleChunks_.find(key) == visibleChunks_.end()) {
        continue;
      }
      const int chunkX = static_cast<int>(key >> 32);
      const int chunkZ = static_cast<int>(static_cast<std::int32_t>(key & 0xffffffff));
      const int dx = chunkX - centerChunkX_;
      const int dz = chunkZ - centerChunkZ_;
      candidates.push_back({key, dx * dx + dz * dz});
    }
    std::sort(candidates.begin(), candidates.end(), [](const DirtyCandidate& a, const DirtyCandidate& b) {
      return a.dist2 < b.dist2;
    });

    for (const DirtyCandidate& c : candidates) {
      if (rebuilt >= kMaxChunkRebuildPerFrame) {
        break;
      }
      if (rebuilt > 0) {
        const auto now = std::chrono::steady_clock::now();
        const double elapsedMs =
            std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(now - rebuildStart).count();
        if (elapsedMs >= kChunkRebuildTimeBudgetMs) {
          break;
        }
      }

      if (rebuildChunkKey(c.key)) {
        ++rebuilt;
      }
    }

    if (!dirtyChunks_.empty()) {
      rebuildPending_ = true;
    }
    bool shouldCompose = false;
    if (visibleChanged) {
      shouldCompose = true;
      composeThrottle_ = 0;
    } else if (rebuiltUrgent || rebuilt > 0) {
      if (rebuiltUrgent) {
        shouldCompose = true;
        composeThrottle_ = 3;
      } else if (dirtyChunks_.empty() || composeThrottle_ <= 0) {
        shouldCompose = true;
        composeThrottle_ = 3;
      } else {
        --composeThrottle_;
      }
    }
    if (shouldCompose) {
      composeTerrainMesh();
    }
  }

  if (frustumDirty_) {
    composeTerrainMesh();
    frustumDirty_ = false;
  }

  if (transparentSortPending_) {
    uploadSortedTransparentMesh();
  }
}

void LevelRenderer::rebuildTerrain() {
  visibleSetDirty_ = true;
  rebuildPending_ = true;
  tick();
}

void LevelRenderer::tileChanged(int x, int /*y*/, int z) {
  const int chunkX = floorDiv16(x);
  const int chunkZ = floorDiv16(z);

  // Mark edited chunk immediately; mark neighbors only when the changed block
  // touches a chunk edge. This avoids unnecessary 5-chunk urgent rebuilds for
  // every block edit and smooths frame-time spikes during mining/placing.
  const int localX = ((x % 16) + 16) % 16;
  const int localZ = ((z % 16) + 16) % 16;

  const std::int64_t center = chunkKey(chunkX, chunkZ);
  dirtyChunks_.insert(center);
  urgentDirtyChunks_.insert(center);
  if (localX == 0) {
    const std::int64_t west = chunkKey(chunkX - 1, chunkZ);
    dirtyChunks_.insert(west);
    urgentDirtyChunks_.insert(west);
  }
  if (localX == 15) {
    const std::int64_t east = chunkKey(chunkX + 1, chunkZ);
    dirtyChunks_.insert(east);
    urgentDirtyChunks_.insert(east);
  }
  if (localZ == 0) {
    const std::int64_t north = chunkKey(chunkX, chunkZ - 1);
    dirtyChunks_.insert(north);
    urgentDirtyChunks_.insert(north);
  }
  if (localZ == 15) {
    const std::int64_t south = chunkKey(chunkX, chunkZ + 1);
    dirtyChunks_.insert(south);
    urgentDirtyChunks_.insert(south);
  }
  rebuildPending_ = true;
}

void LevelRenderer::setTilesDirty(int x0, int /*y0*/, int z0, int x1, int /*y1*/, int z1, Level* /*level*/) {
  const int c0x = floorDiv16(x0);
  const int c0z = floorDiv16(z0);
  const int c1x = floorDiv16(x1);
  const int c1z = floorDiv16(z1);
  for (int cx = c0x - 1; cx <= c1x + 1; ++cx) {
    for (int cz = c0z - 1; cz <= c1z + 1; ++cz) {
      markChunkDirty(cx, cz);
    }
  }
  rebuildPending_ = true;
}

void LevelRenderer::allChanged() {
  chunkMeshes_.clear();
  chunkTransparentMeshes_.clear();
  dirtyChunks_.clear();
  urgentDirtyChunks_.clear();
  metalRenderer_->clearChunkMeshes();
  metalRenderer_->setChunkDrawList({});
  visibleSetDirty_ = true;
  rebuildPending_ = true;
}

int LevelRenderer::textureForTileFace(int tile, Face face) const {
  if (tile == static_cast<int>(TileId::Cactus)) {
    if (face == Face::Top) {
      return kTexCactusTop;
    }
    if (face == Face::Bottom) {
      return kTexCactusBottom;
    }
    return kTexCactusSide;
  }
  if (tile == static_cast<int>(TileId::Ice)) {
    return 67;
  }
  if (tile == static_cast<int>(TileId::Snow)) {
    if (face == Face::Top) {
      return 66;
    }
    if (face == Face::Bottom) {
      return 2;
    }
    return 68;
  }
  if (tile == static_cast<int>(TileId::Glass)) {
    return 49;
  }
  if (tile == static_cast<int>(TileId::Sandstone)) {
    if (face == Face::Top) {
      return 176;
    }
    if (face == Face::Bottom) {
      return 208;
    }
    return 192;
  }
  if (tile == static_cast<int>(TileId::Gravel)) {
    return 19;
  }
  if (tile == static_cast<int>(TileId::Cobblestone)) {
    return 16;
  }
  if (tile == static_cast<int>(TileId::Planks)) {
    return 4;
  }
  if (tile == static_cast<int>(TileId::CoalOre)) {
    return 34;
  }
  if (tile == static_cast<int>(TileId::IronOre)) {
    return 33;
  }
  if (tile == static_cast<int>(TileId::GoldOre)) {
    return 32;
  }
  if (tile == static_cast<int>(TileId::DiamondOre)) {
    return 50;
  }
  if (tile == static_cast<int>(TileId::Clay)) {
    return 72;
  }
  if (tile == static_cast<int>(TileId::Water)) {
    return 205;
  }
  if (tile == static_cast<int>(TileId::Sand)) {
    return 18;
  }
  if (tile == static_cast<int>(TileId::Bedrock)) {
    return 17;
  }
  if (tile == static_cast<int>(TileId::Wood)) {
    return (face == Face::Top || face == Face::Bottom) ? 21 : 20;
  }
  if (tile == static_cast<int>(TileId::SpruceWood)) {
    return (face == Face::Top || face == Face::Bottom) ? 21 : 116;
  }
  if (tile == static_cast<int>(TileId::BirchWood)) {
    return (face == Face::Top || face == Face::Bottom) ? 21 : 117;
  }
  if (tile == static_cast<int>(TileId::Leaves)) {
    // PS3 non-fancy style uses the solid leaf tile to avoid alpha-sorted artifacts.
    return 53;
  }
  if (tile == static_cast<int>(TileId::SpruceLeaves)) {
    return 132;
  }
  if (tile == static_cast<int>(TileId::BirchLeaves)) {
    return 133;
  }
  if (tile == static_cast<int>(TileId::TallGrass)) {
    return 39;
  }
  if (tile == static_cast<int>(TileId::Fern)) {
    return 56;
  }
  if (tile == static_cast<int>(TileId::DeadBush)) {
    return 55;
  }
  if (tile == static_cast<int>(TileId::FlowerYellow)) {
    return 13;
  }
  if (tile == static_cast<int>(TileId::FlowerRed)) {
    return 12;
  }
  if (tile == static_cast<int>(TileId::MushroomBrown)) {
    return 29;
  }
  if (tile == static_cast<int>(TileId::MushroomRed)) {
    return 28;
  }
  if (tile == static_cast<int>(TileId::SugarCane)) {
    return 73;
  }
  if (tile == static_cast<int>(TileId::Stone)) {
    return 1;
  }
  if (tile == static_cast<int>(TileId::Dirt)) {
    return 2;
  }
  if (tile == static_cast<int>(TileId::Grass)) {
    if (face == Face::Top) {
      return 0;
    }
    if (face == Face::Bottom) {
      return 2;
    }
    return 3;
  }
  return 1;
}

void LevelRenderer::buildChunkMesh(int chunkX, int chunkZ, std::vector<TerrainVertex>& opaqueOut, std::vector<TerrainVertex>& transparentOut) {
  using detail::ChunkBuildView;
  using detail::FaceDir;
  using detail::FloraMesher;
  using detail::OpaqueGreedyMesher;
  using detail::TransparentMesher;

  opaqueOut.clear();
  transparentOut.clear();
  if (!level_) {
    return;
  }
  const Level* level = level_;
  const LevelChunk* center = level->getChunk(chunkX, chunkZ);
  if (!center) {
    return;
  }
  const LevelChunk* west = level->getChunk(chunkX - 1, chunkZ);
  const LevelChunk* east = level->getChunk(chunkX + 1, chunkZ);
  const LevelChunk* north = level->getChunk(chunkX, chunkZ - 1);
  const LevelChunk* south = level->getChunk(chunkX, chunkZ + 1);
  opaqueOut.reserve(12000);
  transparentOut.reserve(6000);
  const ChunkBuildView view{
      .center = center,
      .west = west,
      .east = east,
      .north = north,
      .south = south,
      .x0 = chunkX * 16,
      .z0 = chunkZ * 16,
  };
  auto mapFace = [](FaceDir face) {
    switch (face) {
      case FaceDir::Top: return Face::Top;
      case FaceDir::Bottom: return Face::Bottom;
      case FaceDir::North: return Face::North;
      case FaceDir::South: return Face::South;
      case FaceDir::West: return Face::West;
      case FaceDir::East: return Face::East;
    }
    return Face::Top;
  };
  auto textureForFace = [&](int tile, FaceDir face) {
    return textureForTileFace(tile, mapFace(face));
  };
  auto emitTransparentFace = [&](std::vector<TerrainVertex>& out, int worldX, int y, int worldZ, FaceDir face, int textureIndex, float shade,
                                 int tile) {
    const int lx = worldX - view.x0;
    const int lz = worldZ - view.z0;
    const bool waterHasSameAbove =
        (tile == static_cast<int>(TileId::Water)) && (view.tileAt(lx, y + 1, lz) == static_cast<int>(TileId::Water));
    appendFace(out, static_cast<float>(worldX), static_cast<float>(y), static_cast<float>(worldZ), mapFace(face), textureIndex, shade, tile,
               0.0f, true, waterHasSameAbove);
  };
  auto emitOpaqueRect = [&](std::vector<TerrainVertex>& out, FaceDir face, int tile, int tex, float shade, int worldX, int y, int worldZ,
                            int w, int h) {
    const float x = static_cast<float>(worldX);
    const float fy = static_cast<float>(y);
    const float z = static_cast<float>(worldZ);
    const simd_float3 tint = detail::biomeTintForBlock(tile, worldX, worldZ, face == FaceDir::Top);
    const simd_float3 color = {tint.x * shade, tint.y * shade, tint.z * shade};
    TerrainVertex v0{};
    TerrainVertex v1{};
    TerrainVertex v2{};
    TerrainVertex v3{};
    if (face == FaceDir::Top) {
      v0.position = {x, fy + 1.0f, z};
      v1.position = {x + static_cast<float>(w), fy + 1.0f, z};
      v2.position = {x + static_cast<float>(w), fy + 1.0f, z + static_cast<float>(h)};
      v3.position = {x, fy + 1.0f, z + static_cast<float>(h)};
    } else if (face == FaceDir::Bottom) {
      v0.position = {x, fy, z + static_cast<float>(h)};
      v1.position = {x + static_cast<float>(w), fy, z + static_cast<float>(h)};
      v2.position = {x + static_cast<float>(w), fy, z};
      v3.position = {x, fy, z};
    } else if (face == FaceDir::North) {
      v0.position = {x + static_cast<float>(w), fy, z};
      v1.position = {x + static_cast<float>(w), fy + static_cast<float>(h), z};
      v2.position = {x, fy + static_cast<float>(h), z};
      v3.position = {x, fy, z};
    } else if (face == FaceDir::South) {
      v0.position = {x, fy, z + 1.0f};
      v1.position = {x, fy + static_cast<float>(h), z + 1.0f};
      v2.position = {x + static_cast<float>(w), fy + static_cast<float>(h), z + 1.0f};
      v3.position = {x + static_cast<float>(w), fy, z + 1.0f};
    } else if (face == FaceDir::West) {
      v0.position = {x, fy, z};
      v1.position = {x, fy + static_cast<float>(h), z};
      v2.position = {x, fy + static_cast<float>(h), z + static_cast<float>(w)};
      v3.position = {x, fy, z + static_cast<float>(w)};
    } else {
      v0.position = {x + 1.0f, fy, z + static_cast<float>(w)};
      v1.position = {x + 1.0f, fy + static_cast<float>(h), z + static_cast<float>(w)};
      v2.position = {x + 1.0f, fy + static_cast<float>(h), z};
      v3.position = {x + 1.0f, fy, z};
    }
    v0.color = color;
    v1.color = color;
    v2.color = color;
    v3.color = color;
    if (face == FaceDir::Top || face == FaceDir::Bottom) {
      v0.uv = {0.0f, static_cast<float>(h)};
      v1.uv = {static_cast<float>(w), static_cast<float>(h)};
      v2.uv = {static_cast<float>(w), 0.0f};
      v3.uv = {0.0f, 0.0f};
    } else {
      v0.uv = {static_cast<float>(w), static_cast<float>(h)};
      v1.uv = {static_cast<float>(w), 0.0f};
      v2.uv = {0.0f, 0.0f};
      v3.uv = {0.0f, static_cast<float>(h)};
    }
    const simd_float2 tileOrigin = detail::atlasTileOrigin(tex);
    v0.tileOrigin = tileOrigin;
    v1.tileOrigin = tileOrigin;
    v2.tileOrigin = tileOrigin;
    v3.tileOrigin = tileOrigin;
    out.push_back(v0);
    out.push_back(v1);
    out.push_back(v2);
    out.push_back(v0);
    out.push_back(v2);
    out.push_back(v3);
  };
  auto emitCactusFaces = [&](int lx, int y, int lz, int worldX, int worldZ, int tile) {
    struct FaceRule {
      Face renderFace;
      FaceDir texFace;
      int neighborTile;
      float shade;
      bool skip = false;
    };
    const std::array<FaceRule, 6> rules = {{
        {Face::Top, FaceDir::Top, view.tileAt(lx, y + 1, lz), 1.0f, false},
        {Face::Bottom, FaceDir::Bottom, view.tileAt(lx, y - 1, lz), 0.55f,
         (tile == static_cast<int>(TileId::Bedrock) && y == Level::minBuildHeight)},
        {Face::North, FaceDir::North, view.tileAt(lx, y, lz - 1), 0.78f, false},
        {Face::South, FaceDir::South, view.tileAt(lx, y, lz + 1), 0.72f, false},
        {Face::West, FaceDir::West, view.tileAt(lx - 1, y, lz), 0.84f, false},
        {Face::East, FaceDir::East, view.tileAt(lx + 1, y, lz), 0.88f, false},
    }};
    for (const FaceRule& rule : rules) {
      if (rule.skip || !detail::shouldRenderFaceForTile(tile, rule.neighborTile)) {
        continue;
      }
      appendFace(opaqueOut, static_cast<float>(worldX), static_cast<float>(y), static_cast<float>(worldZ), rule.renderFace,
                 textureForFace(tile, rule.texFace), rule.shade, tile);
    }
  };

  FloraMesher{}.build(view, opaqueOut, textureForFace);
  for (int lx = 0; lx < 16; ++lx) {
    for (int lz = 0; lz < 16; ++lz) {
      const int worldX = view.x0 + lx;
      const int worldZ = view.z0 + lz;
      for (int y = Level::minBuildHeight; y < Level::maxBuildHeight; ++y) {
        const int tile = view.center->getTile(lx, y, lz);
        if (tile != static_cast<int>(TileId::Cactus)) {
          continue;
        }
        emitCactusFaces(lx, y, lz, worldX, worldZ, tile);
      }
    }
  }
  TransparentMesher{}.build(view, transparentOut, textureForFace, emitTransparentFace);
  OpaqueGreedyMesher{}.build(view, opaqueOut, textureForFace, emitOpaqueRect);
}

void LevelRenderer::composeTerrainMesh() {
  if (!level_) {
    metalRenderer_->setChunkDrawList({});
    uploadOverlayVertices();
    return;
  }

  std::vector<std::int64_t> drawList;
  drawList.reserve(visibleChunks_.size());
  for (const std::int64_t key : visibleChunks_) {
    const int chunkX = static_cast<int>(key >> 32);
    const int chunkZ = static_cast<int>(static_cast<std::int32_t>(key & 0xffffffff));
    if (!chunkVisibleInFrustum(chunkX, chunkZ)) {
      continue;
    }
    drawList.push_back(key);
  }
  metalRenderer_->setChunkDrawList(drawList);
  uploadOverlayVertices();
}

void LevelRenderer::uploadSortedTransparentMesh() {
  transparentSortPending_ = false;
}

void LevelRenderer::refreshVisibleChunks() {
  visibleSetDirty_ = false;
  visibleChunks_.clear();
  if (!level_) {
    chunkMeshes_.clear();
    chunkTransparentMeshes_.clear();
    dirtyChunks_.clear();
    return;
  }

  for (int cx = centerChunkX_ - renderRadiusChunks_; cx <= centerChunkX_ + renderRadiusChunks_; ++cx) {
    for (int cz = centerChunkZ_ - renderRadiusChunks_; cz <= centerChunkZ_ + renderRadiusChunks_; ++cz) {
      const std::int64_t key = chunkKey(cx, cz);
      visibleChunks_.insert(key);
      if (chunkMeshes_.find(key) == chunkMeshes_.end()) {
        dirtyChunks_.insert(key);
      }
    }
  }

  // Keep built chunk meshes cached even when currently out of view so moving
  // back does not trigger expensive re-meshing and visible popping.
}

bool LevelRenderer::chunkVisibleInFrustum(int chunkX, int chunkZ) const {
  if (!hasFrustum_) {
    return true;
  }

  const float minX = static_cast<float>(chunkX * 16);
  const float maxX = minX + 16.0f;
  const float minY = static_cast<float>(Level::minBuildHeight);
  const float maxY = static_cast<float>(Level::maxBuildHeight);
  const float minZ = static_cast<float>(chunkZ * 16);
  const float maxZ = minZ + 16.0f;

  for (const simd_float4& p : frustumPlanes_) {
    const float px = (p.x >= 0.0f) ? maxX : minX;
    const float py = (p.y >= 0.0f) ? maxY : minY;
    const float pz = (p.z >= 0.0f) ? maxZ : minZ;
    if (p.x * px + p.y * py + p.z * pz + p.w < 0.0f) {
      return false;
    }
  }
  return true;
}

void LevelRenderer::markChunkDirty(int chunkX, int chunkZ) {
  dirtyChunks_.insert(chunkKey(chunkX, chunkZ));
}

void LevelRenderer::markChunkAndNeighborsDirty(int chunkX, int chunkZ, bool urgent, bool includeCenter) {
  const std::int64_t center = chunkKey(chunkX, chunkZ);
  const std::int64_t west = chunkKey(chunkX - 1, chunkZ);
  const std::int64_t east = chunkKey(chunkX + 1, chunkZ);
  const std::int64_t north = chunkKey(chunkX, chunkZ - 1);
  const std::int64_t south = chunkKey(chunkX, chunkZ + 1);
  if (includeCenter) {
    dirtyChunks_.insert(center);
  }
  dirtyChunks_.insert(west);
  dirtyChunks_.insert(east);
  dirtyChunks_.insert(north);
  dirtyChunks_.insert(south);
  if (!urgent) {
    return;
  }
  // Prioritize directly edited chunks to avoid one-frame border holes when a
  // block on a chunk seam is mined or placed.
  if (includeCenter) {
    urgentDirtyChunks_.insert(center);
  }
  urgentDirtyChunks_.insert(west);
  urgentDirtyChunks_.insert(east);
  urgentDirtyChunks_.insert(north);
  urgentDirtyChunks_.insert(south);
}

void LevelRenderer::appendOverlayCube(int x, int y, int z, int textureIndex, float inflate) {
  const float fx = static_cast<float>(x);
  const float fy = static_cast<float>(y);
  const float fz = static_cast<float>(z);
  appendFace(overlayVertices_, fx, fy, fz, Face::Top, textureIndex, 1.0f, -1, inflate, true);
  appendFace(overlayVertices_, fx, fy, fz, Face::Bottom, textureIndex, 1.0f, -1, inflate, true);
  appendFace(overlayVertices_, fx, fy, fz, Face::North, textureIndex, 1.0f, -1, inflate, true);
  appendFace(overlayVertices_, fx, fy, fz, Face::South, textureIndex, 1.0f, -1, inflate, true);
  appendFace(overlayVertices_, fx, fy, fz, Face::West, textureIndex, 1.0f, -1, inflate, true);
  appendFace(overlayVertices_, fx, fy, fz, Face::East, textureIndex, 1.0f, -1, inflate, true);
}

void LevelRenderer::appendFace(std::vector<TerrainVertex>& out, float x, float y, float z, Face face, int textureIndex, float shade,
                               int tile, float inflate, bool stabilizeUvEdges, bool waterHasSameAbove) {
  const bool allowGrassTint = (face == Face::Top);
  const simd_float3 tint = detail::biomeTintForBlock(tile, static_cast<int>(x), static_cast<int>(z), allowGrassTint);
  const simd_float3 baseColor = {tint.x * shade, tint.y * shade, tint.z * shade};
  const simd_float3 color = applyCactusCutoutTint(tile, baseColor);
  const FaceBounds bounds = computeFaceBounds(tile, static_cast<int>(face), x, y, z, inflate, waterHasSameAbove);

  TerrainVertex v0{};
  TerrainVertex v1{};
  TerrainVertex v2{};
  TerrainVertex v3{};

  switch (face) {
    case Face::Top:
      v0.position = {bounds.topX0, bounds.maxY, bounds.topZ0};
      v1.position = {bounds.topX1, bounds.maxY, bounds.topZ0};
      v2.position = {bounds.topX1, bounds.maxY, bounds.topZ1};
      v3.position = {bounds.topX0, bounds.maxY, bounds.topZ1};
      break;
    case Face::Bottom:
      v0.position = {bounds.topX0, bounds.minY, bounds.topZ1};
      v1.position = {bounds.topX1, bounds.minY, bounds.topZ1};
      v2.position = {bounds.topX1, bounds.minY, bounds.topZ0};
      v3.position = {bounds.topX0, bounds.minY, bounds.topZ0};
      break;
    case Face::North:
      v0.position = {bounds.sideX1, bounds.minY, bounds.northZ};
      v1.position = {bounds.sideX1, bounds.maxY, bounds.northZ};
      v2.position = {bounds.sideX0, bounds.maxY, bounds.northZ};
      v3.position = {bounds.sideX0, bounds.minY, bounds.northZ};
      break;
    case Face::South:
      v0.position = {bounds.sideX0, bounds.minY, bounds.southZ};
      v1.position = {bounds.sideX0, bounds.maxY, bounds.southZ};
      v2.position = {bounds.sideX1, bounds.maxY, bounds.southZ};
      v3.position = {bounds.sideX1, bounds.minY, bounds.southZ};
      break;
    case Face::West:
      v0.position = {bounds.westX, bounds.minY, bounds.sideZ0};
      v1.position = {bounds.westX, bounds.maxY, bounds.sideZ0};
      v2.position = {bounds.westX, bounds.maxY, bounds.sideZ1};
      v3.position = {bounds.westX, bounds.minY, bounds.sideZ1};
      break;
    case Face::East:
      v0.position = {bounds.eastX, bounds.minY, bounds.sideZ1};
      v1.position = {bounds.eastX, bounds.maxY, bounds.sideZ1};
      v2.position = {bounds.eastX, bounds.maxY, bounds.sideZ0};
      v3.position = {bounds.eastX, bounds.minY, bounds.sideZ0};
      break;
  }

  v0.color = color;
  v1.color = color;
  v2.color = color;
  v3.color = color;

  const float u0 = 0.0f;
  const float v0uv = 0.0f;
  const float u1 = stabilizeUvEdges ? 0.9995f : 1.0f;
  const float v1uv = stabilizeUvEdges ? 0.9995f : 1.0f;

  switch (face) {
    case Face::Top:
      v0.uv = {u0, v1uv};
      v1.uv = {u1, v1uv};
      v2.uv = {u1, v0uv};
      v3.uv = {u0, v0uv};
      break;
    case Face::Bottom:
      v0.uv = {u0, v1uv};
      v1.uv = {u1, v1uv};
      v2.uv = {u1, v0uv};
      v3.uv = {u0, v0uv};
      break;
    case Face::North:
      v0.uv = {u1, v1uv};
      v1.uv = {u1, v0uv};
      v2.uv = {u0, v0uv};
      v3.uv = {u0, v1uv};
      break;
    case Face::South:
      v0.uv = {u0, v1uv};
      v1.uv = {u0, v0uv};
      v2.uv = {u1, v0uv};
      v3.uv = {u1, v1uv};
      break;
    case Face::West:
      v0.uv = {u0, v1uv};
      v1.uv = {u0, v0uv};
      v2.uv = {u1, v0uv};
      v3.uv = {u1, v1uv};
      break;
    case Face::East:
      v0.uv = {u0, v1uv};
      v1.uv = {u0, v0uv};
      v2.uv = {u1, v0uv};
      v3.uv = {u1, v1uv};
      break;
  }

  const simd_float2 tileOrigin = detail::atlasTileOrigin(textureIndex);
  v0.tileOrigin = tileOrigin;
  v1.tileOrigin = tileOrigin;
  v2.tileOrigin = tileOrigin;
  v3.tileOrigin = tileOrigin;

  out.push_back(v0);
  out.push_back(v1);
  out.push_back(v2);
  out.push_back(v0);
  out.push_back(v2);
  out.push_back(v3);
}

void LevelRenderer::setDestroyProgress(int x, int y, int z, int stage) {
  if (stage < 0 || stage >= 10) {
    clearDestroyProgress();
    return;
  }
  destroyActive_ = true;
  destroyX_ = x;
  destroyY_ = y;
  destroyZ_ = z;
  destroyStage_ = stage;
  uploadOverlayVertices();
}

void LevelRenderer::clearDestroyProgress() {
  if (!destroyActive_ && destroyStage_ < 0) {
    return;
  }
  destroyActive_ = false;
  destroyStage_ = -1;
  uploadOverlayVertices();
}

void LevelRenderer::setSelectionBlock(int x, int y, int z) {
  if (selectionActive_ && selectionX_ == x && selectionY_ == y && selectionZ_ == z) {
    return;
  }
  selectionActive_ = true;
  selectionX_ = x;
  selectionY_ = y;
  selectionZ_ = z;
  uploadOverlayVertices();
}

void LevelRenderer::clearSelectionBlock() {
  if (!selectionActive_) {
    return;
  }
  selectionActive_ = false;
  uploadOverlayVertices();
}

void LevelRenderer::appendSelectionOverlay() {
  if (!selectionActive_ || !level_) {
    return;
  }

  const int tile = level_->getTile(selectionX_, selectionY_, selectionZ_);
  if (!isOverlayRenderableTile(tile)) {
    return;
  }

  // Use a crack tile with slight expansion as a simple "looked-at block" indicator.
  const int selectTexture = 240;
  const float eps = 0.0030f;
  appendOverlayCube(selectionX_, selectionY_, selectionZ_, selectTexture, eps);
}

void LevelRenderer::appendDestroyOverlay() {
  if (!destroyActive_ || destroyStage_ < 0 || !level_) {
    return;
  }

  const int tile = level_->getTile(destroyX_, destroyY_, destroyZ_);
  if (!isOverlayRenderableTile(tile)) {
    return;
  }

  // PS3-style destroy stages are atlas tiles destroy_0..destroy_9 in terrain atlas.
  const int destroyTexture = 240 + destroyStage_;
  // Keep crack overlay visibly in front of block faces at distance/grazing angles.
  const float eps = 0.0030f;
  appendOverlayCube(destroyX_, destroyY_, destroyZ_, destroyTexture, eps);
}

void LevelRenderer::uploadOverlayVertices() {
  overlayVertices_.clear();
  appendSelectionOverlay();
  appendDestroyOverlay();
  metalRenderer_->setTerrainOverlayVertices(overlayVertices_);
}

}  // namespace mc
