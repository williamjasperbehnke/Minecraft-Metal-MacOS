#pragma once

namespace mc {

class Level;

class LevelListener {
public:
  virtual ~LevelListener() = default;
  virtual void tileChanged(int x, int y, int z) = 0;
  virtual void setTilesDirty(int x0, int y0, int z0, int x1, int y1, int z1, Level* level) = 0;
  virtual void allChanged() = 0;
};

}  // namespace mc
