#include "Client/GameMode/CreativeGameMode.h"

namespace mc {

bool CreativeGameMode::destroyBlockAt(int x, int y, int z) {
  return destroyBlockAtDefault(x, y, z);
}

bool CreativeGameMode::placeBlockAt(int x, int y, int z) {
  return placeBlockAtDefault(x, y, z);
}

}  // namespace mc
