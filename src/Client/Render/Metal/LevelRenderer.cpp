#include "Client/Render/Metal/LevelRenderer.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>

#include "Client/Render/BlockRender.h"
#include "Client/Render/Metal/ChunkMeshers.h"
#include "World/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Tile/Tile.h"

namespace mc {

namespace {

constexpr float kParticleTickSeconds = 1.0f / 60.0f;

struct DirtyCandidate {
  std::int64_t key = 0;
  int dist2 = 0;
};

int floorDiv16(int v) {
  return v >= 0 ? v / 16 : (v - 15) / 16;
}

int chunkXFromKey(std::int64_t key) {
  return static_cast<int>(key >> 32);
}

int chunkZFromKey(std::int64_t key) {
  return static_cast<int>(static_cast<std::int32_t>(key & 0xffffffff));
}

bool isOverlayRenderableTile(int tile) {
  return tile > static_cast<int>(TileId::Air) && tile != static_cast<int>(TileId::Water);
}

bool usesBiomeTintForBreakingParticles(int tile) {
  return tile == static_cast<int>(TileId::Grass) || tile == static_cast<int>(TileId::Leaves) ||
         tile == static_cast<int>(TileId::SpruceLeaves) || tile == static_cast<int>(TileId::BirchLeaves) ||
         tile == static_cast<int>(TileId::TallGrass) || tile == static_cast<int>(TileId::Fern);
}

simd_float3 breakParticleTintForFace(int tile, int x, int z, int faceOrdinal) {
  if (tile == static_cast<int>(TileId::Grass)) {
    // Match Minecraft/PS3 behavior: only grass top gets biome tint.
    return (faceOrdinal == 0) ? detail::biomeTintForBlock(tile, x, z, true) : simd_float3{1.0f, 1.0f, 1.0f};
  }
  if (usesBiomeTintForBreakingParticles(tile)) {
    return detail::biomeTintForBlock(tile, x, z, true);
  }
  return {1.0f, 1.0f, 1.0f};
}

simd_float3 applyCactusCutoutTint(int tile, simd_float3 color) {
  if (render::isCactusRenderTile(tile)) {
    // Negative red channel marks cutout geometry in the fragment shader.
    color.x = -std::abs(color.x);
  }
  return color;
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
  breakParticles_.clear();
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
  const bool particlesChanged = tickBreakParticles();

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
      const int chunkX = chunkXFromKey(key);
      const int chunkZ = chunkZFromKey(key);
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

    auto collectCandidates = [&](const std::unordered_set<std::int64_t>& source, bool requireDirty) {
      std::vector<DirtyCandidate> out;
      out.reserve(source.size());
      for (const std::int64_t key : source) {
        if (requireDirty && dirtyChunks_.find(key) == dirtyChunks_.end()) {
          continue;
        }
        if (visibleChunks_.find(key) == visibleChunks_.end()) {
          continue;
        }
        const int dx = chunkXFromKey(key) - centerChunkX_;
        const int dz = chunkZFromKey(key) - centerChunkZ_;
        out.push_back({key, dx * dx + dz * dz});
      }
      std::sort(out.begin(), out.end(), [](const DirtyCandidate& a, const DirtyCandidate& b) {
        return a.dist2 < b.dist2;
      });
      return out;
    };

    // Process direct block-edit chunks first so border-adjacent chunk pairs are
    // rebuilt together, preventing one-frame seam flashes during mining/placing.
    std::vector<DirtyCandidate> urgentCandidates = collectCandidates(urgentDirtyChunks_, true);
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

    std::vector<DirtyCandidate> candidates = collectCandidates(dirtyChunks_, false);

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

  if (particlesChanged) {
    uploadOverlayVertices();
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

  insertDirtyChunk(chunkKey(chunkX, chunkZ), true);
  if (localX == 0) {
    insertDirtyChunk(chunkKey(chunkX - 1, chunkZ), true);
  }
  if (localX == 15) {
    insertDirtyChunk(chunkKey(chunkX + 1, chunkZ), true);
  }
  if (localZ == 0) {
    insertDirtyChunk(chunkKey(chunkX, chunkZ - 1), true);
  }
  if (localZ == 15) {
    insertDirtyChunk(chunkKey(chunkX, chunkZ + 1), true);
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
  breakParticles_.clear();
  metalRenderer_->clearChunkMeshes();
  metalRenderer_->setChunkDrawList({});
  visibleSetDirty_ = true;
  rebuildPending_ = true;
}

void LevelRenderer::buildChunkMesh(int chunkX, int chunkZ, std::vector<TerrainVertex>& opaqueOut, std::vector<TerrainVertex>& transparentOut) {
  using detail::ChunkBuildView;
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
  auto textureForFace = [&](int tile, render::BlockFace face) {
    return render::textureForTileFace(tile, face);
  };
  auto emitTransparentFace = [&](std::vector<TerrainVertex>& out, int worldX, int y, int worldZ, render::BlockFace face, int textureIndex, float shade,
                                 int tile) {
    const int lx = worldX - view.x0;
    const int lz = worldZ - view.z0;
    const bool waterHasSameAbove =
        (tile == static_cast<int>(TileId::Water)) && (view.tileAt(lx, y + 1, lz) == static_cast<int>(TileId::Water));
    appendFace(out, static_cast<float>(worldX), static_cast<float>(y), static_cast<float>(worldZ), face, textureIndex, shade, tile, 0.0f, true,
               waterHasSameAbove);
  };
  auto emitOpaqueRect = [&](std::vector<TerrainVertex>& out, render::BlockFace face, int tile, int tex, float shade, int worldX, int y, int worldZ,
                            int w, int h) {
    const float x = static_cast<float>(worldX);
    const float fy = static_cast<float>(y);
    const float z = static_cast<float>(worldZ);
    const simd_float3 tint = detail::biomeTintForBlock(tile, worldX, worldZ, face == render::BlockFace::Top);
    const simd_float3 color = {tint.x * shade, tint.y * shade, tint.z * shade};
    TerrainVertex v0{};
    TerrainVertex v1{};
    TerrainVertex v2{};
    TerrainVertex v3{};
    if (face == render::BlockFace::Top) {
      v0.position = {x, fy + 1.0f, z};
      v1.position = {x + static_cast<float>(w), fy + 1.0f, z};
      v2.position = {x + static_cast<float>(w), fy + 1.0f, z + static_cast<float>(h)};
      v3.position = {x, fy + 1.0f, z + static_cast<float>(h)};
    } else if (face == render::BlockFace::Bottom) {
      v0.position = {x, fy, z + static_cast<float>(h)};
      v1.position = {x + static_cast<float>(w), fy, z + static_cast<float>(h)};
      v2.position = {x + static_cast<float>(w), fy, z};
      v3.position = {x, fy, z};
    } else if (face == render::BlockFace::North) {
      v0.position = {x + static_cast<float>(w), fy, z};
      v1.position = {x + static_cast<float>(w), fy + static_cast<float>(h), z};
      v2.position = {x, fy + static_cast<float>(h), z};
      v3.position = {x, fy, z};
    } else if (face == render::BlockFace::South) {
      v0.position = {x, fy, z + 1.0f};
      v1.position = {x, fy + static_cast<float>(h), z + 1.0f};
      v2.position = {x + static_cast<float>(w), fy + static_cast<float>(h), z + 1.0f};
      v3.position = {x + static_cast<float>(w), fy, z + 1.0f};
    } else if (face == render::BlockFace::West) {
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
    if (face == render::BlockFace::Top || face == render::BlockFace::Bottom) {
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
      render::BlockFace face;
      int neighborTile;
      float shade;
      bool skip = false;
    };
    const std::array<FaceRule, 6> rules = {{
        {render::BlockFace::Top, view.tileAt(lx, y + 1, lz), 1.0f, false},
        {render::BlockFace::Bottom, view.tileAt(lx, y - 1, lz), 0.55f,
         (tile == static_cast<int>(TileId::Bedrock) && y == Level::minBuildHeight)},
        {render::BlockFace::North, view.tileAt(lx, y, lz - 1), 0.78f, false},
        {render::BlockFace::South, view.tileAt(lx, y, lz + 1), 0.72f, false},
        {render::BlockFace::West, view.tileAt(lx - 1, y, lz), 0.84f, false},
        {render::BlockFace::East, view.tileAt(lx + 1, y, lz), 0.88f, false},
    }};
    for (const FaceRule& rule : rules) {
      if (rule.skip || !detail::shouldRenderFaceForTile(tile, rule.neighborTile)) {
        continue;
      }
      appendFace(opaqueOut, static_cast<float>(worldX), static_cast<float>(y), static_cast<float>(worldZ), rule.face,
                 textureForFace(tile, rule.face), rule.shade, tile);
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
    const int chunkX = chunkXFromKey(key);
    const int chunkZ = chunkZFromKey(key);
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

  // Keep built chunk meshes cached outside the active visible set.
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
  insertDirtyChunk(chunkKey(chunkX, chunkZ), false);
}

void LevelRenderer::insertDirtyChunk(std::int64_t key, bool urgent) {
  dirtyChunks_.insert(key);
  if (urgent) {
    urgentDirtyChunks_.insert(key);
  }
}

void LevelRenderer::markChunkAndNeighborsDirty(int chunkX, int chunkZ, bool urgent, bool includeCenter) {
  const std::int64_t center = chunkKey(chunkX, chunkZ);
  const std::int64_t west = chunkKey(chunkX - 1, chunkZ);
  const std::int64_t east = chunkKey(chunkX + 1, chunkZ);
  const std::int64_t north = chunkKey(chunkX, chunkZ - 1);
  const std::int64_t south = chunkKey(chunkX, chunkZ + 1);
  if (includeCenter) {
    insertDirtyChunk(center, urgent);
  }
  insertDirtyChunk(west, urgent);
  insertDirtyChunk(east, urgent);
  insertDirtyChunk(north, urgent);
  insertDirtyChunk(south, urgent);
}

void LevelRenderer::appendOverlayCube(int x, int y, int z, int textureIndex, float inflate) {
  const float fx = static_cast<float>(x);
  const float fy = static_cast<float>(y);
  const float fz = static_cast<float>(z);
  appendFace(overlayVertices_, fx, fy, fz, render::BlockFace::Top, textureIndex, 1.0f, -1, inflate, true);
  appendFace(overlayVertices_, fx, fy, fz, render::BlockFace::Bottom, textureIndex, 1.0f, -1, inflate, true);
  appendFace(overlayVertices_, fx, fy, fz, render::BlockFace::North, textureIndex, 1.0f, -1, inflate, true);
  appendFace(overlayVertices_, fx, fy, fz, render::BlockFace::South, textureIndex, 1.0f, -1, inflate, true);
  appendFace(overlayVertices_, fx, fy, fz, render::BlockFace::West, textureIndex, 1.0f, -1, inflate, true);
  appendFace(overlayVertices_, fx, fy, fz, render::BlockFace::East, textureIndex, 1.0f, -1, inflate, true);
}

void LevelRenderer::appendFace(std::vector<TerrainVertex>& out, float x, float y, float z, render::BlockFace face, int textureIndex, float shade,
                               int tile, float inflate, bool stabilizeUvEdges, bool waterHasSameAbove) {
  const bool allowGrassTint = (face == render::BlockFace::Top);
  const simd_float3 tint = detail::biomeTintForBlock(tile, static_cast<int>(x), static_cast<int>(z), allowGrassTint);
  const simd_float3 baseColor = {tint.x * shade, tint.y * shade, tint.z * shade};
  const simd_float3 color = applyCactusCutoutTint(tile, baseColor);
  const render::FaceBounds bounds = render::computeFaceBounds(tile, static_cast<render::BlockFace>(face), x, y, z, inflate, waterHasSameAbove);

  TerrainVertex v0{};
  TerrainVertex v1{};
  TerrainVertex v2{};
  TerrainVertex v3{};

  switch (face) {
    case render::BlockFace::Top:
      v0.position = {bounds.topX0, bounds.maxY, bounds.topZ0};
      v1.position = {bounds.topX1, bounds.maxY, bounds.topZ0};
      v2.position = {bounds.topX1, bounds.maxY, bounds.topZ1};
      v3.position = {bounds.topX0, bounds.maxY, bounds.topZ1};
      break;
    case render::BlockFace::Bottom:
      v0.position = {bounds.topX0, bounds.minY, bounds.topZ1};
      v1.position = {bounds.topX1, bounds.minY, bounds.topZ1};
      v2.position = {bounds.topX1, bounds.minY, bounds.topZ0};
      v3.position = {bounds.topX0, bounds.minY, bounds.topZ0};
      break;
    case render::BlockFace::North:
      v0.position = {bounds.sideX1, bounds.minY, bounds.northZ};
      v1.position = {bounds.sideX1, bounds.maxY, bounds.northZ};
      v2.position = {bounds.sideX0, bounds.maxY, bounds.northZ};
      v3.position = {bounds.sideX0, bounds.minY, bounds.northZ};
      break;
    case render::BlockFace::South:
      v0.position = {bounds.sideX0, bounds.minY, bounds.southZ};
      v1.position = {bounds.sideX0, bounds.maxY, bounds.southZ};
      v2.position = {bounds.sideX1, bounds.maxY, bounds.southZ};
      v3.position = {bounds.sideX1, bounds.minY, bounds.southZ};
      break;
    case render::BlockFace::West:
      v0.position = {bounds.westX, bounds.minY, bounds.sideZ0};
      v1.position = {bounds.westX, bounds.maxY, bounds.sideZ0};
      v2.position = {bounds.westX, bounds.maxY, bounds.sideZ1};
      v3.position = {bounds.westX, bounds.minY, bounds.sideZ1};
      break;
    case render::BlockFace::East:
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
    case render::BlockFace::Top:
      v0.uv = {u0, v1uv};
      v1.uv = {u1, v1uv};
      v2.uv = {u1, v0uv};
      v3.uv = {u0, v0uv};
      break;
    case render::BlockFace::Bottom:
      v0.uv = {u0, v1uv};
      v1.uv = {u1, v1uv};
      v2.uv = {u1, v0uv};
      v3.uv = {u0, v0uv};
      break;
    case render::BlockFace::North:
      v0.uv = {u1, v1uv};
      v1.uv = {u1, v0uv};
      v2.uv = {u0, v0uv};
      v3.uv = {u0, v1uv};
      break;
    case render::BlockFace::South:
      v0.uv = {u0, v1uv};
      v1.uv = {u0, v0uv};
      v2.uv = {u1, v0uv};
      v3.uv = {u1, v1uv};
      break;
    case render::BlockFace::West:
      v0.uv = {u0, v1uv};
      v1.uv = {u0, v0uv};
      v2.uv = {u1, v0uv};
      v3.uv = {u1, v1uv};
      break;
    case render::BlockFace::East:
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

BreakingParticles::SpawnContext LevelRenderer::makeBreakParticleSpawnContext(int x, int y, int z, int tile) const {
  BreakingParticles::SpawnContext ctx;
  ctx.x = x;
  ctx.y = y;
  ctx.z = z;
  ctx.tile = tile;
  const std::array<render::BlockFace, 6> faces = {
      render::BlockFace::Top, render::BlockFace::Bottom, render::BlockFace::North,
      render::BlockFace::South, render::BlockFace::West,  render::BlockFace::East,
  };
  for (std::size_t i = 0; i < faces.size(); ++i) {
    ctx.faceTextures[i] = render::textureForTileFace(tile, faces[i]);
    ctx.faceTints[i] = breakParticleTintForFace(tile, x, z, static_cast<int>(faces[i]));
  }
  return ctx;
}

void LevelRenderer::spawnBreakParticles(int x, int y, int z, int tile) {
  if (!isOverlayRenderableTile(tile) || tile == static_cast<int>(TileId::Bedrock)) {
    return;
  }
  const BreakingParticles::SpawnContext ctx = makeBreakParticleSpawnContext(x, y, z, tile);
  breakParticles_.spawnBreakBurst(ctx);
  uploadOverlayVertices();
}

void LevelRenderer::spawnMiningParticles(int x, int y, int z, int prevX, int prevY, int prevZ, int tile) {
  if (!isOverlayRenderableTile(tile) || tile == static_cast<int>(TileId::Bedrock)) {
    return;
  }
  const BreakingParticles::SpawnContext ctx = makeBreakParticleSpawnContext(x, y, z, tile);

  bool spawnedAny = false;
  auto tryFace = [&](int nx, int ny, int nz) {
    if (!level_) {
      return;
    }
    const int neighborTile = level_->getTile(nx, ny, nz);
    if (!isSolidTileId(neighborTile)) {
      breakParticles_.spawnMiningChip(ctx, nx, ny, nz);
      spawnedAny = true;
    }
  };

  // Emit chips from every exposed face, matching classic debris behavior.
  tryFace(x + 1, y, z);
  tryFace(x - 1, y, z);
  tryFace(x, y + 1, z);
  tryFace(x, y - 1, z);
  tryFace(x, y, z + 1);
  tryFace(x, y, z - 1);

  // Fallback for fully enclosed blocks.
  if (!spawnedAny) {
    breakParticles_.spawnMiningChip(ctx, prevX, prevY, prevZ);
  }
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

void LevelRenderer::appendBreakParticlesOverlay() {
  breakParticles_.appendVertices(overlayVertices_, {cameraX_, cameraY_, cameraZ_});
}

bool LevelRenderer::tickBreakParticles() {
  return breakParticles_.tick(kParticleTickSeconds);
}

void LevelRenderer::uploadOverlayVertices() {
  overlayVertices_.clear();
  appendSelectionOverlay();
  appendDestroyOverlay();
  appendBreakParticlesOverlay();
  metalRenderer_->setTerrainOverlayVertices(overlayVertices_);
}

}  // namespace mc
