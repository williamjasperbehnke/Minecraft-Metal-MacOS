#pragma once

#import <Cocoa/Cocoa.h>

#include <array>
#include <functional>

#include "Client/Core/InputState.h"
#include "Client/Inventory/Inventory.h"

@class InventoryView;

namespace mc {
class Minecraft;
class RenderDebugController;
}

namespace mc::app {

class AppInputState {
public:
  bool handleMovementKeyEvent(NSEvent* event, BOOL isPressed, RenderDebugController* debugController, Minecraft* game);
  bool handleScrollWheelEvent(NSEvent* event, Minecraft* game);
  bool handleInventoryMouseEvent(NSEvent* event, Minecraft* game, InventoryView* inventoryView);
  bool handleInventoryKeyDownEvent(NSEvent* event, Minecraft* game, InventoryView* inventoryView);
  int takePendingHotbarTooltipTile();
  void handleModifierFlagsChanged(NSEventModifierFlags flags);
  void handleLeftMouse(bool pressed, Minecraft* game);
  void handleRightMouse(bool pressed, Minecraft* game);
  void resetForFocusLoss(Minecraft* game);
  void setInventoryOpen(bool open, Minecraft* game);
  void advancePlacement(double dtSeconds, const std::function<void()>& placeAction);
  void advanceItemDrop(double dtSeconds, Minecraft* game);

  InputState currentInputState() const;
  bool leftMouseHeld() const { return leftMouseHeld_; }

private:
  void clearDropRepeatState();

  BOOL moveForward_ = NO;
  BOOL moveBackward_ = NO;
  BOOL moveLeft_ = NO;
  BOOL moveRight_ = NO;
  BOOL jump_ = NO;
  BOOL sprintHeld_ = NO;
  BOOL sprintLatched_ = NO;
  BOOL crouch_ = NO;
  BOOL leftMouseHeld_ = NO;
  BOOL rightMouseHeld_ = NO;
  BOOL inventoryLeftMouseHeld_ = NO;
  BOOL inventoryLeftDragSplit_ = NO;
  int inventoryLeftDownSlot_ = -1;
  std::array<bool, mc::Inventory::kTotalSlots> inventoryRightDragVisited_{};
  CFAbsoluteTime lastForwardTapTime_ = 0.0;
  double placeRepeatAccumulator_ = 0.0;
  double dropRepeatAccumulator_ = 0.0;
  BOOL dropKeyHeld_ = NO;
  BOOL dropStackHeld_ = NO;
  int pendingHotbarTooltipTile_ = 0;
  BOOL inventoryOpen_ = NO;
};

}  // namespace mc::app
