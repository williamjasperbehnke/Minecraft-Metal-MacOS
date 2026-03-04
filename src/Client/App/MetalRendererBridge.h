#pragma once

#import <Metal/Metal.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "Client/Render/Metal/MetalRenderer.h"

namespace mc::app {

class MetalRendererBridge final : public MetalRenderer {
public:
  struct ChunkGpuMesh {
    id<MTLBuffer> opaqueBuffer = nil;
    NSUInteger opaqueCount = 0;
    id<MTLBuffer> transparentBuffer = nil;
    NSUInteger transparentCount = 0;
  };

  explicit MetalRendererBridge(id<MTLDevice> device);

  void upsertChunkMesh(std::int64_t key, const std::vector<TerrainVertex>& opaque,
                       const std::vector<TerrainVertex>& transparent) override;
  void removeChunkMesh(std::int64_t key) override;
  void clearChunkMeshes() override;
  void setChunkDrawList(const std::vector<std::int64_t>& keys) override;
  void setViewParams(const TerrainViewParams& params) override;
  void setTerrainOverlayVertices(const std::vector<TerrainVertex>& vertices) override;
  void setDebugLineVertices(const std::vector<TerrainVertex>& vertices) override;

  id<MTLBuffer> overlayVertexBuffer() const;
  NSUInteger overlayVertexCount() const;
  id<MTLBuffer> debugLineBuffer() const;
  NSUInteger debugLineCount() const;
  const std::vector<std::int64_t>& drawKeys() const;
  const std::unordered_map<std::int64_t, ChunkGpuMesh>& chunkMeshes() const;
  NSUInteger visibleOpaqueVertexCount() const;
  NSUInteger visibleTransparentVertexCount() const;
  TerrainViewParams params() const;

private:
  id<MTLDevice> device_;
  std::unordered_map<std::int64_t, ChunkGpuMesh> chunkMeshes_;
  std::vector<std::int64_t> drawKeys_;
  std::vector<TerrainVertex> overlayVertices_;
  std::vector<TerrainVertex> debugLineVertices_;
  id<MTLBuffer> overlayVertexBuffer_ = nil;
  id<MTLBuffer> debugLineBuffer_ = nil;
  TerrainViewParams params_{};
};

void unpackChunkKey(std::int64_t key, int* chunkX, int* chunkZ);

}  // namespace mc::app
