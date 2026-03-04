#import "Client/App/AppWorldRenderer.h"

#include <algorithm>

#include "Client/App/MetalRendererBridge.h"
#include "Client/App/TerrainRenderResources.h"
#include "Client/Core/Minecraft.h"
#include "Client/Debug/ChunkBorderOverlay.h"
#include "Client/Debug/RenderDebugController.h"

namespace mc::app {

namespace {

constexpr double kTransparentSortIntervalSeconds = 0.066;

}  // namespace

void AppWorldRenderer::appendSelectionWireCube(std::vector<TerrainVertex>& out, int x, int y, int z) {
  const float minX = static_cast<float>(x) - 0.001f;
  const float minY = static_cast<float>(y) - 0.001f;
  const float minZ = static_cast<float>(z) - 0.001f;
  const float maxX = static_cast<float>(x + 1) + 0.001f;
  const float maxY = static_cast<float>(y + 1) + 0.001f;
  const float maxZ = static_cast<float>(z + 1) + 0.001f;
  const simd_float3 color = {0.0f, 0.0f, 0.0f};

  auto vtx = [&](float px, float py, float pz) {
    TerrainVertex v{};
    v.position = {px, py, pz};
    v.color = color;
    v.uv = {0.0f, 0.0f};
    v.tileOrigin = {0.0f, 0.0f};
    return v;
  };
  auto addEdge = [&](float ax, float ay, float az, float bx, float by, float bz) {
    out.push_back(vtx(ax, ay, az));
    out.push_back(vtx(bx, by, bz));
  };

  addEdge(minX, minY, minZ, maxX, minY, minZ);
  addEdge(maxX, minY, minZ, maxX, minY, maxZ);
  addEdge(maxX, minY, maxZ, minX, minY, maxZ);
  addEdge(minX, minY, maxZ, minX, minY, minZ);

  addEdge(minX, maxY, minZ, maxX, maxY, minZ);
  addEdge(maxX, maxY, minZ, maxX, maxY, maxZ);
  addEdge(maxX, maxY, maxZ, minX, maxY, maxZ);
  addEdge(minX, maxY, maxZ, minX, maxY, minZ);

  addEdge(minX, minY, minZ, minX, maxY, minZ);
  addEdge(maxX, minY, minZ, maxX, maxY, minZ);
  addEdge(maxX, minY, maxZ, maxX, maxY, maxZ);
  addEdge(minX, minY, maxZ, minX, maxY, maxZ);
}

AppWorldRenderResult AppWorldRenderer::render(MTKView* view, id<MTLCommandQueue> queue,
                                              const TerrainRenderResources& resources, MetalRendererBridge& bridge,
                                              Minecraft& game, const RenderDebugController* debugController,
                                              ChunkBorderOverlay* borderOverlay, double dtRaw) {
  AppWorldRenderResult result;
  if (!queue || !resources.opaquePipeline() || !resources.transparentPipeline()) {
    return result;
  }

  MTLRenderPassDescriptor* pass = view.currentRenderPassDescriptor;
  id<CAMetalDrawable> drawable = view.currentDrawable;
  if (!pass || !drawable) {
    return result;
  }

  const TerrainViewParams params = bridge.params();
  const RenderDebugController::RenderMode renderMode =
      debugController ? debugController->renderMode() : RenderDebugController::RenderMode::Textured;
  const FragmentMode fragmentMode =
      (renderMode == RenderDebugController::RenderMode::Textured) ? FragmentMode::Textured : FragmentMode::Flat;
  const FragmentParams fragmentParams{
      .mode = static_cast<uint32_t>(fragmentMode),
      .underwater = game.isCameraUnderwater() ? 1.0f : 0.0f,
  };
  const bool cameraUnderwater = fragmentParams.underwater > 0.5f;
  const MTLTriangleFillMode fillMode =
      (renderMode == RenderDebugController::RenderMode::Wireframe) ? MTLTriangleFillModeLines : MTLTriangleFillModeFill;

  if (borderOverlay && debugController && debugController->showChunkBorders()) {
    borderOverlay->build(game.renderCenterChunkX(), game.renderCenterChunkZ(), game.renderDistanceChunks(), debugLineVertices_);
  } else {
    debugLineVertices_.clear();
  }

  result.hasLookTarget = game.lookTargetBlock(&result.lookX, &result.lookY, &result.lookZ);
  if (result.hasLookTarget) {
    appendSelectionWireCube(debugLineVertices_, result.lookX, result.lookY, result.lookZ);
  }
  bridge.setDebugLineVertices(debugLineVertices_);

  result.opaqueVertices = bridge.visibleOpaqueVertexCount();
  result.transparentVertices = bridge.visibleTransparentVertexCount();

  id<MTLCommandBuffer> cmd = [queue commandBuffer];
  id<MTLRenderCommandEncoder> enc = [cmd renderCommandEncoderWithDescriptor:pass];
  const auto meshForKey = [&](std::int64_t key) -> const MetalRendererBridge::ChunkGpuMesh* {
    const auto it = bridge.chunkMeshes().find(key);
    if (it == bridge.chunkMeshes().end()) {
      return nullptr;
    }
    return &it->second;
  };

  resources.bindTerrainTexture(enc);
  auto bindCommonFrameState = [&](const FragmentParams& fp) {
    [enc setFrontFacingWinding:MTLWindingClockwise];
    [enc setTriangleFillMode:fillMode];
    [enc setFragmentBytes:&fp length:sizeof(FragmentParams) atIndex:1];
    [enc setVertexBytes:&params length:sizeof(TerrainViewParams) atIndex:1];
  };

  [enc setRenderPipelineState:resources.opaquePipeline()];
  [enc setDepthStencilState:resources.opaqueDepthState()];
  [enc setCullMode:MTLCullModeBack];
  bindCommonFrameState(fragmentParams);
  for (const std::int64_t key : bridge.drawKeys()) {
    const MetalRendererBridge::ChunkGpuMesh* mesh = meshForKey(key);
    if (!mesh) {
      continue;
    }
    if (mesh->opaqueBuffer && mesh->opaqueCount > 0) {
      [enc setVertexBuffer:mesh->opaqueBuffer offset:0 atIndex:0];
      [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:mesh->opaqueCount];
    }
  }

  [enc setRenderPipelineState:resources.transparentPipeline()];
  [enc setDepthStencilState:resources.transparentDepthState()];
  [enc setCullMode:(cameraUnderwater ? MTLCullModeNone : MTLCullModeBack)];
  bindCommonFrameState(fragmentParams);

  transparentSortAccum_ += dtRaw;
  if (transparentSortAccum_ >= kTransparentSortIntervalSeconds || transparentDrawOrder_.empty()) {
    transparentSortAccum_ = 0.0;
    struct ChunkSort {
      std::int64_t key = 0;
      float dist2 = 0.0f;
    };
    std::vector<ChunkSort> transparentOrder;
    transparentOrder.reserve(bridge.drawKeys().size());
    const simd_float3 cam = game.cameraWorldPosition();
    const float camX = cam.x;
    const float camZ = cam.z;
    for (const std::int64_t key : bridge.drawKeys()) {
      const MetalRendererBridge::ChunkGpuMesh* mesh = meshForKey(key);
      if (!mesh || mesh->transparentCount == 0 || !mesh->transparentBuffer) {
        continue;
      }
      int chunkX = 0;
      int chunkZ = 0;
      unpackChunkKey(key, &chunkX, &chunkZ);
      const float cx = static_cast<float>(chunkX * 16 + 8);
      const float cz = static_cast<float>(chunkZ * 16 + 8);
      const float dx = cx - camX;
      const float dz = cz - camZ;
      transparentOrder.push_back({key, dx * dx + dz * dz});
    }
    std::sort(transparentOrder.begin(), transparentOrder.end(), [](const ChunkSort& a, const ChunkSort& b) {
      return a.dist2 > b.dist2;
    });
    transparentDrawOrder_.clear();
    transparentDrawOrder_.reserve(transparentOrder.size());
    for (const ChunkSort& ord : transparentOrder) {
      transparentDrawOrder_.push_back(ord.key);
    }
  }

  for (const std::int64_t key : transparentDrawOrder_) {
    const MetalRendererBridge::ChunkGpuMesh* mesh = meshForKey(key);
    if (!mesh) {
      continue;
    }
    [enc setVertexBuffer:mesh->transparentBuffer offset:0 atIndex:0];
    [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:mesh->transparentCount];
  }

  id<MTLBuffer> ovb = bridge.overlayVertexBuffer();
  const NSUInteger ocount = bridge.overlayVertexCount();
  if (ovb && ocount > 0) {
    [enc setRenderPipelineState:resources.transparentPipeline()];
    [enc setDepthStencilState:resources.transparentDepthState()];
    [enc setCullMode:MTLCullModeBack];
    bindCommonFrameState(fragmentParams);
    [enc setVertexBuffer:ovb offset:0 atIndex:0];
    [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:ocount];
  }

  id<MTLBuffer> dlb = bridge.debugLineBuffer();
  const NSUInteger dlc = bridge.debugLineCount();
  if (dlb && dlc > 0) {
    const FragmentParams debugParams{
        .mode = static_cast<uint32_t>(FragmentMode::Flat),
        .underwater = fragmentParams.underwater,
    };
    [enc setRenderPipelineState:resources.transparentPipeline()];
    [enc setDepthStencilState:resources.transparentDepthState()];
    [enc setCullMode:MTLCullModeNone];
    [enc setTriangleFillMode:MTLTriangleFillModeFill];
    [enc setFragmentBytes:&debugParams length:sizeof(FragmentParams) atIndex:1];
    [enc setVertexBuffer:dlb offset:0 atIndex:0];
    [enc setVertexBytes:&params length:sizeof(TerrainViewParams) atIndex:1];
    [enc drawPrimitives:MTLPrimitiveTypeLine vertexStart:0 vertexCount:dlc];
  }

  [enc endEncoding];
  [cmd presentDrawable:drawable];
  [cmd commit];
  return result;
}

}  // namespace mc::app
