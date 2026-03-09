#import "Client/App/AppInputState.h"

#import "Client/App/InventoryView.h"

#include <algorithm>
#include <cmath>

#include "Client/Core/Minecraft.h"
#include "Client/Debug/RenderDebugController.h"

namespace mc::app {

namespace {

constexpr double kPlaceRepeatInterval = 0.16;
constexpr double kSprintTapWindowSeconds = 0.28;

enum KeyCode : unsigned short {
  Key1 = 18,
  Key2 = 19,
  Key3 = 20,
  Key4 = 21,
  Key5 = 23,
  Key6 = 22,
  Key7 = 26,
  Key8 = 28,
  Key9 = 25,
  KeyA = 0,
  KeyE = 14,
  KeyS = 1,
  KeyD = 2,
  KeyG = 5,
  KeyV = 9,
  KeyB = 11,
  KeyQ = 12,
  KeyW = 13,
  KeyM = 46,
  KeySpace = 49,
  KeyLeftControl = 59,
  KeyRightShift = 60,
  KeyRightControl = 62,
  KeyLeftShift = 56,
  KeyLeftArrow = 123,
  KeyRightArrow = 124,
  KeyDownArrow = 125,
  KeyUpArrow = 126,
};

int hotbarIndexForKeyCode(unsigned short keyCode) {
  switch (keyCode) {
    case Key1: return 0;
    case Key2: return 1;
    case Key3: return 2;
    case Key4: return 3;
    case Key5: return 4;
    case Key6: return 5;
    case Key7: return 6;
    case Key8: return 7;
    case Key9: return 8;
    default: return -1;
  }
}

}  // namespace

bool AppInputState::handleInventoryMouseEvent(NSEvent* event, Minecraft* game, InventoryView* inventoryView) {
  if (!game || !inventoryView || !game->isInventoryOpen()) {
    return false;
  }

  NSPoint local = [inventoryView convertPoint:event.locationInWindow fromView:nil];
  const int hoveredSlot = [inventoryView slotIndexAtPoint:local];
  const bool shiftHeld = ((event.modifierFlags & NSEventModifierFlagShift) != 0);

  switch (event.type) {
    case NSEventTypeMouseMoved:
      return true;
    case NSEventTypeLeftMouseDragged:
      if (inventoryLeftMouseHeld_ == YES && game->inventoryCarriedCount() > 0) {
        if (!inventoryLeftDragSplit_) {
          inventoryLeftDragSplit_ = YES;
          game->inventoryBeginDragSplit();
          if (inventoryLeftDownSlot_ >= 0) {
            game->inventoryDragSplitAddSlot(inventoryLeftDownSlot_);
          }
        }
        if (hoveredSlot >= 0) {
          game->inventoryDragSplitAddSlot(hoveredSlot);
        }
      }
      return true;
    case NSEventTypeRightMouseDragged:
      if (hoveredSlot >= 0 && game->inventoryCarriedCount() > 0 &&
          hoveredSlot < static_cast<int>(inventoryRightDragVisited_.size()) &&
          !inventoryRightDragVisited_[static_cast<std::size_t>(hoveredSlot)]) {
        game->inventoryRightClickSlot(hoveredSlot);
        inventoryRightDragVisited_[static_cast<std::size_t>(hoveredSlot)] = true;
      }
      return true;
    case NSEventTypeLeftMouseDown:
      inventoryLeftMouseHeld_ = YES;
      inventoryLeftDragSplit_ = NO;
      inventoryLeftDownSlot_ = hoveredSlot;
      if (shiftHeld) {
        if (hoveredSlot >= 0) {
          game->inventoryLeftClickSlot(hoveredSlot, true, false);
        } else {
          game->inventoryLeftClickOutside();
        }
      }
      return true;
    case NSEventTypeLeftMouseUp:
      if (inventoryLeftMouseHeld_ == YES) {
        if (inventoryLeftDragSplit_ && game->inventoryIsDragSplitActive()) {
          game->inventoryEndDragSplit();
        } else if (!shiftHeld) {
          const bool isDoubleClick = (event.clickCount >= 2);
          if (hoveredSlot >= 0) {
            game->inventoryLeftClickSlot(hoveredSlot, false, isDoubleClick);
          } else {
            game->inventoryLeftClickOutside();
          }
        }
      }
      inventoryLeftMouseHeld_ = NO;
      inventoryLeftDragSplit_ = NO;
      inventoryLeftDownSlot_ = -1;
      return true;
    case NSEventTypeRightMouseDown:
      inventoryRightDragVisited_.fill(false);
      if (hoveredSlot >= 0) {
        game->inventoryRightClickSlot(hoveredSlot);
        if (hoveredSlot < static_cast<int>(inventoryRightDragVisited_.size())) {
          inventoryRightDragVisited_[static_cast<std::size_t>(hoveredSlot)] = true;
        }
      } else {
        game->inventoryRightClickOutside();
      }
      return true;
    case NSEventTypeRightMouseUp:
      inventoryRightDragVisited_.fill(false);
      return true;
    case NSEventTypeOtherMouseDown:
      if (event.buttonNumber == 2 && hoveredSlot >= 0) {
        game->inventoryMiddleClickSlot(hoveredSlot);
      }
      return true;
    case NSEventTypeOtherMouseUp:
      return true;
    default:
      return false;
  }
}

bool AppInputState::handleInventoryKeyDownEvent(NSEvent* event, Minecraft* game, InventoryView* inventoryView) {
  if (!game || !inventoryView || !game->isInventoryOpen() || event.type != NSEventTypeKeyDown || event.isARepeat) {
    return false;
  }
  const int hoveredSlot = [inventoryView hoveredSlotIndex];
  if (hoveredSlot < 0) {
    return false;
  }
  const int hotbar = hotbarIndexForKeyCode(event.keyCode);
  if (hotbar >= 0) {
    game->inventoryHotbarSwap(hoveredSlot, hotbar);
    return true;
  }
  if (event.keyCode == KeyQ) {
    const bool dropStack = ((event.modifierFlags & NSEventModifierFlagControl) != 0);
    game->inventoryDropFromSlot(hoveredSlot, dropStack);
    return true;
  }
  return false;
}

bool AppInputState::handleMovementKeyEvent(NSEvent* event, BOOL isPressed, RenderDebugController* debugController,
                                           Minecraft* game) {
  if (isPressed && !event.isARepeat && game) {
    if (event.keyCode == KeyE) {
      game->toggleInventory();
      setInventoryOpen(game->isInventoryOpen(), game);
      return true;
    }
    const int hotbarIndex = hotbarIndexForKeyCode(event.keyCode);
    if (hotbarIndex >= 0) {
      game->selectHotbarSlot(hotbarIndex);
      pendingHotbarTooltipTile_ = game->selectedPlaceTile();
      return true;
    }
  }

  if (game && game->isInventoryOpen()) {
    return true;
  }

  switch (event.keyCode) {
    case KeyW:
    case KeyUpArrow:
      moveForward_ = isPressed;
      if (isPressed && !event.isARepeat) {
        const CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
        if ((now - lastForwardTapTime_) <= kSprintTapWindowSeconds) {
          sprintLatched_ = YES;
        }
        lastForwardTapTime_ = now;
      } else if (!isPressed) {
        sprintLatched_ = NO;
      }
      return true;
    case KeyS:
    case KeyDownArrow:
      moveBackward_ = isPressed;
      if (isPressed) {
        sprintLatched_ = NO;
      }
      return true;
    case KeyA:
    case KeyLeftArrow:
      moveLeft_ = isPressed;
      return true;
    case KeyD:
    case KeyRightArrow:
      moveRight_ = isPressed;
      return true;
    case KeySpace:
      jump_ = isPressed;
      return true;
    case KeyLeftShift:
    case KeyRightShift:
      crouch_ = isPressed;
      if (isPressed) {
        sprintLatched_ = NO;
      }
      return true;
    case KeyLeftControl:
    case KeyRightControl:
      sprintHeld_ = isPressed;
      return true;
    case KeyB:
      if (isPressed && !event.isARepeat && debugController) {
        debugController->toggleChunkBorders();
      }
      return true;
    case KeyM:
      if (isPressed && !event.isARepeat && debugController) {
        debugController->cycleRenderMode();
      }
      return true;
    case KeyG:
      if (isPressed && !event.isARepeat && game) {
        game->toggleCreativeMode();
      }
      return true;
    case KeyV:
      if (isPressed && !event.isARepeat && game) {
        game->toggleSpectatorMode();
      }
      return true;
    default:
      return false;
  }
}

bool AppInputState::handleScrollWheelEvent(NSEvent* event, Minecraft* game) {
  if (!event || !game || game->isInventoryOpen()) {
    return false;
  }
  if (std::abs(event.scrollingDeltaY) < 0.01) {
    return true;
  }
  const int delta = (event.scrollingDeltaY > 0.0) ? -1 : 1;
  const int current = std::clamp(game->selectedHotbarSlot(), 0, 8);
  const int next = (current + delta + 9) % 9;
  game->selectHotbarSlot(next);
  pendingHotbarTooltipTile_ = game->selectedPlaceTile();
  return true;
}

int AppInputState::takePendingHotbarTooltipTile() {
  const int tile = pendingHotbarTooltipTile_;
  pendingHotbarTooltipTile_ = 0;
  return tile;
}

void AppInputState::handleModifierFlagsChanged(NSEventModifierFlags flags) {
  if (inventoryOpen_) {
    crouch_ = NO;
    sprintHeld_ = NO;
    sprintLatched_ = NO;
    return;
  }
  const NSEventModifierFlags f = (flags & NSEventModifierFlagDeviceIndependentFlagsMask);
  crouch_ = ((f & NSEventModifierFlagShift) != 0);
  sprintHeld_ = ((f & NSEventModifierFlagControl) != 0);
  if (crouch_) {
    sprintLatched_ = NO;
  }
}

void AppInputState::handleLeftMouse(bool pressed, Minecraft* game) {
  leftMouseHeld_ = pressed ? YES : NO;
  if (game) {
    game->setBreakHeld(pressed);
  }
}

void AppInputState::handleRightMouse(bool pressed, Minecraft* game) {
  rightMouseHeld_ = pressed ? YES : NO;
  placeRepeatAccumulator_ = 0.0;
  if (pressed && game) {
    game->interactAtCrosshair(true);
  }
}

void AppInputState::resetForFocusLoss(Minecraft* game) {
  leftMouseHeld_ = NO;
  rightMouseHeld_ = NO;
  sprintHeld_ = NO;
  sprintLatched_ = NO;
  crouch_ = NO;
  inventoryLeftMouseHeld_ = NO;
  inventoryLeftDragSplit_ = NO;
  inventoryLeftDownSlot_ = -1;
  inventoryRightDragVisited_.fill(false);
  placeRepeatAccumulator_ = 0.0;
  pendingHotbarTooltipTile_ = 0;
  if (game) {
    game->setBreakHeld(false);
    game->inventoryEndDragSplit();
    game->setInventoryOpen(false);
  }
}

void AppInputState::setInventoryOpen(bool open, Minecraft* game) {
  inventoryOpen_ = open ? YES : NO;
  inventoryLeftMouseHeld_ = NO;
  inventoryLeftDragSplit_ = NO;
  inventoryLeftDownSlot_ = -1;
  inventoryRightDragVisited_.fill(false);
  pendingHotbarTooltipTile_ = 0;
  if (game) {
    game->inventoryEndDragSplit();
  }
  if (!open) {
    return;
  }
  leftMouseHeld_ = NO;
  rightMouseHeld_ = NO;
  jump_ = NO;
  moveForward_ = NO;
  moveBackward_ = NO;
  moveLeft_ = NO;
  moveRight_ = NO;
  sprintHeld_ = NO;
  sprintLatched_ = NO;
  placeRepeatAccumulator_ = 0.0;
  if (game) {
    game->setBreakHeld(false);
  }
}

void AppInputState::advancePlacement(double dtSeconds, const std::function<void()>& placeAction) {
  if (!rightMouseHeld_) {
    placeRepeatAccumulator_ = 0.0;
    return;
  }

  placeRepeatAccumulator_ += dtSeconds;
  int repeats = 0;
  while (placeRepeatAccumulator_ >= kPlaceRepeatInterval && repeats < 3) {
    placeRepeatAccumulator_ -= kPlaceRepeatInterval;
    placeAction();
    ++repeats;
  }
}

InputState AppInputState::currentInputState() const {
  InputState input;
  input.moveForward = (moveForward_ == YES);
  input.moveBackward = (moveBackward_ == YES);
  input.moveLeft = (moveLeft_ == YES);
  input.moveRight = (moveRight_ == YES);
  input.jump = (jump_ == YES);
  input.crouch = (crouch_ == YES);
  input.sprint = (sprintHeld_ == YES) || (sprintLatched_ == YES);
  return input;
}

}  // namespace mc::app
