#pragma once

#import <Cocoa/Cocoa.h>

#include <vector>

#include "Client/App/DebugHudFormatter.h"
#include "World/Level/Gen/BiomeProvider.h"

namespace mc {
class Minecraft;
}

namespace mc::app {

class DebugHudController {
public:
  DebugHudController();

  void attachToView(NSView* parentView, NSRect windowFrame);
  void tick(double dtRaw, const Minecraft& game, RenderDebugController::RenderMode renderMode, bool hasLookTarget, int lookX,
            int lookY, int lookZ, NSUInteger opaqueVerts, NSUInteger transparentVerts, double smoothedFrameMs);

private:
  int floorDiv16(int v) const;
  int mod16(int v) const;

  NSView* panel_ = nil;
  NSTextField* shadowLabel_ = nil;
  NSTextField* label_ = nil;
  double accum_ = 0.0;

  gen::BiomeProvider biomeProvider_;
  int cachedBiomeChunkX_;
  int cachedBiomeChunkZ_;
  std::vector<gen::BiomeSample> cachedBiomes_;
};

}  // namespace mc::app
