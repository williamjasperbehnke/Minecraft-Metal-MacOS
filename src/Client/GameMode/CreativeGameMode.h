#pragma once

#include "Client/GameMode/GameMode.h"

namespace mc {

class CreativeGameMode : public GameMode {
public:
  using GameMode::GameMode;

  GameModeType type() const override { return GameModeType::Creative; }
};

}  // namespace mc
