#pragma once

#import <Metal/Metal.h>

#include "Client/Render/Metal/MetalRenderer.h"

namespace mc::app {

class TerrainRenderResources {
public:
  TerrainRenderResources(id<MTLDevice> device, MTLPixelFormat colorPixelFormat, MTLPixelFormat depthPixelFormat);

  id<MTLRenderPipelineState> opaquePipeline() const { return opaquePipeline_; }
  id<MTLRenderPipelineState> transparentPipeline() const { return transparentPipeline_; }
  id<MTLDepthStencilState> opaqueDepthState() const { return opaqueDepthState_; }
  id<MTLDepthStencilState> transparentDepthState() const { return transparentDepthState_; }

  void bindTerrainTexture(id<MTLRenderCommandEncoder> encoder) const;

private:
  void buildPipelines();
  void loadTerrainTexture();

  id<MTLDevice> device_;
  MTLPixelFormat colorPixelFormat_;
  MTLPixelFormat depthPixelFormat_;

  id<MTLRenderPipelineState> opaquePipeline_ = nil;
  id<MTLRenderPipelineState> transparentPipeline_ = nil;
  id<MTLDepthStencilState> opaqueDepthState_ = nil;
  id<MTLDepthStencilState> transparentDepthState_ = nil;
  id<MTLSamplerState> terrainSampler_ = nil;
  id<MTLTexture> terrainTexture_ = nil;
};

}  // namespace mc::app
