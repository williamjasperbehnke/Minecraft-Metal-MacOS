#pragma once

namespace mc {

class LevelSource {
public:
  virtual ~LevelSource() = default;
  virtual int getTile(int x, int y, int z) const = 0;
  virtual bool isEmptyTile(int x, int y, int z) const = 0;
};

}  // namespace mc
