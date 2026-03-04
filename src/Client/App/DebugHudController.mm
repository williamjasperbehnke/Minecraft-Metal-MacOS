#import "Client/App/DebugHudController.h"

#include <cmath>
#include <limits>
#include <string>

#include "Client/Core/Minecraft.h"
#include "World/Level/Level.h"

namespace mc::app {

namespace {

constexpr double kHudRefreshSeconds = 0.15;
constexpr int kBiomeSamplesPerChunk = 256;
constexpr float kFloorEpsilon = 0.0001f;

}  // namespace

DebugHudController::DebugHudController()
    : biomeProvider_(1337u), cachedBiomeChunkX_(std::numeric_limits<int>::max()),
      cachedBiomeChunkZ_(std::numeric_limits<int>::max()) {}

void DebugHudController::attachToView(NSView* parentView, NSRect windowFrame) {
  const NSRect debugPanelFrame = NSMakeRect(8, windowFrame.size.height - 148, 700, 138);
  panel_ = [[NSView alloc] initWithFrame:debugPanelFrame];
  panel_.autoresizingMask = NSViewMaxXMargin | NSViewMinYMargin;
  panel_.wantsLayer = YES;
  panel_.layer.backgroundColor = NSColor.clearColor.CGColor;
  [parentView addSubview:panel_];

  NSRect debugTextFrame = NSMakeRect(8, 6, debugPanelFrame.size.width - 16, debugPanelFrame.size.height - 12);
  shadowLabel_ = [[NSTextField alloc] initWithFrame:NSOffsetRect(debugTextFrame, 1.0, -1.0)];
  label_ = [[NSTextField alloc] initWithFrame:debugTextFrame];

  NSArray<NSTextField*>* labels = @[ shadowLabel_, label_ ];
  for (NSTextField* textField in labels) {
    textField.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    textField.editable = NO;
    textField.selectable = NO;
    textField.bordered = NO;
    textField.drawsBackground = NO;
    textField.usesSingleLineMode = NO;
    textField.maximumNumberOfLines = 0;
    textField.font = [NSFont fontWithName:@"Menlo-Bold" size:12.0] ?: [NSFont monospacedSystemFontOfSize:12 weight:NSFontWeightBold];
    textField.alignment = NSTextAlignmentLeft;
    textField.stringValue = @"";
  }

  shadowLabel_.textColor = [NSColor colorWithWhite:0.0 alpha:0.75];
  label_.textColor = [NSColor colorWithWhite:1.0 alpha:0.98];
  [panel_ addSubview:shadowLabel_];
  [panel_ addSubview:label_];
}

void DebugHudController::tick(double dtRaw, const Minecraft& game, RenderDebugController::RenderMode renderMode,
                              bool hasLookTarget, int lookX, int lookY, int lookZ, NSUInteger opaqueVerts,
                              NSUInteger transparentVerts, double smoothedFrameMs) {
  if (!label_ || !shadowLabel_) {
    return;
  }

  Level* level = game.level();
  if (!level) {
    return;
  }

  accum_ += dtRaw;
  if (accum_ < kHudRefreshSeconds) {
    return;
  }
  accum_ = 0.0;

  const double fps = (smoothedFrameMs > 0.001) ? (1000.0 / smoothedFrameMs) : 0.0;
  const char* targetBlockText = "none";
  if (hasLookTarget) {
    const int lookedTile = level->getTile(lookX, lookY, lookZ);
    targetBlockText = DebugHudFormatter::tileName(lookedTile);
  }

  const simd_float3 cam = game.cameraWorldPosition();
  const int px = static_cast<int>(std::floor(cam.x + kFloorEpsilon));
  const int py = static_cast<int>(std::floor(cam.y + kFloorEpsilon));
  const int pz = static_cast<int>(std::floor(cam.z));
  const int biomeChunkX = floorDiv16(px);
  const int biomeChunkZ = floorDiv16(pz);
  const int biomeLx = mod16(px);
  const int biomeLz = mod16(pz);

  if (biomeChunkX != cachedBiomeChunkX_ || biomeChunkZ != cachedBiomeChunkZ_ ||
      cachedBiomes_.size() != kBiomeSamplesPerChunk) {
    biomeProvider_.sampleChunkBiomes(biomeChunkX, biomeChunkZ, cachedBiomes_);
    cachedBiomeChunkX_ = biomeChunkX;
    cachedBiomeChunkZ_ = biomeChunkZ;
  }

  const gen::BiomeSample biome =
      cachedBiomes_.empty() ? gen::BiomeSample{} : cachedBiomes_[static_cast<std::size_t>(biomeLx + biomeLz * 16)];
  const GameModeType gameMode = game.isSpectatorMode()
                                    ? GameModeType::Spectator
                                    : (game.isCreativeMode() ? GameModeType::Creative : GameModeType::Survival);

  DebugHudData hudData;
  hudData.fps = fps;
  hudData.frameMs = smoothedFrameMs;
  hudData.renderMode = renderMode;
  hudData.gameMode = gameMode;
  hudData.playerX = px;
  hudData.playerY = py;
  hudData.playerZ = pz;
  hudData.yawDegrees = game.lookYawDegrees();
  hudData.pitchDegrees = game.lookPitchDegrees();
  hudData.biomeName = DebugHudFormatter::biomeName(biome.kind);
  hudData.lookedBlockName = targetBlockText;
  hudData.hasLookTarget = hasLookTarget;
  hudData.lookX = lookX;
  hudData.lookY = lookY;
  hudData.lookZ = lookZ;
  hudData.loadedChunks = level->loadedChunkCount();
  hudData.pendingChunks = level->pendingChunkCount();
  hudData.readyChunks = level->readyChunkCount();
  hudData.generationThreads = level->generationThreadCount();
  hudData.opaqueVertices = static_cast<unsigned long>(opaqueVerts);
  hudData.transparentVertices = static_cast<unsigned long>(transparentVerts);

  const std::string hudText = DebugHudFormatter::format(hudData);
  NSString* formatted = [NSString stringWithUTF8String:hudText.c_str()];
  label_.stringValue = formatted;
  shadowLabel_.stringValue = formatted;
}

int DebugHudController::floorDiv16(int v) const {
  return v >= 0 ? v / 16 : (v - 15) / 16;
}

int DebugHudController::mod16(int v) const {
  return (v % 16 + 16) % 16;
}

}  // namespace mc::app
