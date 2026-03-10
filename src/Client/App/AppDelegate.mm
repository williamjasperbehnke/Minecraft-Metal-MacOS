#import "Client/App/AppDelegate.h"
#import "Client/App/AppInputState.h"
#import "Client/App/AppWorldRenderer.h"
#import "Client/App/CrosshairView.h"
#import "Client/App/DebugHudController.h"
#import "Client/App/InventoryView.h"
#import "Client/App/MetalRendererBridge.h"
#import "Client/App/TerrainRenderResources.h"

#import <ApplicationServices/ApplicationServices.h>
#import <Metal/Metal.h>

#include <algorithm>

#include "Client/Core/Minecraft.h"
#include "Client/Debug/ChunkBorderOverlay.h"
#include "Client/Debug/RenderDebugController.h"

@interface AppDelegate ()
- (void)installInputHandlers;
- (void)enableRelativeMouse;
- (void)disableRelativeMouse;
- (void)centerCursorInWindow;
- (void)handleFocusLoss;
- (void)updateMouseCaptureState;
- (NSEvent* _Nullable)handleInputEvent:(NSEvent*)event;
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
  InventoryView* _inventoryView;

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
  _inventoryView = [[InventoryView alloc] initWithFrame:_mtkView.bounds];
  [_mtkView addSubview:_inventoryView];

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

- (void)updateMouseCaptureState {
  if (!_game) {
    return;
  }
  if (_game->isInventoryOpen()) {
    [self disableRelativeMouse];
  } else if (_window && _window.isKeyWindow && NSApp.isActive) {
    [self enableRelativeMouse];
  }
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
                                                                 NSEventMaskOtherMouseDown |
                                                                 NSEventMaskOtherMouseUp |
                                                                 NSEventMaskKeyDown |
                                                                 NSEventMaskKeyUp |
                                                                 NSEventMaskFlagsChanged |
                                                                 NSEventMaskScrollWheel |
                                                                 NSEventMaskMouseMoved |
                                                                 NSEventMaskLeftMouseDragged |
                                                                 NSEventMaskRightMouseDragged)
                                                         handler:^NSEvent* _Nullable(NSEvent* event) {
    return [self handleInputEvent:event];
  }];
}

- (NSEvent* _Nullable)handleInputEvent:(NSEvent*)event {
  if (!_window || !_game || !_inputState) {
    return event;
  }
  if (event.window != _window) {
    return event;
  }

  const bool inventoryOpen = _game->isInventoryOpen();
  switch (event.type) {
    case NSEventTypeMouseMoved:
    case NSEventTypeLeftMouseDragged:
    case NSEventTypeRightMouseDragged:
      if (inventoryOpen) {
        _inputState->handleInventoryMouseEvent(event, _game.get(), _inventoryView);
        return nil;
      }
      _game->addLookInput(static_cast<float>(event.deltaX), static_cast<float>(event.deltaY));
      return event;
    case NSEventTypeLeftMouseDown:
      if (inventoryOpen) {
        _inputState->handleInventoryMouseEvent(event, _game.get(), _inventoryView);
        return nil;
      }
      _inputState->handleLeftMouse(true, _game.get());
      return nil;
    case NSEventTypeLeftMouseUp:
      if (inventoryOpen) {
        _inputState->handleInventoryMouseEvent(event, _game.get(), _inventoryView);
        return nil;
      }
      _inputState->handleLeftMouse(false, _game.get());
      return nil;
    case NSEventTypeRightMouseDown:
      if (inventoryOpen) {
        _inputState->handleInventoryMouseEvent(event, _game.get(), _inventoryView);
        return nil;
      }
      _inputState->handleRightMouse(true, _game.get());
      return nil;
    case NSEventTypeOtherMouseDown:
      if (inventoryOpen) {
        _inputState->handleInventoryMouseEvent(event, _game.get(), _inventoryView);
        return nil;
      }
      return event;
    case NSEventTypeRightMouseUp:
      if (inventoryOpen) {
        _inputState->handleInventoryMouseEvent(event, _game.get(), _inventoryView);
        return nil;
      }
      _inputState->handleRightMouse(false, _game.get());
      return nil;
    case NSEventTypeOtherMouseUp:
      if (inventoryOpen) {
        _inputState->handleInventoryMouseEvent(event, _game.get(), _inventoryView);
        return nil;
      }
      return event;
    case NSEventTypeKeyDown:
      if (inventoryOpen) {
        if (_inputState->handleInventoryKeyDownEvent(event, _game.get(), _inventoryView)) {
          return nil;
        }
      }
      if (_inputState->handleMovementKeyEvent(event, true, _debugController.get(), _game.get())) {
        if (!inventoryOpen) {
          const int tooltipTile = _inputState->takePendingHotbarTooltipTile();
          if (tooltipTile > 0) {
            [_inventoryView showHotbarTooltipForTile:tooltipTile];
          }
        }
        return nil;
      }
      return event;
    case NSEventTypeKeyUp:
      if (_inputState->handleMovementKeyEvent(event, false, _debugController.get(), _game.get())) {
        return nil;
      }
      return event;
    case NSEventTypeScrollWheel:
      if (!_inputState->handleScrollWheelEvent(event, _game.get())) {
        if (inventoryOpen) {
          return nil;
        }
        return event;
      }
      {
        const int tooltipTile = _inputState->takePendingHotbarTooltipTile();
        if (tooltipTile > 0) {
          [_inventoryView showHotbarTooltipForTile:tooltipTile];
        }
      }
      if (inventoryOpen) {
        return nil;
      }
      return nil;
    case NSEventTypeFlagsChanged:
      _inputState->handleModifierFlagsChanged(event.modifierFlags);
      return nil;
    default:
      return event;
  }
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
  if (_inputState && !_game->isInventoryOpen()) {
    _inputState->advancePlacement(dt, [self]() { self->_game->interactAtCrosshair(true); });
    _inputState->advanceItemDrop(dt, _game.get());
  }
  [self updateMouseCaptureState];
  _crosshairView.hidden = (_game->isInventoryOpen() || _game->isSpectatorMode() || _game->isThirdPersonMode()) ? YES : NO;
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
  [_inventoryView updateFromGame:*_game];
}

@end
