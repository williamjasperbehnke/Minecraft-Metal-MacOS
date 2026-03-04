#include "Client/GameMode/SpectatorGameMode.h"

namespace mc {

bool SpectatorGameMode::destroyBlockAt(int /*x*/, int /*y*/, int /*z*/) {
  return false;
}

bool SpectatorGameMode::placeBlockAt(int /*x*/, int /*y*/, int /*z*/) {
  return false;
}

}  // namespace mc
