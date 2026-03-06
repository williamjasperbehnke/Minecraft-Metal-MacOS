#import "Client/App/TerrainRenderResources.h"

#import <MetalKit/MTKTextureLoader.h>

namespace mc::app {

TerrainRenderResources::TerrainRenderResources(id<MTLDevice> device, MTLPixelFormat colorPixelFormat,
                                               MTLPixelFormat depthPixelFormat)
    : device_(device), colorPixelFormat_(colorPixelFormat), depthPixelFormat_(depthPixelFormat) {
  buildPipelines();
  loadTerrainTexture();
}

void TerrainRenderResources::buildPipelines() {
  NSString* shaderSource = @"using namespace metal;"
    "struct VertexIn { float3 position [[attribute(0)]]; float3 color [[attribute(1)]]; float2 uv [[attribute(2)]]; float2 tileOrigin [[attribute(3)]]; };"
    "struct ViewParams { float4x4 viewProj; };"
    "struct FragmentParams { uint mode; float underwater; };"
    "struct VertexOut { float4 position [[position]]; float3 color; float2 uv; float2 tileOrigin; };"
    "vertex VertexOut v_main(VertexIn in [[stage_in]], constant ViewParams& view [[buffer(1)]]) {"
    "  VertexOut out;"
    "  out.position = view.viewProj * float4(in.position, 1.0);"
    "  out.color = in.color;"
    "  out.uv = in.uv;"
    "  out.tileOrigin = in.tileOrigin;"
    "  return out;"
    "}"
    "fragment float4 f_main(VertexOut in [[stage_in]], constant FragmentParams& fp [[buffer(1)]], texture2d<float> tex [[texture(0)]], sampler samp [[sampler(0)]]) {"
    "  constexpr float kInset = 0.0015;"
    "  const float2 tileSize = float2(1.0 / 16.0, 1.0 / 16.0);"
    "  const bool uvRepeats = any(in.uv > float2(1.001)) || any(in.uv < float2(-0.001));"
    "  const float2 uvWrapped = fract(in.uv + float2(0.0005));"
    "  const float2 uvLocal = uvRepeats ? clamp(uvWrapped, float2(0.0), float2(0.9995)) : clamp(in.uv, float2(0.0), float2(0.9995));"
    "  const float2 atlasUv = in.tileOrigin + float2(kInset, kInset) + uvLocal * (tileSize - float2(kInset * 2.0, kInset * 2.0));"
    "  float4 albedo = (fp.mode == 0u) ? tex.sample(samp, atlasUv) : float4(1.0, 1.0, 1.0, 1.0);"
    "  const bool cutout = in.color.x < 0.0;"
    "  const float3 litColor = float3(abs(in.color.x), in.color.y, in.color.z);"
    "  const bool chromaKeyGreen = (albedo.g > 0.85) && (albedo.r < 0.16) && (albedo.b < 0.16);"
    "  if (cutout && (albedo.a < 0.1 || chromaKeyGreen)) discard_fragment();"
    "  const float outAlpha = cutout ? 1.0 : albedo.a;"
    "  float3 outRgb = albedo.rgb * litColor;"
    "  if (fp.underwater > 0.5) {"
    "    const float3 underwaterTint = float3(0.57, 0.76, 0.96);"
    "    outRgb = mix(outRgb, outRgb * underwaterTint, 0.46);"
    "    outRgb *= 0.90;"
    "  }"
    "  return float4(outRgb, outAlpha);"
    "}";

  NSError* error = nil;
  id<MTLLibrary> library = [device_ newLibraryWithSource:shaderSource options:nil error:&error];
  if (!library) {
    NSLog(@"Shader compile error: %@", error);
    abort();
  }

  MTLRenderPipelineDescriptor* desc = [[MTLRenderPipelineDescriptor alloc] init];
  desc.vertexFunction = [library newFunctionWithName:@"v_main"];
  desc.fragmentFunction = [library newFunctionWithName:@"f_main"];
  desc.colorAttachments[0].pixelFormat = colorPixelFormat_;
  desc.depthAttachmentPixelFormat = depthPixelFormat_;

  MTLVertexDescriptor* vdesc = [[MTLVertexDescriptor alloc] init];
  vdesc.attributes[0].format = MTLVertexFormatFloat3;
  vdesc.attributes[0].offset = 0;
  vdesc.attributes[0].bufferIndex = 0;

  vdesc.attributes[1].format = MTLVertexFormatFloat3;
  vdesc.attributes[1].offset = sizeof(simd_float3);
  vdesc.attributes[1].bufferIndex = 0;

  vdesc.attributes[2].format = MTLVertexFormatFloat2;
  vdesc.attributes[2].offset = sizeof(simd_float3) * 2;
  vdesc.attributes[2].bufferIndex = 0;

  vdesc.attributes[3].format = MTLVertexFormatFloat2;
  vdesc.attributes[3].offset = sizeof(simd_float3) * 2 + sizeof(simd_float2);
  vdesc.attributes[3].bufferIndex = 0;

  vdesc.layouts[0].stride = sizeof(mc::TerrainVertex);
  desc.vertexDescriptor = vdesc;

  desc.colorAttachments[0].blendingEnabled = NO;
  opaquePipeline_ = [device_ newRenderPipelineStateWithDescriptor:desc error:&error];
  if (!opaquePipeline_) {
    NSLog(@"Opaque pipeline error: %@", error);
    abort();
  }

  desc.colorAttachments[0].blendingEnabled = YES;
  desc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
  desc.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
  desc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
  desc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
  desc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
  desc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
  transparentPipeline_ = [device_ newRenderPipelineStateWithDescriptor:desc error:&error];
  if (!transparentPipeline_) {
    NSLog(@"Pipeline error: %@", error);
    abort();
  }

  MTLDepthStencilDescriptor* opaqueDepthDesc = [[MTLDepthStencilDescriptor alloc] init];
  opaqueDepthDesc.depthCompareFunction = MTLCompareFunctionLess;
  opaqueDepthDesc.depthWriteEnabled = YES;
  opaqueDepthState_ = [device_ newDepthStencilStateWithDescriptor:opaqueDepthDesc];

  MTLDepthStencilDescriptor* transparentDepthDesc = [[MTLDepthStencilDescriptor alloc] init];
  transparentDepthDesc.depthCompareFunction = MTLCompareFunctionLessEqual;
  transparentDepthDesc.depthWriteEnabled = NO;
  transparentDepthState_ = [device_ newDepthStencilStateWithDescriptor:transparentDepthDesc];

  MTLSamplerDescriptor* samplerDesc = [[MTLSamplerDescriptor alloc] init];
  samplerDesc.minFilter = MTLSamplerMinMagFilterNearest;
  samplerDesc.magFilter = MTLSamplerMinMagFilterNearest;
  samplerDesc.mipFilter = MTLSamplerMipFilterNotMipmapped;
  samplerDesc.sAddressMode = MTLSamplerAddressModeRepeat;
  samplerDesc.tAddressMode = MTLSamplerAddressModeRepeat;
  terrainSampler_ = [device_ newSamplerStateWithDescriptor:samplerDesc];
}

void TerrainRenderResources::loadTerrainTexture() {
  NSString* bundlePath = [[NSBundle mainBundle] bundlePath];
  NSString* cursor = [bundlePath stringByDeletingLastPathComponent];
  NSFileManager* fm = [NSFileManager defaultManager];
  NSString* foundPath = nil;
  for (int i = 0; i < 10; ++i) {
    NSString* localAssetPath = [cursor stringByAppendingPathComponent:@"MinecraftMetal/Assets/terrain.png"];
    if ([fm fileExistsAtPath:localAssetPath]) {
      foundPath = localAssetPath;
      break;
    }
    NSString* parent = [cursor stringByDeletingLastPathComponent];
    if ([parent isEqualToString:cursor]) {
      break;
    }
    cursor = parent;
  }

  if (!foundPath) {
    NSLog(@"Could not locate terrain texture at MinecraftMetal/Assets/terrain.png");
    return;
  }

  NSError* error = nil;
  MTKTextureLoader* loader = [[MTKTextureLoader alloc] initWithDevice:device_];
  NSDictionary* options = @{
    MTKTextureLoaderOptionSRGB : @NO,
    MTKTextureLoaderOptionOrigin : MTKTextureLoaderOriginTopLeft,
  };

  terrainTexture_ = [loader newTextureWithContentsOfURL:[NSURL fileURLWithPath:foundPath] options:options error:&error];
  if (!terrainTexture_) {
    NSLog(@"Failed to load terrain texture %@: %@", foundPath, error);
  } else {
    NSLog(@"Loaded terrain texture: %@", foundPath);
  }
}

void TerrainRenderResources::bindTerrainTexture(id<MTLRenderCommandEncoder> encoder) const {
  if (!terrainTexture_) {
    return;
  }
  [encoder setFragmentTexture:terrainTexture_ atIndex:0];
  [encoder setFragmentSamplerState:terrainSampler_ atIndex:0];
}

}  // namespace mc::app
