#import "Client/App/InventoryView.h"
#import "Client/App/UiImageHelpers.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <CoreText/CoreText.h>
#include <simd/simd.h>

#include "Client/Core/Minecraft.h"
#include "Client/App/DebugHudFormatter.h"
#include "Client/Inventory/Inventory.h"
#include "Client/Render/BlockRender.h"
#include "World/Tile/Tile.h"

namespace {

constexpr CGFloat kHotbarBottomMargin = 18.0;
constexpr CGFloat kUiScale = 3.0;

constexpr CGFloat kInventoryGuiWidth = 176.0;
constexpr CGFloat kInventoryGuiHeight = 166.0;
constexpr CGFloat kInventorySlotPx = 18.0;
constexpr CGFloat kInventoryMainX = 7.0;
constexpr CGFloat kInventoryMainTopY = 83.0;
constexpr CGFloat kInventoryHotbarX = 7.0;
constexpr CGFloat kInventoryHotbarTopY = 141.0;
constexpr CFTimeInterval kHotbarTooltipDuration = 1.2;

int atlasTextureForTile(int tile) {
  return mc::render::textureForTileFace(tile, mc::render::BlockFace::North);
}

bool isFlatItemTile(int tile) {
  return mc::render::isPlantRenderTile(tile) || !mc::isSolidTileId(tile);
}

bool tileUsesBiomeTint(int tile, bool allowGrassTint) {
  return mc::render::usesBiomeTint(tile, allowGrassTint);
}

NSColor* biomeTintColorForTile(int tile, bool allowGrassTint) {
  const simd_float3 tint = mc::render::biomeTintForBlock(tile, allowGrassTint);
  return [NSColor colorWithCalibratedRed:tint.x green:tint.y blue:tint.z alpha:1.0];
}

NSRect atlasSrcRectForIndex(int atlasIndex, NSImage* terrainImage) {
  const int col = atlasIndex % 16;
  const int row = atlasIndex / 16;
  const CGFloat atlasW = terrainImage.size.width;
  const CGFloat atlasH = terrainImage.size.height;
  const CGFloat tileW = atlasW / 16.0;
  const CGFloat tileH = atlasH / 16.0;
  return NSMakeRect(col * tileW, atlasH - (row + 1) * tileH, tileW, tileH);
}

NSImage* buildProcessedFlatIcon(NSImage* terrainImage, int atlasIndex, NSInteger outW, NSInteger outH, simd_float3 tint,
                                bool applyTint, bool cutout) {
  NSBitmapImageRep* rep = [[NSBitmapImageRep alloc]
      initWithBitmapDataPlanes:nullptr
                    pixelsWide:std::max<NSInteger>(1, outW)
                    pixelsHigh:std::max<NSInteger>(1, outH)
                 bitsPerSample:8
               samplesPerPixel:4
                      hasAlpha:YES
                      isPlanar:NO
                colorSpaceName:NSCalibratedRGBColorSpace
                   bytesPerRow:0
                  bitsPerPixel:0];
  if (!rep) {
    return nil;
  }
  [NSGraphicsContext saveGraphicsState];
  NSGraphicsContext* ctx = [NSGraphicsContext graphicsContextWithBitmapImageRep:rep];
  [NSGraphicsContext setCurrentContext:ctx];
  NSGraphicsContext.currentContext.imageInterpolation = NSImageInterpolationNone;
  [[NSColor clearColor] setFill];
  NSRectFillUsingOperation(NSMakeRect(0.0, 0.0, static_cast<CGFloat>(outW), static_cast<CGFloat>(outH)),
                           NSCompositingOperationClear);
  const NSRect src = atlasSrcRectForIndex(atlasIndex, terrainImage);
  [terrainImage drawInRect:NSMakeRect(0.0, 0.0, static_cast<CGFloat>(outW), static_cast<CGFloat>(outH))
                  fromRect:src
                 operation:NSCompositingOperationSourceOver
                  fraction:1.0];
  [NSGraphicsContext restoreGraphicsState];

  const CGFloat tr = static_cast<CGFloat>(tint.x);
  const CGFloat tg = static_cast<CGFloat>(tint.y);
  const CGFloat tb = static_cast<CGFloat>(tint.z);
  unsigned char* pixels = [rep bitmapData];
  if (!pixels || [rep bitsPerSample] != 8 || [rep samplesPerPixel] < 4) {
    return nil;
  }
  const NSInteger bytesPerRow = [rep bytesPerRow];
  const bool alphaFirst = ([rep bitmapFormat] & NSBitmapFormatAlphaFirst) != 0;
  const int rIndex = alphaFirst ? 1 : 0;
  const int gIndex = alphaFirst ? 2 : 1;
  const int bIndex = alphaFirst ? 3 : 2;
  const int aIndex = alphaFirst ? 0 : 3;

  for (NSInteger py = 0; py < outH; ++py) {
    unsigned char* row = pixels + py * bytesPerRow;
    for (NSInteger px = 0; px < outW; ++px) {
      unsigned char* p = row + px * 4;
      CGFloat r = static_cast<CGFloat>(p[rIndex]) / 255.0;
      CGFloat g = static_cast<CGFloat>(p[gIndex]) / 255.0;
      CGFloat b = static_cast<CGFloat>(p[bIndex]) / 255.0;
      CGFloat a = static_cast<CGFloat>(p[aIndex]) / 255.0;

      if (cutout && a < 0.1) {
        a = 0.0;
      }
      if (applyTint && a > 0.0) {
        r *= tr;
        g *= tg;
        b *= tb;
      }
      p[rIndex] = static_cast<unsigned char>(std::clamp(r, 0.0, 1.0) * 255.0 + 0.5);
      p[gIndex] = static_cast<unsigned char>(std::clamp(g, 0.0, 1.0) * 255.0 + 0.5);
      p[bIndex] = static_cast<unsigned char>(std::clamp(b, 0.0, 1.0) * 255.0 + 0.5);
      p[aIndex] = static_cast<unsigned char>(std::clamp(a, 0.0, 1.0) * 255.0 + 0.5);
    }
  }

  NSImage* out = [[NSImage alloc] initWithSize:NSMakeSize(static_cast<CGFloat>(outW), static_cast<CGFloat>(outH))];
  [out addRepresentation:rep];
  return out;
}

NSRect centeredSquareInRect(NSRect rect, CGFloat fill) {
  const CGFloat side = std::max<CGFloat>(1.0, std::floor(std::min(rect.size.width, rect.size.height) * fill));
  const CGFloat x = std::floor(NSMidX(rect) - side * 0.5);
  const CGFloat y = std::floor(NSMidY(rect) - side * 0.5);
  return NSMakeRect(x, y, side, side);
}

NSString* itemNameForTile(int tile) {
  static NSMutableDictionary<NSNumber*, NSString*>* sNameCache = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    sNameCache = [[NSMutableDictionary alloc] init];
  });

  NSNumber* key = @(tile);
  NSString* cached = sNameCache[key];
  if (cached) {
    return cached;
  }

  const char* name = mc::app::DebugHudFormatter::tileName(tile);
  NSString* out = [NSString stringWithUTF8String:name];
  if (!out || out.length == 0) {
    out = @"Unknown";
  }
  sNameCache[key] = out;
  return out;
}

NSFont* minecraftHudFont(CGFloat size) {
  static NSString* sFontName = nil;
  static bool sFontInit = false;
  if (!sFontInit) {
    sFontInit = true;
    NSString* fontPath = mc::app::ui::findAssetPath(@"MinecraftMetal/Assets/Mojangles.ttf");
    if (!fontPath) {
      fontPath = mc::app::ui::findAssetPath(@"MinecraftMetal/Assets/MojangFont11.ttf");
    }
    if (fontPath) {
      NSURL* url = [NSURL fileURLWithPath:fontPath];
      CTFontManagerRegisterFontsForURL((__bridge CFURLRef)url, kCTFontManagerScopeProcess, nullptr);
      CFArrayRef descriptors = CTFontManagerCreateFontDescriptorsFromURL((__bridge CFURLRef)url);
      if (descriptors && CFArrayGetCount(descriptors) > 0) {
        auto* descriptor = (CTFontDescriptorRef)CFArrayGetValueAtIndex(descriptors, 0);
        CFStringRef psName = (CFStringRef)CTFontDescriptorCopyAttribute(descriptor, kCTFontNameAttribute);
        if (psName) {
          sFontName = [(__bridge NSString*)psName copy];
          CFRelease(psName);
        }
      }
      if (descriptors) {
        CFRelease(descriptors);
      }
    }
  }
  NSFont* font = sFontName ? [NSFont fontWithName:sFontName size:size] : nil;
  if (!font) font = [NSFont fontWithName:@"Mojangles" size:size];
  if (!font) font = [NSFont fontWithName:@"Minecraft" size:size];
  return font ?: [NSFont monospacedSystemFontOfSize:size weight:NSFontWeightBold];
}

}  // namespace

@implementation InventoryView {
  BOOL _inventoryOpen;
  int _selectedHotbar;
  int _hoveredSlotIndex;
  int _carriedTile;
  int _carriedCount;
  BOOL _hotbarTooltipVisibleLastFrame;
  NSPoint _cursorPoint;
  CFAbsoluteTime _hotbarTooltipUntil;
  int _hotbarTooltipTile;
  std::array<mc::Inventory::Slot, mc::Inventory::kTotalSlots> _slots;
  std::array<mc::Inventory::Slot, mc::Inventory::kTotalSlots> _lastCachedSlots;
  BOOL _hasCachedSlots;
  NSInteger _cachedInvFlat;
  NSInteger _cachedInvCube;
  NSInteger _cachedHotFlat;
  NSInteger _cachedHotCube;
  NSImage* _terrainImage;
  NSImage* _inventoryPanelImage;
  NSImage* _hotbarBackImage;
  NSImage* _hotbarSelectedImage;
  NSImage* _slotHighlightImage;
  NSTimer* _hotbarTooltipTimer;
  NSMutableDictionary<NSNumber*, NSImage*>* _flatIconCache;
  NSMutableDictionary<NSNumber*, NSImage*>* _cubeIconCache;
}

- (instancetype)initWithFrame:(NSRect)frameRect {
  self = [super initWithFrame:frameRect];
  if (self) {
    [self setWantsLayer:YES];
    self.layer.backgroundColor = NSColor.clearColor.CGColor;
    self.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _inventoryOpen = NO;
    _selectedHotbar = 0;
    _hoveredSlotIndex = -1;
    _carriedTile = 0;
    _carriedCount = 0;
    _hotbarTooltipVisibleLastFrame = NO;
    _cursorPoint = NSMakePoint(0.0, 0.0);
    _hotbarTooltipUntil = 0.0;
    _hotbarTooltipTile = 0;
    _hotbarTooltipTimer = nil;
    _hasCachedSlots = NO;
    _cachedInvFlat = 0;
    _cachedInvCube = 0;
    _cachedHotFlat = 0;
    _cachedHotCube = 0;

    NSString* terrainPath = mc::app::ui::findAssetPath(@"MinecraftMetal/Assets/terrain.png");
    if (terrainPath) {
      _terrainImage = [[NSImage alloc] initWithContentsOfFile:terrainPath];
    }
    NSString* hotbarBackPath = mc::app::ui::findAssetPath(@"MinecraftMetal/Assets/hotbar_item_back.png");
    if (hotbarBackPath) {
      _hotbarBackImage = [[NSImage alloc] initWithContentsOfFile:hotbarBackPath];
    }
    NSString* hotbarSelectedPath = mc::app::ui::findAssetPath(@"MinecraftMetal/Assets/hotbar_item_selected.png");
    if (hotbarSelectedPath) {
      _hotbarSelectedImage = [[NSImage alloc] initWithContentsOfFile:hotbarSelectedPath];
    }
    NSString* inventoryPanelPath = mc::app::ui::findAssetPath(@"MinecraftMetal/Assets/inventory_panel.png");
    if (inventoryPanelPath) {
      _inventoryPanelImage = [[NSImage alloc] initWithContentsOfFile:inventoryPanelPath];
    }
    {
      NSString* slotPath = mc::app::ui::findAssetPath(@"MinecraftMetal/Assets/slot.png");
      if (slotPath) {
        _slotHighlightImage = [[NSImage alloc] initWithContentsOfFile:slotPath];
      }
    }
    _flatIconCache = [[NSMutableDictionary alloc] init];
    _cubeIconCache = [[NSMutableDictionary alloc] init];
    (void)minecraftHudFont(14.0);
  }
  return self;
}

- (BOOL)isOpaque {
  return NO;
}

- (void)dealloc {
  [_hotbarTooltipTimer invalidate];
  [super dealloc];
}

- (void)clearHotbarTooltipTimer {
  [_hotbarTooltipTimer invalidate];
  _hotbarTooltipTimer = nil;
}

- (void)scheduleHotbarTooltipTimer {
  [self clearHotbarTooltipTimer];
  const CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
  const NSTimeInterval delay = std::max<CFTimeInterval>(0.0, _hotbarTooltipUntil - now);
  _hotbarTooltipTimer = [NSTimer timerWithTimeInterval:delay
                                                target:self
                                              selector:@selector(hotbarTooltipTimerFired:)
                                              userInfo:nil
                                               repeats:NO];
  [[NSRunLoop mainRunLoop] addTimer:_hotbarTooltipTimer forMode:NSRunLoopCommonModes];
}

- (void)hotbarTooltipTimerFired:(NSTimer*)timer {
  if (timer != _hotbarTooltipTimer) {
    return;
  }
  _hotbarTooltipTimer = nil;
  const CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
  if (_hotbarTooltipTile > 0 && now >= _hotbarTooltipUntil) {
    _hotbarTooltipTile = 0;
    _hotbarTooltipUntil = 0.0;
    [self setNeedsDisplay:YES];
  }
}

- (void)showHotbarTooltipForTile:(int)tile {
  const int shownTile = (tile > 0) ? tile : 0;
  if (shownTile <= 0) {
    _hotbarTooltipTile = 0;
    _hotbarTooltipUntil = 0.0;
    [self clearHotbarTooltipTimer];
    [self setNeedsDisplay:YES];
    return;
  }
  _hotbarTooltipTile = shownTile;
  _hotbarTooltipUntil = CFAbsoluteTimeGetCurrent() + kHotbarTooltipDuration;
  [self scheduleHotbarTooltipTimer];
  [self setNeedsDisplay:YES];
}

- (void)updateFromGame:(const mc::Minecraft&)game {
  const CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
  const BOOL prevInventoryOpen = _inventoryOpen;
  const int prevSelectedHotbar = _selectedHotbar;
  const int prevCarriedTile = _carriedTile;
  const int prevCarriedCount = _carriedCount;
  const int prevHoveredSlot = _hoveredSlotIndex;

  _inventoryOpen = game.isInventoryOpen() ? YES : NO;
  _selectedHotbar = game.selectedHotbarSlot();
  _carriedTile = game.inventoryCarriedTile();
  _carriedCount = game.inventoryCarriedCount();
  const mc::Inventory& inv = game.inventory();
  bool slotsChanged = !_hasCachedSlots;
  for (int i = 0; i < mc::Inventory::kTotalSlots; ++i) {
    const mc::Inventory::Slot slot = inv.slot(i);
    _slots[static_cast<std::size_t>(i)] = slot;
    if (!_hasCachedSlots) {
      continue;
    }
    const mc::Inventory::Slot prev = _lastCachedSlots[static_cast<std::size_t>(i)];
    if (slot.tile != prev.tile || slot.count != prev.count) {
      slotsChanged = true;
    }
  }
  if (_inventoryOpen && self.window) {
    const NSPoint winPoint = [self.window mouseLocationOutsideOfEventStream];
    _cursorPoint = [self convertPoint:winPoint fromView:nil];
    _hoveredSlotIndex = [self slotIndexAtPoint:_cursorPoint];
  } else if (!_inventoryOpen) {
    _hoveredSlotIndex = -1;
  }
  const CGFloat invSlot = kInventorySlotPx * kUiScale;
  const CGFloat hotbarSlot = 16.0 * kUiScale;
  const NSInteger invFlat = std::max<NSInteger>(1, static_cast<NSInteger>(std::floor(invSlot * 0.86)));
  const NSInteger invCube = std::max<NSInteger>(1, static_cast<NSInteger>(std::floor(invSlot * 0.96)));
  const NSInteger hotFlat = std::max<NSInteger>(1, static_cast<NSInteger>(std::floor(hotbarSlot * 0.86)));
  const NSInteger hotCube = std::max<NSInteger>(1, static_cast<NSInteger>(std::floor(hotbarSlot * 0.96)));
  const bool iconSizeChanged =
      (invFlat != _cachedInvFlat || invCube != _cachedInvCube || hotFlat != _cachedHotFlat || hotCube != _cachedHotCube);
  if (slotsChanged || iconSizeChanged) {
    for (const auto& slot : _slots) {
      if (slot.tile <= 0 || slot.count <= 0) {
        continue;
      }
      if (isFlatItemTile(slot.tile)) {
        [self ensureFlatIconCachedForTile:slot.tile width:invFlat height:invFlat];
        [self ensureFlatIconCachedForTile:slot.tile width:hotFlat height:hotFlat];
      } else {
        [self ensureCubeIconCachedForTile:slot.tile width:invCube height:invCube];
        [self ensureCubeIconCachedForTile:slot.tile width:hotCube height:hotCube];
      }
    }
    _lastCachedSlots = _slots;
    _hasCachedSlots = YES;
    _cachedInvFlat = invFlat;
    _cachedInvCube = invCube;
    _cachedHotFlat = hotFlat;
    _cachedHotCube = hotCube;
  }

  if (_hotbarTooltipTile > 0 && now >= _hotbarTooltipUntil) {
    _hotbarTooltipTile = 0;
    _hotbarTooltipUntil = 0.0;
    [self clearHotbarTooltipTimer];
  }
  const bool tooltipVisibleNow = (!_inventoryOpen && _hotbarTooltipTile > 0 && now < _hotbarTooltipUntil);
  const bool hotbarTooltipVisibilityChanged = (_hotbarTooltipVisibleLastFrame != (tooltipVisibleNow ? YES : NO));
  _hotbarTooltipVisibleLastFrame = tooltipVisibleNow ? YES : NO;
  const bool visualStateChanged = (prevInventoryOpen != _inventoryOpen) || (prevSelectedHotbar != _selectedHotbar) ||
                                  (prevCarriedTile != _carriedTile) || (prevCarriedCount != _carriedCount) ||
                                  (prevHoveredSlot != _hoveredSlotIndex);
  if (visualStateChanged || slotsChanged || iconSizeChanged || tooltipVisibleNow || hotbarTooltipVisibilityChanged ||
      _inventoryOpen) {
    [self setNeedsDisplay:YES];
  }
}

- (NSRect)inventoryPanelRectForBounds:(NSRect)bounds {
  const CGFloat panelWidth = kInventoryGuiWidth * kUiScale;
  const CGFloat panelHeight = kInventoryGuiHeight * kUiScale;
  const CGFloat centeredPanelX = std::floor((NSWidth(bounds) - panelWidth) * 0.5);
  const CGFloat centeredPanelY = std::floor((NSHeight(bounds) - panelHeight) * 0.5);
  const CGFloat panelX = std::clamp(centeredPanelX, 0.0, std::max(0.0, NSWidth(bounds) - panelWidth));
  const CGFloat panelY = std::clamp(centeredPanelY, 0.0, std::max(0.0, NSHeight(bounds) - panelHeight));
  return NSMakeRect(panelX, panelY, panelWidth, panelHeight);
}

- (NSRect)inventorySlotRectForIndex:(int)slotIndex panelRect:(NSRect)panelRect {
  if (slotIndex < 0 || slotIndex >= mc::Inventory::kTotalSlots) {
    return NSZeroRect;
  }
  const CGFloat s = kUiScale;
  const CGFloat slotSize = kInventorySlotPx * s;
  if (slotIndex < mc::Inventory::kHotbarSize) {
    const CGFloat hotbarStartX = std::floor(NSMinX(panelRect) + kInventoryHotbarX * s);
    const CGFloat hotbarSlotsY = std::floor(NSMinY(panelRect) + (kInventoryGuiHeight - kInventoryHotbarTopY - kInventorySlotPx) * s);
    return NSMakeRect(hotbarStartX + slotIndex * slotSize, hotbarSlotsY, slotSize, slotSize);
  }
  const int main = slotIndex - mc::Inventory::kHotbarSize;
  const int row = main / mc::Inventory::kMainCols;
  const int col = main % mc::Inventory::kMainCols;
  const CGFloat slotsStartX = std::floor(NSMinX(panelRect) + kInventoryMainX * s);
  const CGFloat topRowY = std::floor(NSMinY(panelRect) + (kInventoryGuiHeight - kInventoryMainTopY - kInventorySlotPx) * s);
  return NSMakeRect(slotsStartX + col * slotSize, topRowY - row * slotSize, slotSize, slotSize);
}

- (int)slotIndexAtPoint:(NSPoint)viewPoint {
  if (!_inventoryOpen) {
    return -1;
  }
  const NSRect panelRect = [self inventoryPanelRectForBounds:self.bounds];
  if (!NSPointInRect(viewPoint, panelRect)) {
    return -1;
  }
  for (int i = 0; i < mc::Inventory::kTotalSlots; ++i) {
    if (NSPointInRect(viewPoint, [self inventorySlotRectForIndex:i panelRect:panelRect])) {
      return i;
    }
  }
  return -1;
}

- (int)hoveredSlotIndex {
  return _hoveredSlotIndex;
}

- (NSNumber*)iconCacheKeyForTile:(int)tile width:(NSInteger)w height:(NSInteger)h {
  const NSInteger keyValue = (static_cast<NSInteger>(tile) << 20) ^ (w << 10) ^ h;
  return @(keyValue);
}

- (void)ensureFlatIconCachedForTile:(int)tile width:(NSInteger)w height:(NSInteger)h {
  if (!_terrainImage || tile <= 0) {
    return;
  }
  NSNumber* key = [self iconCacheKeyForTile:tile width:w height:h];
  if (_flatIconCache[key]) {
    return;
  }
  const simd_float3 tint = mc::render::biomeTintForBlock(tile, true);
  NSImage* icon = buildProcessedFlatIcon(_terrainImage, atlasTextureForTile(tile), w, h, tint,
                                         tileUsesBiomeTint(tile, true), mc::render::isPlantRenderTile(tile));
  if (!icon) {
    icon = [[NSImage alloc] initWithSize:NSMakeSize(static_cast<CGFloat>(w), static_cast<CGFloat>(h))];
    [icon lockFocus];
    NSGraphicsContext.currentContext.imageInterpolation = NSImageInterpolationNone;
    const NSRect src = atlasSrcRectForIndex(atlasTextureForTile(tile), _terrainImage);
    [_terrainImage drawInRect:NSMakeRect(0.0, 0.0, static_cast<CGFloat>(w), static_cast<CGFloat>(h))
                     fromRect:src
                    operation:NSCompositingOperationSourceOver
                     fraction:1.0];
    [icon unlockFocus];
  }
  _flatIconCache[key] = icon;
}

- (void)ensureCubeIconCachedForTile:(int)tile width:(NSInteger)w height:(NSInteger)h {
  if (!_terrainImage || tile <= 0) {
    return;
  }
  NSNumber* key = [self iconCacheKeyForTile:tile width:w height:h];
  if (_cubeIconCache[key]) {
    return;
  }
  NSImage* icon = [[NSImage alloc] initWithSize:NSMakeSize(static_cast<CGFloat>(w), static_cast<CGFloat>(h))];
  [icon lockFocus];
  NSGraphicsContext.currentContext.imageInterpolation = NSImageInterpolationNone;
  [[NSColor clearColor] setFill];
  NSRectFillUsingOperation(NSMakeRect(0.0, 0.0, static_cast<CGFloat>(w), static_cast<CGFloat>(h)), NSCompositingOperationClear);
  const NSRect localRect = NSMakeRect(0.0, 0.0, static_cast<CGFloat>(w), static_cast<CGFloat>(h));

  auto drawTexturedQuad = [&](const std::array<NSPoint, 4>& q, mc::render::BlockFace face, CGFloat shade) {
    NSBezierPath* path = [NSBezierPath bezierPath];
    [path moveToPoint:q[0]];
    [path lineToPoint:q[1]];
    [path lineToPoint:q[2]];
    [path lineToPoint:q[3]];
    [path closePath];

    const NSRect src = atlasSrcRectForIndex(mc::render::textureForTileFace(tile, face), _terrainImage);
    [NSGraphicsContext saveGraphicsState];
    [path addClip];

    NSAffineTransformStruct t;
    t.m11 = q[1].x - q[0].x;
    t.m12 = q[1].y - q[0].y;
    t.m21 = q[3].x - q[0].x;
    t.m22 = q[3].y - q[0].y;
    t.tX = q[0].x;
    t.tY = q[0].y;
    NSAffineTransform* xform = [NSAffineTransform transform];
    [xform setTransformStruct:t];
    [xform concat];
    [_terrainImage drawInRect:NSMakeRect(0.0, 0.0, 1.0, 1.0)
                     fromRect:src
                    operation:NSCompositingOperationSourceOver
                     fraction:1.0];

    const bool allowGrassTint = (face == mc::render::BlockFace::Top);
    if (mc::render::shouldTintFace(tile, face) && tileUsesBiomeTint(tile, allowGrassTint)) {
      [biomeTintColorForTile(tile, allowGrassTint) setFill];
      NSRectFillUsingOperation(NSMakeRect(0.0, 0.0, 1.0, 1.0), NSCompositingOperationMultiply);
    }

    if (shade < 0.999) {
      [[NSColor colorWithWhite:0.0 alpha:(1.0 - shade)] setFill];
      [path fill];
    }
    [NSGraphicsContext restoreGraphicsState];
  };

  const mc::render::FaceBounds topBounds =
      mc::render::computeFaceBounds(tile, mc::render::BlockFace::Top, 0.0f, 0.0f, 0.0f, 0.0f, false);
  const mc::render::FaceBounds southBounds =
      mc::render::computeFaceBounds(tile, mc::render::BlockFace::South, 0.0f, 0.0f, 0.0f, 0.0f, false);
  const mc::render::FaceBounds eastBounds =
      mc::render::computeFaceBounds(tile, mc::render::BlockFace::East, 0.0f, 0.0f, 0.0f, 0.0f, false);

  std::array<simd_float3, 4> top3d = {
      simd_float3{topBounds.topX0, topBounds.maxY, topBounds.topZ0},
      simd_float3{topBounds.topX1, topBounds.maxY, topBounds.topZ0},
      simd_float3{topBounds.topX1, topBounds.maxY, topBounds.topZ1},
      simd_float3{topBounds.topX0, topBounds.maxY, topBounds.topZ1},
  };
  std::array<simd_float3, 4> south3d = {
      simd_float3{southBounds.sideX0, southBounds.minY, southBounds.southZ},
      simd_float3{southBounds.sideX0, southBounds.maxY, southBounds.southZ},
      simd_float3{southBounds.sideX1, southBounds.maxY, southBounds.southZ},
      simd_float3{southBounds.sideX1, southBounds.minY, southBounds.southZ},
  };
  std::array<simd_float3, 4> east3d = {
      simd_float3{eastBounds.eastX, eastBounds.minY, eastBounds.sideZ1},
      simd_float3{eastBounds.eastX, eastBounds.maxY, eastBounds.sideZ1},
      simd_float3{eastBounds.eastX, eastBounds.maxY, eastBounds.sideZ0},
      simd_float3{eastBounds.eastX, eastBounds.minY, eastBounds.sideZ0},
  };
  // Hide tiny raster crack between top and side faces by slightly overlapping side tops.
  constexpr float extraInset = 1.0f / 128.0f;
  south3d[1].y += extraInset;
  south3d[2].y += extraInset;
  east3d[1].y += extraInset;
  east3d[2].y += extraInset;

  if (mc::render::isCactusRenderTile(tile)) {
    constexpr float cactusInset = 1.0f / 64.0f;
    east3d[0].z += cactusInset;
    east3d[1].z += cactusInset;
    east3d[2].y += cactusInset;
    south3d[1].y += cactusInset;
    south3d[2].x += cactusInset;
    south3d[3].x += cactusInset;
  }

  auto projectIso = [](const simd_float3& p) {
    const float u = (p.x - p.z) * 0.90f;
    const float v = (p.x + p.z) * 0.5f - p.y;
    return simd_float2{u, v};
  };
  CGFloat minU = CGFLOAT_MAX;
  CGFloat minV = CGFLOAT_MAX;
  CGFloat maxU = -CGFLOAT_MAX;
  CGFloat maxV = -CGFLOAT_MAX;
  auto includeProjected = [&](const simd_float3& p) {
    const simd_float2 q = projectIso(p);
    minU = std::min(minU, static_cast<CGFloat>(q.x));
    minV = std::min(minV, static_cast<CGFloat>(q.y));
    maxU = std::max(maxU, static_cast<CGFloat>(q.x));
    maxV = std::max(maxV, static_cast<CGFloat>(q.y));
  };
  for (const simd_float3& p : top3d) includeProjected(p);
  for (const simd_float3& p : south3d) includeProjected(p);
  for (const simd_float3& p : east3d) includeProjected(p);
  const CGFloat spanU = std::max<CGFloat>(0.0001, maxU - minU);
  const CGFloat spanV = std::max<CGFloat>(0.0001, maxV - minV);
  const CGFloat fit = 0.90;
  const CGFloat sx = (localRect.size.width * fit) / spanU;
  const CGFloat sy = (localRect.size.height * fit) / spanV;
  const CGFloat scale = std::min(sx, sy);
  const CGFloat centerU = (minU + maxU) * 0.5;
  const CGFloat centerV = (minV + maxV) * 0.5;
  auto mapPoint = [&](const simd_float3& p) {
    const simd_float2 q = projectIso(p);
    return NSMakePoint(NSMidX(localRect) - (q.x - centerU) * scale, NSMidY(localRect) - (q.y - centerV) * scale);
  };
  std::array<NSPoint, 4> topFace = {mapPoint(top3d[0]), mapPoint(top3d[1]), mapPoint(top3d[2]), mapPoint(top3d[3])};
  std::array<NSPoint, 4> southFace = {mapPoint(south3d[0]), mapPoint(south3d[3]), mapPoint(south3d[2]), mapPoint(south3d[1])};
  std::array<NSPoint, 4> eastFace = {mapPoint(east3d[0]), mapPoint(east3d[3]), mapPoint(east3d[2]), mapPoint(east3d[1])};

  drawTexturedQuad(southFace, mc::render::BlockFace::South, 0.72);
  drawTexturedQuad(eastFace, mc::render::BlockFace::East, 0.84);
  drawTexturedQuad(topFace, mc::render::BlockFace::Top, 1.00);
  [icon unlockFocus];
  _cubeIconCache[key] = icon;
}

- (void)drawTileIcon:(int)tile inRect:(NSRect)slotRect {
  if (!_terrainImage || tile <= 0) {
    return;
  }
  const NSRect dst = centeredSquareInRect(slotRect, 0.86);
  const NSInteger w = std::max<NSInteger>(1, static_cast<NSInteger>(std::llround(dst.size.width)));
  const NSInteger h = std::max<NSInteger>(1, static_cast<NSInteger>(std::llround(dst.size.height)));
  [self ensureFlatIconCachedForTile:tile width:w height:h];
  NSImage* icon = _flatIconCache[[self iconCacheKeyForTile:tile width:w height:h]];
  [icon drawInRect:dst fromRect:NSZeroRect operation:NSCompositingOperationSourceOver fraction:1.0];
}

- (void)drawHotbarBarAt:(NSRect)barRect selectedIndex:(int)selectedIndex {
  if (_hotbarBackImage) {
    [_hotbarBackImage drawInRect:barRect fromRect:NSZeroRect operation:NSCompositingOperationSourceOver fraction:1.0];
  }
  if (_hotbarSelectedImage && selectedIndex >= 0 && selectedIndex < mc::Inventory::kHotbarSize) {
    const CGFloat slotStep = 20.0 * kUiScale;
    const CGFloat selectedSize = 24.0 * kUiScale;
    const CGFloat selectedX = barRect.origin.x + selectedIndex * slotStep - kUiScale;
    const CGFloat selectedY = barRect.origin.y - kUiScale;
    const NSRect selectedRect = NSMakeRect(selectedX, selectedY, selectedSize, selectedSize);
    [_hotbarSelectedImage drawInRect:selectedRect fromRect:NSZeroRect operation:NSCompositingOperationSourceOver fraction:1.0];
  }
}

- (void)drawCount:(int)count inRect:(NSRect)slotRect {
  if (count <= 1) {
    return;
  }
  const CGFloat fontSize = std::max<CGFloat>(8.0 * kUiScale, std::floor(slotRect.size.height * 0.34));
  NSString* text = [NSString stringWithFormat:@"%d", count];
  NSDictionary* attrs = @{
    NSFontAttributeName : minecraftHudFont(fontSize),
    NSForegroundColorAttributeName : [NSColor colorWithWhite:1.0 alpha:0.98],
  };
  NSSize size = [text sizeWithAttributes:attrs];
  const NSPoint p = NSMakePoint(std::floor(NSMaxX(slotRect) - size.width - 1.0),
                                std::floor(NSMinY(slotRect) + 0.0));
  [NSGraphicsContext saveGraphicsState];
  NSRectClip(slotRect);
  [text drawAtPoint:NSMakePoint(p.x + 1.0, p.y - 1.0)
     withAttributes:@{
       NSFontAttributeName : attrs[NSFontAttributeName],
       NSForegroundColorAttributeName : [NSColor colorWithWhite:0.0 alpha:0.85],
     }];
  [text drawAtPoint:p withAttributes:attrs];
  [NSGraphicsContext restoreGraphicsState];
}

- (void)drawCubeItem:(int)tile inRect:(NSRect)slotRect {
  if (!_terrainImage || tile <= 0) {
    return;
  }

  const NSRect dst = centeredSquareInRect(slotRect, 0.96);
  const NSInteger w = std::max<NSInteger>(1, static_cast<NSInteger>(std::llround(dst.size.width)));
  const NSInteger h = std::max<NSInteger>(1, static_cast<NSInteger>(std::llround(dst.size.height)));
  [self ensureCubeIconCachedForTile:tile width:w height:h];
  NSImage* icon = _cubeIconCache[[self iconCacheKeyForTile:tile width:w height:h]];
  [icon drawInRect:dst fromRect:NSZeroRect operation:NSCompositingOperationSourceOver fraction:1.0];
}

- (void)drawSlotContents:(const mc::Inventory::Slot&)slot inRect:(NSRect)slotRect {
  if (slot.count <= 0 || slot.tile <= 0) {
    return;
  }
  if (isFlatItemTile(slot.tile)) {
    [self drawTileIcon:slot.tile inRect:slotRect];
  } else {
    [self drawCubeItem:slot.tile inRect:slotRect];
  }
  [self drawCount:slot.count inRect:slotRect];
}

- (void)drawInventorySlotHighlightInRect:(NSRect)slotRect {
  if (_slotHighlightImage) {
    NSGraphicsContext.currentContext.imageInterpolation = NSImageInterpolationNone;
    const NSRect src = mc::app::ui::atlasSrcRectTopLeftPixels(_slotHighlightImage, 0.0, 18.0, 18.0, 18.0);
    [_slotHighlightImage drawInRect:slotRect fromRect:src operation:NSCompositingOperationSourceOver fraction:1.0];
    return;
  }
  [[NSColor colorWithCalibratedWhite:1.0 alpha:0.18] setFill];
  NSRectFillUsingOperation(NSInsetRect(slotRect, 1.0, 1.0), NSCompositingOperationSourceOver);
  [[NSColor colorWithCalibratedWhite:1.0 alpha:0.34] setFill];
  NSRectFill(NSMakeRect(NSMinX(slotRect), NSMaxY(slotRect) - 1.0, slotRect.size.width, 1.0));
  NSRectFill(NSMakeRect(NSMinX(slotRect), NSMinY(slotRect), 1.0, slotRect.size.height));
  [[NSColor colorWithCalibratedWhite:0.0 alpha:0.30] setFill];
  NSRectFill(NSMakeRect(NSMinX(slotRect), NSMinY(slotRect), slotRect.size.width, 1.0));
  NSRectFill(NSMakeRect(NSMaxX(slotRect) - 1.0, NSMinY(slotRect), 1.0, slotRect.size.height));
}

- (void)drawRect:(NSRect)dirtyRect {
  (void)dirtyRect;

  const CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
  if (_hotbarTooltipTile > 0 && now >= _hotbarTooltipUntil) {
    _hotbarTooltipTile = 0;
    _hotbarTooltipUntil = 0.0;
  }

  const NSRect b = self.bounds;
  [[NSColor clearColor] setFill];
  NSRectFillUsingOperation(b, NSCompositingOperationClear);
  NSGraphicsContext.currentContext.imageInterpolation = NSImageInterpolationNone;
  const CGFloat hotbarWidth = 182.0 * kUiScale;
  const CGFloat hotbarHeight = 22.0 * kUiScale;
  const CGFloat hotbarX = std::floor((NSWidth(b) - hotbarWidth) * 0.5);
  const CGFloat hotbarY = std::floor(kHotbarBottomMargin);
  const NSRect hotbarRect = NSMakeRect(hotbarX, hotbarY, hotbarWidth, hotbarHeight);

  if (_inventoryOpen) {
    [[NSColor colorWithCalibratedWhite:0.0 alpha:0.34] setFill];
    NSRectFillUsingOperation(b, NSCompositingOperationSourceOver);

    const NSRect panelRect = [self inventoryPanelRectForBounds:b];

    if (_inventoryPanelImage) {
      [_inventoryPanelImage drawInRect:panelRect fromRect:NSZeroRect operation:NSCompositingOperationSourceOver fraction:1.0];
    } else {
      [[NSColor colorWithCalibratedWhite:0.16 alpha:0.88] setFill];
      NSRectFill(panelRect);
      [[NSColor colorWithCalibratedWhite:0.92 alpha:0.96] setStroke];
      NSFrameRectWithWidth(panelRect, 1.0);
    }

    if (_hoveredSlotIndex >= 0 && _hoveredSlotIndex < mc::Inventory::kTotalSlots) {
      const NSRect hoverRect = [self inventorySlotRectForIndex:_hoveredSlotIndex panelRect:panelRect];
      [self drawInventorySlotHighlightInRect:hoverRect];
    }
    for (int i = 0; i < mc::Inventory::kTotalSlots; ++i) {
      const NSRect slotRect = [self inventorySlotRectForIndex:i panelRect:panelRect];
      const mc::Inventory::Slot slot = _slots[static_cast<std::size_t>(i)];
      [self drawSlotContents:slot inRect:slotRect];
    }
  }

  [self drawHotbarBarAt:hotbarRect selectedIndex:_selectedHotbar];
  const CGFloat iconSize = 18.0 * kUiScale;
  const CGFloat slotStep = 20.0 * kUiScale;
  const CGFloat iconX0 = hotbarX + (slotStep - iconSize) * 0.5 + kUiScale;
  const CGFloat iconY = hotbarY + (slotStep - iconSize) * 0.5 + kUiScale;
  for (int i = 0; i < mc::Inventory::kHotbarSize; ++i) {
    const NSRect iconRect = NSMakeRect(iconX0 + i * slotStep, iconY, iconSize, iconSize);
    const mc::Inventory::Slot slot = _slots[static_cast<std::size_t>(i)];
    [self drawSlotContents:slot inRect:iconRect];
  }

  auto drawTooltipLabel = ^(NSString* text, NSPoint anchor, bool centered) {
    if (!text || text.length == 0) {
      return;
    }
    const CGFloat fontSize = std::floor(8.0 * kUiScale);
    NSDictionary* attrs = @{
      NSFontAttributeName : minecraftHudFont(fontSize),
      NSForegroundColorAttributeName : [NSColor colorWithWhite:1.0 alpha:0.98],
    };
    const NSSize sz = [text sizeWithAttributes:attrs];
    CGFloat x = centered ? std::floor(anchor.x - sz.width * 0.5) : std::floor(anchor.x);
    CGFloat y = std::floor(anchor.y);
    const NSRect bg = NSInsetRect(NSMakeRect(x, y, sz.width, sz.height), -4.0, -2.0);
    [[NSColor colorWithCalibratedWhite:0.0 alpha:0.72] setFill];
    NSRectFill(bg);
    [text drawAtPoint:NSMakePoint(std::floor(x), std::floor(y)) withAttributes:attrs];
  };

  if (!_inventoryOpen && _hotbarTooltipTile > 0 && now < _hotbarTooltipUntil) {
    drawTooltipLabel(itemNameForTile(_hotbarTooltipTile), NSMakePoint(NSMidX(hotbarRect), NSMaxY(hotbarRect) + 8.0), true);
  }

  if (_inventoryOpen) {
    int tooltipTile = 0;
    if (_hoveredSlotIndex >= 0 && _hoveredSlotIndex < mc::Inventory::kTotalSlots) {
      const mc::Inventory::Slot hovered = _slots[static_cast<std::size_t>(_hoveredSlotIndex)];
      tooltipTile = hovered.tile;
    } else if (_carriedTile > 0 && _carriedCount > 0) {
      tooltipTile = _carriedTile;
    }
    if (tooltipTile > 0) {
      NSString* text = itemNameForTile(tooltipTile);
      drawTooltipLabel(text, NSMakePoint(_cursorPoint.x + 14.0, _cursorPoint.y + 18.0), false);
    }
  }

  if (_inventoryOpen && _carriedTile > 0 && _carriedCount > 0) {
    const CGFloat size = kInventorySlotPx * kUiScale;
    const NSRect cursorRect = NSMakeRect(std::floor(_cursorPoint.x - size * 0.5), std::floor(_cursorPoint.y - size * 0.5), size, size);
    const mc::Inventory::Slot carriedSlot{_carriedTile, _carriedCount};
    [self drawSlotContents:carriedSlot inRect:cursorRect];
  }

}

@end
