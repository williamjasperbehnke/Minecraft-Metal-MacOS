#pragma once

#include <memory>
#include <optional>

#include "Client/Core/BlockInteractionController.h"
#include "Client/Core/InputState.h"
#include "Client/Core/PlayerController.h"
#include "Client/GameMode/GameMode.h"
#include "Client/Render/Metal/MetalRenderer.h"

namespace mc {

class GameMode;
class Inventory;
class Level;
class LevelRenderer;
class LocalPlayer;

class Minecraft {
public:
  Minecraft();
  ~Minecraft();

  void init(MetalRenderer* renderer);
  void tick(double dtSeconds);

  void setInputState(const InputState& input);
  void setCreativeMode(bool enabled);
  void toggleCreativeMode();
  void setSpectatorMode(bool enabled);
  void toggleSpectatorMode();
  bool isCreativeMode() const;
  bool isSpectatorMode() const;
  void addLookInput(float deltaX, float deltaY);
  void setBreakHeld(bool held);
  void setViewAspect(float aspect);
  void toggleInventory();
  void setInventoryOpen(bool open);
  bool isInventoryOpen() const;
  void selectHotbarSlot(int slotIndex);
  int selectedHotbarSlot() const;
  int selectedPlaceTile() const;
  void dropSelectedHotbarItem(bool dropStack);
  const Inventory& inventory() const;
  int inventoryCarriedTile() const;
  int inventoryCarriedCount() const;
  void inventoryLeftClickSlot(int slotIndex, bool shiftHeld, bool isDoubleClick);
  void inventoryRightClickSlot(int slotIndex);
  void inventoryMiddleClickSlot(int slotIndex);
  void inventoryLeftClickOutside();
  void inventoryRightClickOutside();
  void inventoryHotbarSwap(int slotIndex, int hotbarIndex);
  void inventoryDropFromSlot(int slotIndex, bool dropStack);
  void inventoryBeginDragSplit();
  void inventoryDragSplitAddSlot(int slotIndex);
  void inventoryEndDragSplit();
  bool inventoryIsDragSplitActive() const;

  bool interactAtCrosshair(bool place);
  TerrainViewParams viewParams(float aspect) const;
  int renderCenterChunkX() const { return loadedChunkX_; }
  int renderCenterChunkZ() const { return loadedChunkZ_; }
  int renderDistanceChunks() const { return kRenderDistanceChunks; }
  bool lookTargetBlock(int* x, int* y, int* z) const;
  simd_float3 cameraWorldPosition() const;
  float lookYawDegrees() const;
  float lookPitchDegrees() const;
  bool isCameraUnderwater() const;

  Level* level() const { return level_.get(); }

private:
  bool destroyBlockAt(int x, int y, int z);
  bool placeBlockAt(int x, int y, int z);

  simd_float3 cameraPosition() const;
  simd_float3 forwardVector() const;
  simd_float3 rightVector() const;
  simd_float3 upVector() const;

  bool raycast(float normalizedX, float normalizedY, float aspect, int* hitX, int* hitY, int* hitZ,
               int* prevX, int* prevY, int* prevZ) const;
  std::optional<BlockInteractionController::Hit> raycastCrosshairHit() const;
  GameModeType currentGameModeType() const;
  void updateWorldStreaming(double dtSeconds);
  void updateRendererState();
  void rebuildGameMode(GameModeType mode);

  std::unique_ptr<Level> level_;
  std::unique_ptr<GameMode> gameMode_;
  std::unique_ptr<LevelRenderer> levelRenderer_;
  std::unique_ptr<LocalPlayer> localPlayer_;
  MetalRenderer* renderer_ = nullptr;

  PlayerController playerController_;
  BlockInteractionController blockInteractionController_;
  int loadedChunkX_ = 0;
  int loadedChunkZ_ = 0;
  bool chunksPrimed_ = false;

  float viewAspect_ = 16.0f / 9.0f;
  float currentFovRadians_ = 70.0f * 3.1415926535f / 180.0f;

  static constexpr float kEyeHeight = 1.62f;
  static constexpr float kCrouchEyeOffset = 0.25f;
  static constexpr int kRenderDistanceChunks = 16;
};

}  // namespace mc
