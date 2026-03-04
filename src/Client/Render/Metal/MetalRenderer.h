#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <simd/simd.h>

namespace mc {

struct TerrainVertex {
  simd_float3 position;
  simd_float3 color;
  simd_float2 uv;
  simd_float2 tileOrigin;
};

struct TerrainViewParams {
  simd_float4x4 viewProj;
};

class MetalRenderer {
public:
  virtual ~MetalRenderer() = default;
  virtual void upsertChunkMesh(std::int64_t key, const std::vector<TerrainVertex>& opaque,
                               const std::vector<TerrainVertex>& transparent) = 0;
  virtual void removeChunkMesh(std::int64_t key) = 0;
  virtual void clearChunkMeshes() = 0;
  virtual void setChunkDrawList(const std::vector<std::int64_t>& keys) = 0;
  virtual void setTerrainOverlayVertices(const std::vector<TerrainVertex>& vertices) = 0;
  virtual void setDebugLineVertices(const std::vector<TerrainVertex>& vertices) = 0;
  virtual void setViewParams(const TerrainViewParams& params) = 0;
};

}  // namespace mc
