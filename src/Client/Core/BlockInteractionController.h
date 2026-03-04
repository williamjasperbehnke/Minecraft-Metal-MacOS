#pragma once

#include <functional>
#include <optional>

#include "Client/GameMode/GameMode.h"

namespace mc {

class LevelRenderer;

class BlockInteractionController {
public:
  struct Hit {
    int x = 0;
    int y = 0;
    int z = 0;
    int prevX = 0;
    int prevY = 0;
    int prevZ = 0;
  };

  void setBreakHeld(bool held, LevelRenderer* levelRenderer);
  bool breakHeld() const { return breakHeld_; }

  void updateLookTarget(const std::optional<Hit>& hit);
  bool lookTargetBlock(int* x, int* y, int* z) const;

  bool interactAtCrosshair(bool place, const std::optional<Hit>& hit, const std::function<bool(int, int, int)>& placeBlockAt,
                           const std::function<bool(int, int, int)>& destroyBlockAt);

  void tickBreaking(double dtSeconds, GameModeType mode, const std::optional<Hit>& hit,
                    const std::function<int(int, int, int)>& getTileAt, const std::function<bool(int, int, int)>& destroyBlockAt,
                    const std::function<std::optional<Hit>()>& reacquireHit, LevelRenderer* levelRenderer);

private:
  void clearDestroyState(LevelRenderer* levelRenderer);

  bool breakHeld_ = false;

  bool destroyActive_ = false;
  int destroyX_ = 0;
  int destroyY_ = 0;
  int destroyZ_ = 0;
  float destroyProgress_ = 0.0f;
  int destroyStage_ = -1;
  float miningParticleCooldownSeconds_ = 0.0f;
  float breakCooldownSeconds_ = 0.0f;

  bool lookTargetActive_ = false;
  int lookTargetX_ = 0;
  int lookTargetY_ = 0;
  int lookTargetZ_ = 0;

  static constexpr float kDestroyRate = 1.75f;
  static constexpr float kBreakDelaySeconds = 4.0f / 20.0f;  // 4 game ticks
  static constexpr float kMiningParticleIntervalSeconds = 1.0f / 30.0f;
};

}  // namespace mc
