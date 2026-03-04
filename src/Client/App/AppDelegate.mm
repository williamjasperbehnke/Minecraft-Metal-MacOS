#import "Client/App/AppDelegate.h"

#import <ApplicationServices/ApplicationServices.h>
#import <Metal/Metal.h>
#import <MetalKit/MTKTextureLoader.h>

#include <algorithm>
#include <unordered_map>

#include "Client/Debug/ChunkBorderOverlay.h"
#include "Client/Debug/RenderDebugController.h"
#include "Client/Core/Minecraft.h"
#include "Client/Render/Metal/MetalRenderer.h"
#include "World/Level/Level.h"

@interface AppDelegate ()
- (void)installInputHandlers;
- (BOOL)updateMovementKeyFromEvent:(NSEvent*)event isPressed:(BOOL)isPressed;
- (void)handleBuildClickAtWindowPoint:(NSPoint)windowPoint place:(BOOL)place;
- (void)resetInputStateForFocusLoss;
- (void)loadTerrainTexture;
- (void)enableRelativeMouse;
- (void)disableRelativeMouse;
- (void)centerCursorInWindow;
@end

namespace {

enum class FragmentMode : uint32_t {
  Textured = 0,
  Flat = 1,
};

constexpr double kPlaceRepeatInterval = 0.16;

const char* renderModeString(mc::RenderDebugController::RenderMode mode) {
  switch (mode) {
    case mc::RenderDebugController::RenderMode::Textured: return "TEXTURED";
    case mc::RenderDebugController::RenderMode::Flat: return "FLAT";
    case mc::RenderDebugController::RenderMode::Wireframe: return "WIREFRAME";
  }
  return "TEXTURED";
}

const char* gameModeString(const mc::Minecraft& game) {
  if (game.isSpectatorMode()) {
    return "SPECTATOR";
  }
  if (game.isCreativeMode()) {
    return "CREATIVE";
  }
  return "SURVIVAL";
}

void unpackChunkKey(std::int64_t key, int* chunkX, int* chunkZ) {
  *chunkX = static_cast<int>(key >> 32);
  *chunkZ = static_cast<int>(static_cast<std::int32_t>(key & 0xffffffff));
}

class MetalRendererImpl final : public mc::MetalRenderer {
public:
  struct ChunkGpuMesh {
    id<MTLBuffer> opaqueBuffer = nil;
    NSUInteger opaqueCount = 0;
    id<MTLBuffer> transparentBuffer = nil;
    NSUInteger transparentCount = 0;
  };

  explicit MetalRendererImpl(id<MTLDevice> device) : device_(device) {}

  void upsertChunkMesh(std::int64_t key, const std::vector<mc::TerrainVertex>& opaque,
                       const std::vector<mc::TerrainVertex>& transparent) override {
    ChunkGpuMesh& mesh = chunkMeshes_[key];
    mesh.opaqueCount = static_cast<NSUInteger>(opaque.size());
    mesh.transparentCount = static_cast<NSUInteger>(transparent.size());
    if (mesh.opaqueCount > 0) {
      const NSUInteger sizeBytes = static_cast<NSUInteger>(opaque.size() * sizeof(mc::TerrainVertex));
      mesh.opaqueBuffer = [device_ newBufferWithBytes:opaque.data()
                                               length:sizeBytes
                                              options:MTLResourceStorageModeShared];
    } else {
      mesh.opaqueBuffer = nil;
    }
    if (mesh.transparentCount > 0) {
      const NSUInteger sizeBytes = static_cast<NSUInteger>(transparent.size() * sizeof(mc::TerrainVertex));
      mesh.transparentBuffer = [device_ newBufferWithBytes:transparent.data()
                                                    length:sizeBytes
                                                   options:MTLResourceStorageModeShared];
    } else {
      mesh.transparentBuffer = nil;
    }
  }

  void removeChunkMesh(std::int64_t key) override {
    chunkMeshes_.erase(key);
  }

  void clearChunkMeshes() override {
    chunkMeshes_.clear();
    drawKeys_.clear();
  }

  void setChunkDrawList(const std::vector<std::int64_t>& keys) override {
    drawKeys_ = keys;
  }

  void setViewParams(const mc::TerrainViewParams& params) override {
    params_ = params;
  }

  void setTerrainOverlayVertices(const std::vector<mc::TerrainVertex>& vertices) override {
    overlayVertices_ = vertices;
    if (overlayVertices_.empty()) {
      overlayVertexBuffer_ = nil;
      return;
    }

    const NSUInteger sizeBytes = static_cast<NSUInteger>(overlayVertices_.size() * sizeof(mc::TerrainVertex));
    overlayVertexBuffer_ = [device_ newBufferWithBytes:overlayVertices_.data()
                                                length:sizeBytes
                                               options:MTLResourceStorageModeShared];
  }

  void setDebugLineVertices(const std::vector<mc::TerrainVertex>& vertices) override {
    debugLineVertices_ = vertices;
    if (debugLineVertices_.empty()) {
      debugLineBuffer_ = nil;
      return;
    }

    const NSUInteger sizeBytes = static_cast<NSUInteger>(debugLineVertices_.size() * sizeof(mc::TerrainVertex));
    debugLineBuffer_ = [device_ newBufferWithBytes:debugLineVertices_.data()
                                            length:sizeBytes
                                           options:MTLResourceStorageModeShared];
  }

  id<MTLBuffer> overlayVertexBuffer() const { return overlayVertexBuffer_; }
  NSUInteger overlayVertexCount() const { return static_cast<NSUInteger>(overlayVertices_.size()); }
  id<MTLBuffer> debugLineBuffer() const { return debugLineBuffer_; }
  NSUInteger debugLineCount() const { return static_cast<NSUInteger>(debugLineVertices_.size()); }
  const std::vector<std::int64_t>& drawKeys() const { return drawKeys_; }
  const std::unordered_map<std::int64_t, ChunkGpuMesh>& chunkMeshes() const { return chunkMeshes_; }
  NSUInteger visibleOpaqueVertexCount() const {
    NSUInteger total = 0;
    for (const std::int64_t key : drawKeys_) {
      auto it = chunkMeshes_.find(key);
      if (it != chunkMeshes_.end()) {
        total += it->second.opaqueCount;
      }
    }
    return total;
  }
  NSUInteger visibleTransparentVertexCount() const {
    NSUInteger total = 0;
    for (const std::int64_t key : drawKeys_) {
      auto it = chunkMeshes_.find(key);
      if (it != chunkMeshes_.end()) {
        total += it->second.transparentCount;
      }
    }
    return total;
  }
  mc::TerrainViewParams params() const { return params_; }

private:
  id<MTLDevice> device_;
  std::unordered_map<std::int64_t, ChunkGpuMesh> chunkMeshes_;
  std::vector<std::int64_t> drawKeys_;
  std::vector<mc::TerrainVertex> overlayVertices_;
  std::vector<mc::TerrainVertex> debugLineVertices_;
  id<MTLBuffer> overlayVertexBuffer_ = nil;
  id<MTLBuffer> debugLineBuffer_ = nil;
  mc::TerrainViewParams params_{};
};

void appendSelectionWireCube(std::vector<mc::TerrainVertex>& out, int x, int y, int z) {
  const float minX = static_cast<float>(x) - 0.001f;
  const float minY = static_cast<float>(y) - 0.001f;
  const float minZ = static_cast<float>(z) - 0.001f;
  const float maxX = static_cast<float>(x + 1) + 0.001f;
  const float maxY = static_cast<float>(y + 1) + 0.001f;
  const float maxZ = static_cast<float>(z + 1) + 0.001f;
  const simd_float3 color = {0.0f, 0.0f, 0.0f};

  auto vtx = [&](float px, float py, float pz) {
    mc::TerrainVertex v{};
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

  // Bottom ring.
  addEdge(minX, minY, minZ, maxX, minY, minZ);
  addEdge(maxX, minY, minZ, maxX, minY, maxZ);
  addEdge(maxX, minY, maxZ, minX, minY, maxZ);
  addEdge(minX, minY, maxZ, minX, minY, minZ);
  // Top ring.
  addEdge(minX, maxY, minZ, maxX, maxY, minZ);
  addEdge(maxX, maxY, minZ, maxX, maxY, maxZ);
  addEdge(maxX, maxY, maxZ, minX, maxY, maxZ);
  addEdge(minX, maxY, maxZ, minX, maxY, minZ);
  // Verticals.
  addEdge(minX, minY, minZ, minX, maxY, minZ);
  addEdge(maxX, minY, minZ, maxX, maxY, minZ);
  addEdge(maxX, minY, maxZ, maxX, maxY, maxZ);
  addEdge(minX, minY, maxZ, minX, maxY, maxZ);
}

}  // namespace

@interface CrosshairView : NSView
@end

@implementation CrosshairView

- (instancetype)initWithFrame:(NSRect)frameRect {
  self = [super initWithFrame:frameRect];
  if (self) {
    [self setWantsLayer:YES];
    self.layer.backgroundColor = NSColor.clearColor.CGColor;
  }
  return self;
}

- (BOOL)isOpaque {
  return NO;
}

- (void)drawRect:(NSRect)dirtyRect {
  (void)dirtyRect;
  [[NSColor colorWithWhite:1.0 alpha:0.95] setStroke];

  NSBezierPath* path = [NSBezierPath bezierPath];
  path.lineWidth = 2.0;

  const NSRect b = self.bounds;
  const CGFloat cx = NSMidX(b);
  const CGFloat cy = NSMidY(b);
  const CGFloat arm = 8.0;
  const CGFloat gap = 3.0;

  [path moveToPoint:NSMakePoint(cx - arm, cy)];
  [path lineToPoint:NSMakePoint(cx - gap, cy)];
  [path moveToPoint:NSMakePoint(cx + gap, cy)];
  [path lineToPoint:NSMakePoint(cx + arm, cy)];
  [path moveToPoint:NSMakePoint(cx, cy - arm)];
  [path lineToPoint:NSMakePoint(cx, cy - gap)];
  [path moveToPoint:NSMakePoint(cx, cy + gap)];
  [path lineToPoint:NSMakePoint(cx, cy + arm)];
  [path stroke];
}

@end

@implementation AppDelegate {
  NSWindow* _window;
  MTKView* _mtkView;

  id<MTLDevice> _device;
  id<MTLCommandQueue> _queue;
  id<MTLRenderPipelineState> _opaquePipeline;
  id<MTLRenderPipelineState> _transparentPipeline;
  id<MTLDepthStencilState> _opaqueDepthState;
  id<MTLDepthStencilState> _transparentDepthState;
  id<MTLSamplerState> _terrainSampler;
  id<MTLTexture> _terrainTexture;

  std::unique_ptr<mc::Minecraft> _game;
  std::unique_ptr<MetalRendererImpl> _rendererBridge;
  std::unique_ptr<mc::RenderDebugController> _debugController;
  std::unique_ptr<mc::ChunkBorderOverlay> _chunkBorderOverlay;
  std::vector<mc::TerrainVertex> _debugLineVertices;
  CrosshairView* _crosshairView;
  NSTextField* _debugLabel;

  id _inputMonitor;
  BOOL _moveForward;
  BOOL _moveBackward;
  BOOL _moveLeft;
  BOOL _moveRight;
  BOOL _jump;
  BOOL _sprintHeld;
  BOOL _sprintLatched;
  BOOL _crouch;
  BOOL _relativeMouseEnabled;
  BOOL _leftMouseHeld;
  BOOL _rightMouseHeld;
  CFAbsoluteTime _lastForwardTapTime;
  double _placeRepeatAccumulator;

  CFAbsoluteTime _lastFrameTime;
  double _smoothedFrameMs;
  double _debugHudAccum;
  double _transparentSortAccum;
  std::vector<std::int64_t> _transparentDrawOrder;
}

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
  (void)notification;

  _device = MTLCreateSystemDefaultDevice();
  _queue = [_device newCommandQueue];

  NSRect frame = NSMakeRect(0, 0, 1280, 720);
  _window = [[NSWindow alloc] initWithContentRect:frame
                                         styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable)
                                           backing:NSBackingStoreBuffered
                                             defer:NO];
  _window.delegate = self;
  [_window setTitle:@"MinecraftMetal"];
  [_window setAcceptsMouseMovedEvents:YES];

  _mtkView = [[MTKView alloc] initWithFrame:frame device:_device];
  _mtkView.clearColor = MTLClearColorMake(0.63, 0.79, 0.96, 1.0);
  _mtkView.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
  _mtkView.depthStencilPixelFormat = MTLPixelFormatDepth32Float;
  _mtkView.delegate = self;
  _mtkView.preferredFramesPerSecond = 60;

  [_window setContentView:_mtkView];
  _crosshairView = [[CrosshairView alloc] initWithFrame:_mtkView.bounds];
  _crosshairView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
  [_mtkView addSubview:_crosshairView];
  _debugLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(10, frame.size.height - 96, 520, 86)];
  _debugLabel.autoresizingMask = NSViewMaxXMargin | NSViewMinYMargin;
  _debugLabel.editable = NO;
  _debugLabel.bordered = NO;
  _debugLabel.drawsBackground = NO;
  _debugLabel.font = [NSFont monospacedSystemFontOfSize:12 weight:NSFontWeightMedium];
  _debugLabel.textColor = [NSColor colorWithWhite:1.0 alpha:0.92];
  _debugLabel.stringValue = @"";
  [_mtkView addSubview:_debugLabel];
  [_window makeKeyAndOrderFront:nil];
  [_window makeFirstResponder:_mtkView];

  _rendererBridge = std::make_unique<MetalRendererImpl>(_device);
  _debugController = std::make_unique<mc::RenderDebugController>();
  _chunkBorderOverlay = std::make_unique<mc::ChunkBorderOverlay>();
  _game = std::make_unique<mc::Minecraft>();
  _game->init(_rendererBridge.get());

  [self buildPipeline];
  [self loadTerrainTexture];
  [self installInputHandlers];
  [self enableRelativeMouse];

  _lastFrameTime = CFAbsoluteTimeGetCurrent();
  _smoothedFrameMs = 16.67;
  _debugHudAccum = 0.0;
  _transparentSortAccum = 0.0;
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
  (void)sender;
  return YES;
}

- (void)resetInputStateForFocusLoss {
  _leftMouseHeld = NO;
  _rightMouseHeld = NO;
  _sprintHeld = NO;
  _sprintLatched = NO;
  _crouch = NO;
  _placeRepeatAccumulator = 0.0;
  if (_game) {
    _game->setBreakHeld(false);
  }
}

- (void)applicationWillResignActive:(NSNotification*)notification {
  (void)notification;
  [self resetInputStateForFocusLoss];
  [self disableRelativeMouse];
}

- (void)applicationDidBecomeActive:(NSNotification*)notification {
  (void)notification;
  [self enableRelativeMouse];
}

- (void)windowDidBecomeKey:(NSNotification*)notification {
  (void)notification;
  [self enableRelativeMouse];
}

- (void)windowDidResignKey:(NSNotification*)notification {
  (void)notification;
  [self resetInputStateForFocusLoss];
  [self disableRelativeMouse];
}

- (void)applicationWillTerminate:(NSNotification*)notification {
  (void)notification;
  if (_inputMonitor) {
    [NSEvent removeMonitor:_inputMonitor];
    _inputMonitor = nil;
  }
  [self disableRelativeMouse];
}

- (void)enableRelativeMouse {
  if (_relativeMouseEnabled) {
    return;
  }
  CGAssociateMouseAndMouseCursorPosition(false);
  [NSCursor hide];
  _relativeMouseEnabled = YES;
  [self centerCursorInWindow];
}

- (void)disableRelativeMouse {
  if (!_relativeMouseEnabled) {
    return;
  }
  CGAssociateMouseAndMouseCursorPosition(true);
  [NSCursor unhide];
  _relativeMouseEnabled = NO;
}

- (void)centerCursorInWindow {
  if (!_window) {
    return;
  }
  NSRect frame = [_window frame];
  NSPoint center = NSMakePoint(NSMidX(frame), NSMidY(frame));
  CGWarpMouseCursorPosition(CGPointMake(center.x, center.y));
}

- (void)installInputHandlers {
  _inputMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:(NSEventMaskLeftMouseDown |
                                                                 NSEventMaskLeftMouseUp |
                                                                 NSEventMaskRightMouseDown |
                                                                 NSEventMaskRightMouseUp |
                                                                 NSEventMaskKeyDown |
                                                                 NSEventMaskKeyUp |
                                                                 NSEventMaskFlagsChanged |
                                                                 NSEventMaskMouseMoved |
                                                                 NSEventMaskLeftMouseDragged |
                                                                 NSEventMaskRightMouseDragged)
                                                         handler:^NSEvent* _Nullable(NSEvent* event) {
    AppDelegate* strongSelf = self;
    if (!strongSelf->_window || !strongSelf->_game) {
      return event;
    }

    if (event.window != strongSelf->_window) {
      return event;
    }

    if (event.type == NSEventTypeMouseMoved ||
        event.type == NSEventTypeLeftMouseDragged ||
        event.type == NSEventTypeRightMouseDragged) {
      strongSelf->_game->addLookInput(static_cast<float>(event.deltaX), static_cast<float>(event.deltaY));
      return event;
    }

    if (event.type == NSEventTypeLeftMouseDown) {
      strongSelf->_leftMouseHeld = YES;
      strongSelf->_game->setBreakHeld(true);
      return nil;
    }

    if (event.type == NSEventTypeLeftMouseUp) {
      strongSelf->_leftMouseHeld = NO;
      strongSelf->_game->setBreakHeld(false);
      return nil;
    }

    if (event.type == NSEventTypeRightMouseDown) {
      strongSelf->_rightMouseHeld = YES;
      strongSelf->_placeRepeatAccumulator = 0.0;
      [strongSelf handleBuildClickAtWindowPoint:event.locationInWindow place:YES];
      return nil;
    }

    if (event.type == NSEventTypeRightMouseUp) {
      strongSelf->_rightMouseHeld = NO;
      strongSelf->_placeRepeatAccumulator = 0.0;
      return nil;
    }

    if (event.type == NSEventTypeKeyDown || event.type == NSEventTypeKeyUp) {
      if ([strongSelf updateMovementKeyFromEvent:event isPressed:(event.type == NSEventTypeKeyDown)]) {
        return nil;
      }
      return event;
    }

    if (event.type == NSEventTypeFlagsChanged) {
      const NSEventModifierFlags f = (event.modifierFlags & NSEventModifierFlagDeviceIndependentFlagsMask);
      strongSelf->_crouch = ((f & NSEventModifierFlagShift) != 0);
      strongSelf->_sprintHeld = ((f & NSEventModifierFlagControl) != 0);
      if (strongSelf->_crouch) {
        strongSelf->_sprintLatched = NO;
      }
      return nil;
    }

    return event;
  }];
}

- (BOOL)updateMovementKeyFromEvent:(NSEvent*)event isPressed:(BOOL)isPressed {
  switch (event.keyCode) {
    case 13:  // W
    case 126: // Up
      _moveForward = isPressed;
      if (isPressed && !event.isARepeat) {
        const CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
        if ((now - _lastForwardTapTime) <= 0.28) {
          _sprintLatched = YES;
        }
        _lastForwardTapTime = now;
      } else if (!isPressed) {
        _sprintLatched = NO;
      }
      return YES;
    case 1:   // S
    case 125: // Down
      _moveBackward = isPressed;
      if (isPressed) {
        _sprintLatched = NO;
      }
      return YES;
    case 0:   // A
    case 123: // Left
      _moveLeft = isPressed;
      return YES;
    case 2:   // D
    case 124: // Right
      _moveRight = isPressed;
      return YES;
    case 49:  // Space
      _jump = isPressed;
      return YES;
    case 56:  // Left Shift
    case 60:  // Right Shift
      _crouch = isPressed;
      if (isPressed) {
        _sprintLatched = NO;
      }
      return YES;
    case 59:  // Left Control
    case 62:  // Right Control
      _sprintHeld = isPressed;
      return YES;
    case 11:  // B
      if (isPressed && !event.isARepeat && _debugController) {
        _debugController->toggleChunkBorders();
      }
      return YES;
    case 46:  // M
      if (isPressed && !event.isARepeat && _debugController) {
        _debugController->cycleRenderMode();
      }
      return YES;
    case 5:  // G
      if (isPressed && !event.isARepeat && _game) {
        _game->toggleCreativeMode();
      }
      return YES;
    case 9:  // V
      if (isPressed && !event.isARepeat && _game) {
        _game->toggleSpectatorMode();
      }
      return YES;
    default:
      return NO;
  }
}

- (void)handleBuildClickAtWindowPoint:(NSPoint)windowPoint place:(BOOL)place {
  if (!_mtkView || !_game) {
    return;
  }

  (void)windowPoint;
  _game->interactAtCrosshair(place);
}

- (void)buildPipeline {
  NSString* shaderSource = @"using namespace metal;"
    "struct VertexIn { float3 position [[attribute(0)]]; float3 color [[attribute(1)]]; float2 uv [[attribute(2)]]; float2 tileOrigin [[attribute(3)]]; };"
    "struct ViewParams { float4x4 viewProj; };"
    "struct FragmentParams { uint mode; };"
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
    "  constexpr float kInset = 0.001;"
    "  const float2 tileSize = float2(1.0 / 16.0, 1.0 / 16.0);"
    "  const bool uvRepeats = any(in.uv > float2(1.001)) || any(in.uv < float2(-0.001));"
    "  const float2 uvLocal = uvRepeats ? fract(in.uv) : clamp(in.uv, float2(0.0), float2(0.9995));"
    "  const float2 atlasUv = in.tileOrigin + float2(kInset, kInset) + uvLocal * (tileSize - float2(kInset * 2.0, kInset * 2.0));"
    "  float4 albedo = (fp.mode == 0u) ? tex.sample(samp, atlasUv) : float4(1.0, 1.0, 1.0, 1.0);"
    "  const bool cutout = in.color.x < 0.0;"
    "  const float3 litColor = float3(abs(in.color.x), in.color.y, in.color.z);"
    "  if (cutout && albedo.a < 0.1) discard_fragment();"
    "  const float outAlpha = cutout ? 1.0 : albedo.a;"
    "  return float4(albedo.rgb * litColor, outAlpha);"
    "}";

  NSError* error = nil;
  id<MTLLibrary> library = [_device newLibraryWithSource:shaderSource options:nil error:&error];
  if (!library) {
    NSLog(@"Shader compile error: %@", error);
    abort();
  }

  MTLRenderPipelineDescriptor* desc = [[MTLRenderPipelineDescriptor alloc] init];
  desc.vertexFunction = [library newFunctionWithName:@"v_main"];
  desc.fragmentFunction = [library newFunctionWithName:@"f_main"];
  desc.colorAttachments[0].pixelFormat = _mtkView.colorPixelFormat;
  desc.depthAttachmentPixelFormat = _mtkView.depthStencilPixelFormat;
  desc.colorAttachments[0].blendingEnabled = YES;
  desc.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
  desc.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
  desc.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
  desc.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
  desc.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
  desc.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

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
  _opaquePipeline = [_device newRenderPipelineStateWithDescriptor:desc error:&error];
  if (!_opaquePipeline) {
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
  _transparentPipeline = [_device newRenderPipelineStateWithDescriptor:desc error:&error];
  if (!_transparentPipeline) {
    NSLog(@"Pipeline error: %@", error);
    abort();
  }

  MTLDepthStencilDescriptor* opaqueDepthDesc = [[MTLDepthStencilDescriptor alloc] init];
  opaqueDepthDesc.depthCompareFunction = MTLCompareFunctionLess;
  opaqueDepthDesc.depthWriteEnabled = YES;
  _opaqueDepthState = [_device newDepthStencilStateWithDescriptor:opaqueDepthDesc];

  MTLDepthStencilDescriptor* transparentDepthDesc = [[MTLDepthStencilDescriptor alloc] init];
  transparentDepthDesc.depthCompareFunction = MTLCompareFunctionLessEqual;
  transparentDepthDesc.depthWriteEnabled = NO;
  _transparentDepthState = [_device newDepthStencilStateWithDescriptor:transparentDepthDesc];

  MTLSamplerDescriptor* samplerDesc = [[MTLSamplerDescriptor alloc] init];
  samplerDesc.minFilter = MTLSamplerMinMagFilterNearest;
  samplerDesc.magFilter = MTLSamplerMinMagFilterNearest;
  samplerDesc.mipFilter = MTLSamplerMipFilterNotMipmapped;
  samplerDesc.sAddressMode = MTLSamplerAddressModeRepeat;
  samplerDesc.tAddressMode = MTLSamplerAddressModeRepeat;
  _terrainSampler = [_device newSamplerStateWithDescriptor:samplerDesc];
}

- (void)loadTerrainTexture {
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
  MTKTextureLoader* loader = [[MTKTextureLoader alloc] initWithDevice:_device];
  NSDictionary* options = @{
    MTKTextureLoaderOptionSRGB : @NO,
    MTKTextureLoaderOptionOrigin : MTKTextureLoaderOriginTopLeft,
  };

  _terrainTexture = [loader newTextureWithContentsOfURL:[NSURL fileURLWithPath:foundPath] options:options error:&error];
  if (!_terrainTexture) {
    NSLog(@"Failed to load terrain texture %@: %@", foundPath, error);
  } else {
    NSLog(@"Loaded terrain texture: %@", foundPath);
  }
}

- (void)mtkView:(MTKView*)view drawableSizeWillChange:(CGSize)size {
  (void)view;
  (void)size;
}

- (void)drawInMTKView:(MTKView*)view {
  if (!_game || !_opaquePipeline || !_transparentPipeline || !_queue) {
    return;
  }

  const CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
  const double dtRaw = now - _lastFrameTime;
  _lastFrameTime = now;
  const double dt = std::min(dtRaw, 0.05);
  _smoothedFrameMs = _smoothedFrameMs * 0.90 + (dtRaw * 1000.0) * 0.10;
  _debugHudAccum += dtRaw;

  const NSRect bounds = _mtkView.bounds;
  const float aspect = static_cast<float>(bounds.size.width / bounds.size.height);
  _game->setViewAspect(aspect);

  mc::InputState input;
  input.moveForward = (_moveForward == YES);
  input.moveBackward = (_moveBackward == YES);
  input.moveLeft = (_moveLeft == YES);
  input.moveRight = (_moveRight == YES);
  input.jump = (_jump == YES);
  input.crouch = (_crouch == YES);
  input.sprint = (_sprintHeld == YES) || (_sprintLatched == YES);
  _game->setInputState(input);
  _game->setBreakHeld(_leftMouseHeld == YES);
  _game->tick(dt);
  if (_rightMouseHeld) {
    _placeRepeatAccumulator += dt;
    int repeats = 0;
    while (_placeRepeatAccumulator >= kPlaceRepeatInterval && repeats < 3) {
      _placeRepeatAccumulator -= kPlaceRepeatInterval;
      _game->interactAtCrosshair(true);
      ++repeats;
    }
  } else {
    _placeRepeatAccumulator = 0.0;
  }
  if (_relativeMouseEnabled) {
    [self centerCursorInWindow];
  }
  _rendererBridge->setViewParams(_game->viewParams(aspect));

  MTLRenderPassDescriptor* pass = view.currentRenderPassDescriptor;
  id<CAMetalDrawable> drawable = view.currentDrawable;
  if (!pass || !drawable) {
    return;
  }

  id<MTLCommandBuffer> cmd = [_queue commandBuffer];
  id<MTLRenderCommandEncoder> enc = [cmd renderCommandEncoderWithDescriptor:pass];

  const mc::TerrainViewParams params = _rendererBridge->params();
  const mc::RenderDebugController::RenderMode renderMode =
      _debugController ? _debugController->renderMode() : mc::RenderDebugController::RenderMode::Textured;
  const FragmentMode fragmentMode =
      (renderMode == mc::RenderDebugController::RenderMode::Textured) ? FragmentMode::Textured : FragmentMode::Flat;
  const uint32_t fragmentModeRaw = static_cast<uint32_t>(fragmentMode);
  const MTLTriangleFillMode fillMode =
      (renderMode == mc::RenderDebugController::RenderMode::Wireframe) ? MTLTriangleFillModeLines : MTLTriangleFillModeFill;

  if (_chunkBorderOverlay && _debugController && _debugController->showChunkBorders()) {
    _chunkBorderOverlay->build(_game->renderCenterChunkX(), _game->renderCenterChunkZ(), _game->renderDistanceChunks(),
                               _debugLineVertices);
  } else {
    _debugLineVertices.clear();
  }
  int lookX = 0;
  int lookY = 0;
  int lookZ = 0;
  const bool hasLookTarget = _game->lookTargetBlock(&lookX, &lookY, &lookZ);
  if (hasLookTarget) {
    appendSelectionWireCube(_debugLineVertices, lookX, lookY, lookZ);
  }
  _rendererBridge->setDebugLineVertices(_debugLineVertices);

  const char* modeStr = renderModeString(renderMode);
  mc::Level* level = _game->level();
  const char* gameModeStr = gameModeString(*_game);
  const NSUInteger opaqueVerts = _rendererBridge->visibleOpaqueVertexCount();
  const NSUInteger transpVerts = _rendererBridge->visibleTransparentVertexCount();
  if (level && _debugLabel && _debugHudAccum >= 0.15) {
    _debugHudAccum = 0.0;
    const double fps = (_smoothedFrameMs > 0.001) ? (1000.0 / _smoothedFrameMs) : 0.0;
    NSString* targetText = hasLookTarget ? [NSString stringWithFormat:@"%d, %d, %d", lookX, lookY, lookZ] : @"none";
    _debugLabel.stringValue = [NSString stringWithFormat:
                               @"FPS: %.1f  Frame: %.2f ms  Render: %s  Game: %s  Look: %@\nChunks Loaded: %zu  Pending: %zu  Ready: %zu  GenThreads: %zu\nVerts O/T: %lu / %lu",
                               fps, _smoothedFrameMs, modeStr, gameModeStr,
                               targetText,
                               level->loadedChunkCount(), level->pendingChunkCount(), level->readyChunkCount(),
                               level->generationThreadCount(), (unsigned long)opaqueVerts, (unsigned long)transpVerts];
  }

  if (_terrainTexture) {
    [enc setFragmentTexture:_terrainTexture atIndex:0];
    [enc setFragmentSamplerState:_terrainSampler atIndex:0];
  }

  [enc setRenderPipelineState:_opaquePipeline];
  [enc setDepthStencilState:_opaqueDepthState];
  [enc setTriangleFillMode:fillMode];
  [enc setFragmentBytes:&fragmentModeRaw length:sizeof(uint32_t) atIndex:1];
  [enc setVertexBytes:&params length:sizeof(mc::TerrainViewParams) atIndex:1];
  for (const std::int64_t key : _rendererBridge->drawKeys()) {
    auto it = _rendererBridge->chunkMeshes().find(key);
    if (it == _rendererBridge->chunkMeshes().end()) {
      continue;
    }
    if (it->second.opaqueBuffer && it->second.opaqueCount > 0) {
      [enc setVertexBuffer:it->second.opaqueBuffer offset:0 atIndex:0];
      [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:it->second.opaqueCount];
    }
  }

  [enc setRenderPipelineState:_transparentPipeline];
  [enc setDepthStencilState:_transparentDepthState];
  [enc setTriangleFillMode:fillMode];
  [enc setFragmentBytes:&fragmentModeRaw length:sizeof(uint32_t) atIndex:1];
  [enc setVertexBytes:&params length:sizeof(mc::TerrainViewParams) atIndex:1];
  _transparentSortAccum += dtRaw;
  if (_transparentSortAccum >= 0.066 || _transparentDrawOrder.empty()) {
    _transparentSortAccum = 0.0;
    struct ChunkSort {
      std::int64_t key = 0;
      float dist2 = 0.0f;
    };
    std::vector<ChunkSort> transparentOrder;
    transparentOrder.reserve(_rendererBridge->drawKeys().size());
    const simd_float3 cam = _game->cameraWorldPosition();
    const float camX = cam.x;
    const float camZ = cam.z;
    for (const std::int64_t key : _rendererBridge->drawKeys()) {
      auto it = _rendererBridge->chunkMeshes().find(key);
      if (it == _rendererBridge->chunkMeshes().end() || it->second.transparentCount == 0 || !it->second.transparentBuffer) {
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
    _transparentDrawOrder.clear();
    _transparentDrawOrder.reserve(transparentOrder.size());
    for (const ChunkSort& ord : transparentOrder) {
      _transparentDrawOrder.push_back(ord.key);
    }
  }
  for (const std::int64_t key : _transparentDrawOrder) {
    auto it = _rendererBridge->chunkMeshes().find(key);
    if (it == _rendererBridge->chunkMeshes().end()) {
      continue;
    }
    [enc setVertexBuffer:it->second.transparentBuffer offset:0 atIndex:0];
    [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:it->second.transparentCount];
  }

  id<MTLBuffer> ovb = _rendererBridge->overlayVertexBuffer();
  const NSUInteger ocount = _rendererBridge->overlayVertexCount();
  if (ovb && ocount > 0) {
    [enc setRenderPipelineState:_transparentPipeline];
    [enc setDepthStencilState:_transparentDepthState];
    [enc setTriangleFillMode:fillMode];
    [enc setFragmentBytes:&fragmentModeRaw length:sizeof(uint32_t) atIndex:1];
    [enc setVertexBuffer:ovb offset:0 atIndex:0];
    [enc setVertexBytes:&params length:sizeof(mc::TerrainViewParams) atIndex:1];
    [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:ocount];
  }

  id<MTLBuffer> dlb = _rendererBridge->debugLineBuffer();
  const NSUInteger dlc = _rendererBridge->debugLineCount();
  if (dlb && dlc > 0) {
    const uint32_t flatModeRaw = static_cast<uint32_t>(FragmentMode::Flat);
    [enc setRenderPipelineState:_transparentPipeline];
    [enc setDepthStencilState:_transparentDepthState];
    [enc setTriangleFillMode:MTLTriangleFillModeFill];
    [enc setFragmentBytes:&flatModeRaw length:sizeof(uint32_t) atIndex:1];
    [enc setVertexBuffer:dlb offset:0 atIndex:0];
    [enc setVertexBytes:&params length:sizeof(mc::TerrainViewParams) atIndex:1];
    [enc drawPrimitives:MTLPrimitiveTypeLine vertexStart:0 vertexCount:dlc];
  }

  [enc endEncoding];
  [cmd presentDrawable:drawable];
  [cmd commit];
}

@end
