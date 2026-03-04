#import "Client/App/AppInputState.h"

#include "Client/Core/Minecraft.h"
#include "Client/Debug/RenderDebugController.h"

namespace mc::app {

namespace {

constexpr double kPlaceRepeatInterval = 0.16;
constexpr double kSprintTapWindowSeconds = 0.28;

enum KeyCode : unsigned short {
  KeyA = 0,
  KeyS = 1,
  KeyD = 2,
  KeyG = 5,
  KeyV = 9,
  KeyB = 11,
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

}  // namespace

bool AppInputState::handleMovementKeyEvent(NSEvent* event, BOOL isPressed, RenderDebugController* debugController,
                                           Minecraft* game) {
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

void AppInputState::handleModifierFlagsChanged(NSEventModifierFlags flags) {
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
