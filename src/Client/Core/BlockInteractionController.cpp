#include "Client/Core/BlockInteractionController.h"

#include <algorithm>

#include "Client/Render/Metal/LevelRenderer.h"
#include "World/Tile/Tile.h"

namespace mc {

void BlockInteractionController::setBreakHeld(bool held, LevelRenderer* levelRenderer) {
  breakHeld_ = held;
  if (!breakHeld_) {
    clearDestroyState(levelRenderer);
  }
}

void BlockInteractionController::updateLookTarget(const std::optional<Hit>& hit) {
  if (hit.has_value()) {
    lookTargetActive_ = true;
    lookTargetX_ = hit->x;
    lookTargetY_ = hit->y;
    lookTargetZ_ = hit->z;
  } else {
    lookTargetActive_ = false;
  }
}

bool BlockInteractionController::lookTargetBlock(int* x, int* y, int* z) const {
  if (!lookTargetActive_) {
    return false;
  }
  if (x) *x = lookTargetX_;
  if (y) *y = lookTargetY_;
  if (z) *z = lookTargetZ_;
  return true;
}

bool BlockInteractionController::interactAtCrosshair(bool place, const std::optional<Hit>& hit,
                                                     const std::function<bool(int, int, int)>& placeBlockAt,
                                                     const std::function<bool(int, int, int)>& destroyBlockAt) {
  if (!hit.has_value()) {
    return false;
  }
  if (place) {
    return placeBlockAt(hit->prevX, hit->prevY, hit->prevZ);
  }
  return destroyBlockAt(hit->x, hit->y, hit->z);
}

void BlockInteractionController::tickBreaking(double dtSeconds, GameModeType mode, const std::optional<Hit>& hit,
                                              const std::function<int(int, int, int)>& getTileAt,
                                              const std::function<bool(int, int, int)>& destroyBlockAt,
                                              const std::function<std::optional<Hit>()>& reacquireHit,
                                              LevelRenderer* levelRenderer) {
  if (mode == GameModeType::Spectator || !breakHeld_) {
    clearDestroyState(levelRenderer);
    return;
  }

  if (breakCooldownSeconds_ > 0.0f) {
    breakCooldownSeconds_ = std::max(0.0f, breakCooldownSeconds_ - static_cast<float>(dtSeconds));
    clearDestroyState(levelRenderer);
    return;
  }

  if (!hit.has_value()) {
    clearDestroyState(levelRenderer);
    return;
  }

  const int hitTile = getTileAt(hit->x, hit->y, hit->z);
  if (hitTile == static_cast<int>(TileId::Bedrock)) {
    clearDestroyState(levelRenderer);
    return;
  }

  if (mode == GameModeType::Creative) {
    // Creative breaks blocks instantly with no crack animation.
    if (destroyBlockAt(hit->x, hit->y, hit->z)) {
      breakCooldownSeconds_ = kBreakDelaySeconds;
    }
    clearDestroyState(levelRenderer);
    return;
  }

  if (!destroyActive_ || hit->x != destroyX_ || hit->y != destroyY_ || hit->z != destroyZ_) {
    destroyActive_ = true;
    destroyX_ = hit->x;
    destroyY_ = hit->y;
    destroyZ_ = hit->z;
    // Start each newly targeted block at stage 0 so the crack animation
    // is visible immediately for continuous hold-mining.
    destroyProgress_ = 0.1f;
    destroyStage_ = 0;
    if (levelRenderer) {
      levelRenderer->setDestroyProgress(destroyX_, destroyY_, destroyZ_, destroyStage_);
    }
  }

  destroyProgress_ += static_cast<float>(dtSeconds) * kDestroyRate;
  const int stage = static_cast<int>(destroyProgress_ * 10.0f) - 1;
  if (stage != destroyStage_) {
    destroyStage_ = stage;
    if (levelRenderer) {
      if (destroyStage_ >= 0 && destroyStage_ < 10) {
        levelRenderer->setDestroyProgress(destroyX_, destroyY_, destroyZ_, destroyStage_);
      } else {
        levelRenderer->clearDestroyProgress();
      }
    }
  }

  if (destroyProgress_ < 1.0f) {
    return;
  }

  if (!destroyBlockAt(destroyX_, destroyY_, destroyZ_)) {
    clearDestroyState(levelRenderer);
    return;
  }

  breakCooldownSeconds_ = kBreakDelaySeconds;
  destroyActive_ = false;
  destroyProgress_ = 0.0f;
  destroyStage_ = -1;
  if (levelRenderer) {
    levelRenderer->clearDestroyProgress();
  }

  if (!breakHeld_ || breakCooldownSeconds_ > 0.0f) {
    return;
  }
  const std::optional<Hit> nextHit = reacquireHit();
  if (!nextHit.has_value()) {
    return;
  }

  destroyActive_ = true;
  destroyX_ = nextHit->x;
  destroyY_ = nextHit->y;
  destroyZ_ = nextHit->z;
  destroyProgress_ = 0.1f;
  destroyStage_ = 0;
  if (levelRenderer) {
    levelRenderer->setDestroyProgress(destroyX_, destroyY_, destroyZ_, destroyStage_);
  }
}

void BlockInteractionController::clearDestroyState(LevelRenderer* levelRenderer) {
  if (!destroyActive_ && destroyStage_ < 0 && destroyProgress_ <= 0.0f) {
    return;
  }

  destroyActive_ = false;
  destroyProgress_ = 0.0f;
  destroyStage_ = -1;
  if (levelRenderer) {
    levelRenderer->clearDestroyProgress();
  }
}

}  // namespace mc
