#pragma once

namespace mc {

class Level;
class Minecraft;
class Player;

enum class GameModeType {
  Survival,
  Creative,
  Spectator,
};

class GameMode {
public:
  explicit GameMode(Minecraft* minecraft);
  virtual ~GameMode() = default;

  virtual void initLevel(Level* level) { level_ = level; }
  virtual void initPlayer(Player* player) { player_ = player; }
  virtual bool destroyBlockAt(int x, int y, int z) = 0;
  virtual bool placeBlockAt(int x, int y, int z) = 0;
  virtual GameModeType type() const = 0;
  bool isCreative() const { return type() == GameModeType::Creative; }
  bool isSpectator() const { return type() == GameModeType::Spectator; }

protected:
  bool hasLevelAndPlayer() const;
  bool withinBlockReach(int x, int y, int z) const;
  bool inBuildHeight(int y) const;
  bool canReplaceForPlacement(int tile) const;
  bool blocksPlayerPlacement(int x, int y, int z) const;
  bool canDestroyTile(int tile) const;
  bool destroyBlockAtDefault(int x, int y, int z);
  bool placeBlockAtDefault(int x, int y, int z);
  int placeTile() const;

  Minecraft* minecraft_;
  Level* level_ = nullptr;
  Player* player_ = nullptr;
};

}  // namespace mc
