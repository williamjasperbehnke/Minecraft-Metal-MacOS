#pragma once

namespace mc {

struct InputState {
  bool moveForward = false;
  bool moveBackward = false;
  bool moveLeft = false;
  bool moveRight = false;
  bool jump = false;
  bool sprint = false;
  bool crouch = false;
};

}  // namespace mc
