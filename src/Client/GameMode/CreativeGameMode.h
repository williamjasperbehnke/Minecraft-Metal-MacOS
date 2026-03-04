#pragma once

#include "Client/GameMode/GameMode.h"

namespace mc {

class CreativeGameMode : public GameMode {
public:
  using GameMode::GameMode;

  bool destroyBlockAt(int x, int y, int z) override;
  bool placeBlockAt(int x, int y, int z) override;
  GameModeType type() const override { return GameModeType::Creative; }
};

}  // namespace mc
