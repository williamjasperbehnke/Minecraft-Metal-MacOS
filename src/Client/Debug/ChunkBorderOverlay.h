#pragma once

#include <vector>

#include "Client/Render/Metal/MetalRenderer.h"

namespace mc {

class ChunkBorderOverlay {
public:
  void build(int centerChunkX, int centerChunkZ, int radiusChunks, std::vector<TerrainVertex>& out) const;
};

}  // namespace mc
