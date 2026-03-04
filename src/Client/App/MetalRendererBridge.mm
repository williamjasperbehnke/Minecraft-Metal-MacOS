#import "Client/App/MetalRendererBridge.h"

namespace mc::app {

namespace {

id<MTLBuffer> makeSharedVertexBuffer(id<MTLDevice> device, const std::vector<TerrainVertex>& vertices) {
  if (vertices.empty()) {
    return nil;
  }
  const NSUInteger sizeBytes = static_cast<NSUInteger>(vertices.size() * sizeof(TerrainVertex));
  return [device newBufferWithBytes:vertices.data() length:sizeBytes options:MTLResourceStorageModeShared];
}

template <typename CounterFn>
NSUInteger countVisibleVertices(const std::vector<std::int64_t>& drawKeys,
                                const std::unordered_map<std::int64_t, MetalRendererBridge::ChunkGpuMesh>& chunkMeshes,
                                CounterFn&& counter) {
  NSUInteger total = 0;
  for (const std::int64_t key : drawKeys) {
    const auto it = chunkMeshes.find(key);
    if (it != chunkMeshes.end()) {
      total += counter(it->second);
    }
  }
  return total;
}

}  // namespace

MetalRendererBridge::MetalRendererBridge(id<MTLDevice> device) : device_(device) {}

void MetalRendererBridge::upsertChunkMesh(std::int64_t key, const std::vector<TerrainVertex>& opaque,
                                          const std::vector<TerrainVertex>& transparent) {
  ChunkGpuMesh& mesh = chunkMeshes_[key];
  mesh.opaqueCount = static_cast<NSUInteger>(opaque.size());
  mesh.transparentCount = static_cast<NSUInteger>(transparent.size());
  mesh.opaqueBuffer = makeSharedVertexBuffer(device_, opaque);
  mesh.transparentBuffer = makeSharedVertexBuffer(device_, transparent);
}

void MetalRendererBridge::removeChunkMesh(std::int64_t key) {
  chunkMeshes_.erase(key);
}

void MetalRendererBridge::clearChunkMeshes() {
  chunkMeshes_.clear();
  drawKeys_.clear();
}

void MetalRendererBridge::setChunkDrawList(const std::vector<std::int64_t>& keys) {
  drawKeys_ = keys;
}

void MetalRendererBridge::setViewParams(const TerrainViewParams& params) {
  params_ = params;
}

void MetalRendererBridge::setTerrainOverlayVertices(const std::vector<TerrainVertex>& vertices) {
  overlayVertices_ = vertices;
  overlayVertexBuffer_ = makeSharedVertexBuffer(device_, overlayVertices_);
}

void MetalRendererBridge::setDebugLineVertices(const std::vector<TerrainVertex>& vertices) {
  debugLineVertices_ = vertices;
  debugLineBuffer_ = makeSharedVertexBuffer(device_, debugLineVertices_);
}

id<MTLBuffer> MetalRendererBridge::overlayVertexBuffer() const {
  return overlayVertexBuffer_;
}

NSUInteger MetalRendererBridge::overlayVertexCount() const {
  return static_cast<NSUInteger>(overlayVertices_.size());
}

id<MTLBuffer> MetalRendererBridge::debugLineBuffer() const {
  return debugLineBuffer_;
}

NSUInteger MetalRendererBridge::debugLineCount() const {
  return static_cast<NSUInteger>(debugLineVertices_.size());
}

const std::vector<std::int64_t>& MetalRendererBridge::drawKeys() const {
  return drawKeys_;
}

const std::unordered_map<std::int64_t, MetalRendererBridge::ChunkGpuMesh>& MetalRendererBridge::chunkMeshes() const {
  return chunkMeshes_;
}

NSUInteger MetalRendererBridge::visibleOpaqueVertexCount() const {
  return countVisibleVertices(drawKeys_, chunkMeshes_, [](const ChunkGpuMesh& mesh) { return mesh.opaqueCount; });
}

NSUInteger MetalRendererBridge::visibleTransparentVertexCount() const {
  return countVisibleVertices(drawKeys_, chunkMeshes_, [](const ChunkGpuMesh& mesh) { return mesh.transparentCount; });
}

TerrainViewParams MetalRendererBridge::params() const {
  return params_;
}

void unpackChunkKey(std::int64_t key, int* chunkX, int* chunkZ) {
  *chunkX = static_cast<int>(key >> 32);
  *chunkZ = static_cast<int>(static_cast<std::int32_t>(key & 0xffffffff));
}

}  // namespace mc::app
