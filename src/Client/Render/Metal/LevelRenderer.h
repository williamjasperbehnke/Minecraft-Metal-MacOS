#pragma once

#include <array>
#include <cstdint>
#include <simd/simd.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Client/Render/BlockRender.h"
#include "Client/Render/Metal/MetalRenderer.h"
#include "Client/Render/Particles/BreakingParticles.h"
#include "World/Level/LevelListener.h"

namespace mc {

class Level;

class LevelRenderer : public LevelListener {
public:
  struct DroppedItemVisual {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float bob = 0.0f;
    float yawRadians = 0.0f;
    int tile = 0;
    bool underwater = false;
  };

  explicit LevelRenderer(MetalRenderer* metalRenderer);

  void setLevel(Level* level);
  void setRenderCenter(int x, int z);
  void setRenderDistanceChunks(int radiusChunks);
  void setCameraPosition(float x, float y, float z);
  void setViewProj(const simd_float4x4& viewProj);
  void tick();
  void rebuildTerrain();
  void setDestroyProgress(int x, int y, int z, int stage);
  void clearDestroyProgress();
  void setSelectionBlock(int x, int y, int z);
  void clearSelectionBlock();
  void spawnMiningParticles(int x, int y, int z, int prevX, int prevY, int prevZ, int tile);
  void spawnBreakParticles(int x, int y, int z, int tile);
  void setDroppedItems(const std::vector<DroppedItemVisual>& droppedItems);

  void tileChanged(int x, int y, int z) override;
  void setTilesDirty(int x0, int y0, int z0, int x1, int y1, int z1, Level* level) override;
  void allChanged() override;

private:
  void appendFace(std::vector<TerrainVertex>& out, float x, float y, float z, render::BlockFace face, int textureIndex, float shade, int tile,
                  float inflate = 0.0f, bool stabilizeUvEdges = false, bool waterHasSameAbove = false);
  void buildChunkMesh(int chunkX, int chunkZ, std::vector<TerrainVertex>& opaqueOut, std::vector<TerrainVertex>& transparentOut);
  void composeTerrainMesh();
  void uploadSortedTransparentMesh();
  void refreshVisibleChunks();
  void markChunkDirty(int chunkX, int chunkZ);
  void markChunkAndNeighborsDirty(int chunkX, int chunkZ, bool urgent, bool includeCenter = true);
  void insertDirtyChunk(std::int64_t key, bool urgent);
  static std::int64_t chunkKey(int chunkX, int chunkZ);
  void appendDestroyOverlay();
  void appendSelectionOverlay();
  void appendBreakParticlesOverlay();
  void appendDroppedItemsOverlay();
  BreakingParticles::SpawnContext makeBreakParticleSpawnContext(int x, int y, int z, int tile) const;
  void appendOverlayCube(int x, int y, int z, int textureIndex, float inflate);
  void uploadOverlayVertices();
  bool tickBreakParticles();
  bool chunkVisibleInFrustum(int chunkX, int chunkZ) const;

  MetalRenderer* metalRenderer_;
  Level* level_ = nullptr;
  std::vector<TerrainVertex> overlayVertices_;
  std::vector<TerrainVertex> preTransparentOverlayVertices_;
  int centerChunkX_ = 0;
  int centerChunkZ_ = 0;
  int renderRadiusChunks_ = 16;
  bool centerInitialized_ = false;
  bool rebuildPending_ = false;
  bool visibleSetDirty_ = true;
  std::unordered_map<std::int64_t, std::vector<TerrainVertex>> chunkMeshes_;
  std::unordered_map<std::int64_t, std::vector<TerrainVertex>> chunkTransparentMeshes_;
  std::unordered_set<std::int64_t> visibleChunks_;
  std::unordered_set<std::int64_t> dirtyChunks_;
  std::unordered_set<std::int64_t> urgentDirtyChunks_;
  float cameraX_ = 0.0f;
  float cameraY_ = 0.0f;
  float cameraZ_ = 0.0f;
  bool transparentSortPending_ = true;
  int composeThrottle_ = 0;
  bool frustumDirty_ = true;
  bool hasFrustum_ = false;
  std::array<simd_float4, 6> frustumPlanes_{};

  bool destroyActive_ = false;
  int destroyX_ = 0;
  int destroyY_ = 0;
  int destroyZ_ = 0;
  int destroyStage_ = -1;

  bool selectionActive_ = false;
  int selectionX_ = 0;
  int selectionY_ = 0;
  int selectionZ_ = 0;
  BreakingParticles breakParticles_;
  std::vector<DroppedItemVisual> droppedItems_;
};

}  // namespace mc
