#import "Client/App/AppDelegate.h"
#import "Client/App/CrosshairView.h"
#import "Client/App/DebugHudController.h"
#import "Client/App/AppInputState.h"
#import "Client/App/AppWorldRenderer.h"
#import "Client/App/MetalRendererBridge.h"
#import "Client/App/TerrainRenderResources.h"

#import <ApplicationServices/ApplicationServices.h>
#import <Metal/Metal.h>

#include <algorithm>

#include "Client/Debug/ChunkBorderOverlay.h"
#include "Client/Debug/RenderDebugController.h"
#include "Client/Core/Minecraft.h"

@interface AppDelegate ()
- (void)installInputHandlers;
- (void)enableRelativeMouse;
- (void)disableRelativeMouse;
- (void)centerCursorInWindow;
- (void)handleFocusLoss;
@end

@implementation AppDelegate {
  NSWindow* _window;
  MTKView* _mtkView;

  id<MTLDevice> _device;
  id<MTLCommandQueue> _queue;

  std::unique_ptr<mc::Minecraft> _game;
  std::unique_ptr<mc::app::TerrainRenderResources> _terrainResources;
  std::unique_ptr<mc::app::MetalRendererBridge> _rendererBridge;
  std::unique_ptr<mc::app::AppInputState> _inputState;
  std::unique_ptr<mc::app::AppWorldRenderer> _worldRenderer;
  std::unique_ptr<mc::app::DebugHudController> _debugHudController;
  std::unique_ptr<mc::RenderDebugController> _debugController;
  std::unique_ptr<mc::ChunkBorderOverlay> _chunkBorderOverlay;
  CrosshairView* _crosshairView;

  id _inputMonitor;
  BOOL _relativeMouseEnabled;

  CFAbsoluteTime _lastFrameTime;
  double _smoothedFrameMs;
}

namespace {

constexpr CGFloat kInitialWindowWidth = 1280.0;
constexpr CGFloat kInitialWindowHeight = 720.0;
constexpr double kMaxTickSeconds = 0.05;
constexpr double kSmoothingHistoryWeight = 0.90;
constexpr double kSmoothingSampleWeight = 0.10;
constexpr double kInitialFrameMs = 16.67;
constexpr const char* kWindowTitle = "Minecraft Clone";

}  // namespace

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
  (void)notification;

  _device = MTLCreateSystemDefaultDevice();
  _queue = [_device newCommandQueue];

  NSRect frame = NSMakeRect(0, 0, kInitialWindowWidth, kInitialWindowHeight);
  _window = [[NSWindow alloc] initWithContentRect:frame
                                         styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable)
                                           backing:NSBackingStoreBuffered
                                             defer:NO];
  _window.delegate = self;
  [_window setTitle:[NSString stringWithUTF8String:kWindowTitle]];
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

  [_window makeKeyAndOrderFront:nil];
  [_window makeFirstResponder:_mtkView];

  _rendererBridge = std::make_unique<mc::app::MetalRendererBridge>(_device);
  _terrainResources = std::make_unique<mc::app::TerrainRenderResources>(
      _device, _mtkView.colorPixelFormat, _mtkView.depthStencilPixelFormat);
  _inputState = std::make_unique<mc::app::AppInputState>();
  _worldRenderer = std::make_unique<mc::app::AppWorldRenderer>();
  _debugHudController = std::make_unique<mc::app::DebugHudController>();
  _debugController = std::make_unique<mc::RenderDebugController>();
  _chunkBorderOverlay = std::make_unique<mc::ChunkBorderOverlay>();
  _game = std::make_unique<mc::Minecraft>();
  _game->init(_rendererBridge.get());
  _debugHudController->attachToView(_mtkView, frame);

  [self installInputHandlers];
  [self enableRelativeMouse];

  _lastFrameTime = CFAbsoluteTimeGetCurrent();
  _smoothedFrameMs = kInitialFrameMs;
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
  (void)sender;
  return YES;
}

- (void)applicationWillResignActive:(NSNotification*)notification {
  (void)notification;
  [self handleFocusLoss];
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
  [self handleFocusLoss];
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

- (void)handleFocusLoss {
  if (_inputState) {
    _inputState->resetForFocusLoss(_game.get());
  }
  [self disableRelativeMouse];
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
    if (!strongSelf->_window || !strongSelf->_game || !strongSelf->_inputState) {
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
      strongSelf->_inputState->handleLeftMouse(true, strongSelf->_game.get());
      return nil;
    }

    if (event.type == NSEventTypeLeftMouseUp) {
      strongSelf->_inputState->handleLeftMouse(false, strongSelf->_game.get());
      return nil;
    }

    if (event.type == NSEventTypeRightMouseDown) {
      strongSelf->_inputState->handleRightMouse(true, strongSelf->_game.get());
      return nil;
    }

    if (event.type == NSEventTypeRightMouseUp) {
      strongSelf->_inputState->handleRightMouse(false, strongSelf->_game.get());
      return nil;
    }

    if (event.type == NSEventTypeKeyDown || event.type == NSEventTypeKeyUp) {
      if (strongSelf->_inputState->handleMovementKeyEvent(
              event, (event.type == NSEventTypeKeyDown), strongSelf->_debugController.get(), strongSelf->_game.get())) {
        return nil;
      }
      return event;
    }

    if (event.type == NSEventTypeFlagsChanged) {
      strongSelf->_inputState->handleModifierFlagsChanged(event.modifierFlags);
      return nil;
    }

    return event;
  }];
}

- (void)mtkView:(MTKView*)view drawableSizeWillChange:(CGSize)size {
  (void)view;
  (void)size;
}

- (void)drawInMTKView:(MTKView*)view {
  if (!_game || !_terrainResources || !_rendererBridge || !_worldRenderer || !_debugHudController || !_queue) {
    return;
  }

  const CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
  const double dtRaw = now - _lastFrameTime;
  _lastFrameTime = now;
  const double dt = std::min(dtRaw, kMaxTickSeconds);
  _smoothedFrameMs = _smoothedFrameMs * kSmoothingHistoryWeight + (dtRaw * 1000.0) * kSmoothingSampleWeight;

  const NSRect bounds = _mtkView.bounds;
  const float aspect = static_cast<float>(bounds.size.width / bounds.size.height);
  _game->setViewAspect(aspect);

  _game->setInputState(_inputState ? _inputState->currentInputState() : mc::InputState{});
  _game->setBreakHeld(_inputState && _inputState->leftMouseHeld());
  _game->tick(dt);
  if (_inputState) {
    _inputState->advancePlacement(dt, [self]() { self->_game->interactAtCrosshair(true); });
  }
  if (_relativeMouseEnabled) {
    [self centerCursorInWindow];
  }
  _rendererBridge->setViewParams(_game->viewParams(aspect));

  const mc::app::AppWorldRenderResult renderResult =
      _worldRenderer->render(view, _queue, *_terrainResources, *_rendererBridge, *_game, _debugController.get(),
                             _chunkBorderOverlay.get(), dtRaw);
  _debugHudController->tick(dtRaw, *_game,
                            _debugController ? _debugController->renderMode() : mc::RenderDebugController::RenderMode::Textured,
                            renderResult.hasLookTarget, renderResult.lookX, renderResult.lookY, renderResult.lookZ,
                            renderResult.opaqueVertices, renderResult.transparentVertices, _smoothedFrameMs);
}

@end
