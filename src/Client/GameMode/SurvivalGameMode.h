#pragma once

#include "Client/GameMode/GameMode.h"

namespace mc {

class SurvivalGameMode : public GameMode {
public:
  using GameMode::GameMode;

  GameModeType type() const override { return GameModeType::Survival; }
};

}  // namespace mc
