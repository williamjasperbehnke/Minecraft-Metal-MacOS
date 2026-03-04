#pragma once

#include <cstddef>
#include <string>

#include "Client/Core/Minecraft.h"
#include "Client/Debug/RenderDebugController.h"
#include "Client/GameMode/GameMode.h"
#include "World/Level/Gen/BiomeProvider.h"

namespace mc::app {

struct DebugHudData {
  double fps = 0.0;
  double frameMs = 0.0;
  RenderDebugController::RenderMode renderMode = RenderDebugController::RenderMode::Textured;
  GameModeType gameMode = GameModeType::Survival;
  int playerX = 0;
  int playerY = 0;
  int playerZ = 0;
  float yawDegrees = 0.0f;
  float pitchDegrees = 0.0f;
  const char* biomeName = "Unknown";
  const char* lookedBlockName = "none";
  bool hasLookTarget = false;
  int lookX = 0;
  int lookY = 0;
  int lookZ = 0;
  std::size_t loadedChunks = 0;
  std::size_t pendingChunks = 0;
  std::size_t readyChunks = 0;
  std::size_t generationThreads = 0;
  unsigned long opaqueVertices = 0;
  unsigned long transparentVertices = 0;
};

class DebugHudFormatter {
public:
  static const char* renderModeName(RenderDebugController::RenderMode mode);
  static const char* gameModeName(GameModeType mode);
  static const char* biomeName(gen::BiomeKind kind);
  static const char* tileName(int tile);
  static const char* facingDirection(float yawDegrees);
  static std::string format(const DebugHudData& data);
};

}  // namespace mc::app
