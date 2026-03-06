#pragma once

#import <Cocoa/Cocoa.h>

#include <functional>

#include "Client/Core/InputState.h"

namespace mc {
class Minecraft;
class RenderDebugController;
}

namespace mc::app {

class AppInputState {
public:
  bool handleMovementKeyEvent(NSEvent* event, BOOL isPressed, RenderDebugController* debugController, Minecraft* game);
  void handleModifierFlagsChanged(NSEventModifierFlags flags);
  void handleLeftMouse(bool pressed, Minecraft* game);
  void handleRightMouse(bool pressed, Minecraft* game);
  void resetForFocusLoss(Minecraft* game);
  void setInventoryOpen(bool open, Minecraft* game);
  void advancePlacement(double dtSeconds, const std::function<void()>& placeAction);

  InputState currentInputState() const;
  bool leftMouseHeld() const { return leftMouseHeld_; }

private:
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
  CFAbsoluteTime lastForwardTapTime_ = 0.0;
  double placeRepeatAccumulator_ = 0.0;
};

}  // namespace mc::app
