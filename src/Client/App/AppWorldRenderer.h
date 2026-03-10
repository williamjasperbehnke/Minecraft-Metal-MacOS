#pragma once

#import <MetalKit/MetalKit.h>

#include <cstdint>
#include <vector>

#include "Client/Render/Metal/MetalRenderer.h"

namespace mc {
class Minecraft;
class ChunkBorderOverlay;
class RenderDebugController;
}

namespace mc::app {

class MetalRendererBridge;
class TerrainRenderResources;

struct AppWorldRenderResult {
  bool hasLookTarget = false;
  int lookX = 0;
  int lookY = 0;
  int lookZ = 0;
  NSUInteger opaqueVertices = 0;
  NSUInteger transparentVertices = 0;
};

class AppWorldRenderer {
public:
  AppWorldRenderResult render(MTKView* view, id<MTLCommandQueue> queue, const TerrainRenderResources& resources,
                              MetalRendererBridge& bridge, Minecraft& game,
                              const RenderDebugController* debugController, ChunkBorderOverlay* borderOverlay,
                              double dtRaw);

private:
  enum class FragmentMode : uint32_t {
    Textured = 0,
    Flat = 1,
  };
  struct FragmentParams {
    uint32_t mode = 0;
    float underwater = 0.0f;
  };

  static void appendWireAabb(std::vector<TerrainVertex>& out, float minX, float minY, float minZ, float maxX, float maxY, float maxZ,
                             simd_float3 color);
  static void appendSelectionWireCube(std::vector<TerrainVertex>& out, int x, int y, int z);

  double transparentSortAccum_ = 0.0;
  std::vector<std::int64_t> transparentDrawOrder_;
  std::vector<TerrainVertex> debugLineVertices_;
};

}  // namespace mc::app
