#include "Client/GameMode/SurvivalGameMode.h"

namespace mc {

bool SurvivalGameMode::destroyBlockAt(int x, int y, int z) {
  return destroyBlockAtDefault(x, y, z);
}

bool SurvivalGameMode::placeBlockAt(int x, int y, int z) {
  return placeBlockAtDefault(x, y, z);
}

}  // namespace mc
