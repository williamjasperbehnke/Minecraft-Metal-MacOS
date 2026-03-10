#include "Client/Debug/RenderDebugController.h"

namespace mc {

void RenderDebugController::toggleChunkBorders() {
  showChunkBorders_ = !showChunkBorders_;
}

void RenderDebugController::toggleItemHitboxes() {
  showItemHitboxes_ = !showItemHitboxes_;
}

void RenderDebugController::cycleRenderMode() {
  switch (renderMode_) {
    case RenderMode::Textured:
      renderMode_ = RenderMode::Flat;
      break;
    case RenderMode::Flat:
      renderMode_ = RenderMode::Wireframe;
      break;
    case RenderMode::Wireframe:
      renderMode_ = RenderMode::Textured;
      break;
  }
}

}  // namespace mc
