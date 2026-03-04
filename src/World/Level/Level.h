#pragma once

#include <cstdint>
#include <mutex>
#include <memory>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "World/Level/ChunkSource.h"
#include "World/Level/LevelListener.h"
#include "World/Level/LevelSource.h"
#include "World/Level/ThreadPool.h"

namespace mc {

class LevelChunk;

class Level : public LevelSource {
public:
  static constexpr int minBuildHeight = 0;
  static constexpr int maxBuildHeight = 128;

  Level();
  ~Level() override;

  int getTile(int x, int y, int z) const override;
  bool isEmptyTile(int x, int y, int z) const override;
  bool setTile(int x, int y, int z, int tile);
  int getTopSolidBlockY(int x, int z) const;
  bool ensureChunksInRange(int centerX, int centerZ, int radiusChunks);
  int ensureChunksInRangeBudget(int centerX, int centerZ, int radiusChunks, int maxNewChunks);
  void setChunkSource(std::unique_ptr<ChunkSource> chunkSource);
  std::size_t loadedChunkCount() const { return chunks_.size(); }
  std::size_t pendingChunkCount() const { return pendingChunks_.size(); }
  std::size_t readyChunkCount() const;
  std::size_t generationThreadCount() const { return generationPool_.threadCount(); }

  LevelChunk* getChunk(int x, int z);
  const LevelChunk* getChunk(int x, int z) const;

  void addListener(LevelListener* listener);
  void removeListener(LevelListener* listener);
  void generateFlatWorld();

private:
  static std::int64_t chunkKey(int chunkX, int chunkZ);
  void notifyAllChanged();
  void queueChunkGeneration(int chunkX, int chunkZ);
  int applyGeneratedChunks(int maxApply);

  std::unordered_map<std::int64_t, std::unique_ptr<LevelChunk>> chunks_;
  std::unordered_set<std::int64_t> pendingChunks_;
  std::queue<std::pair<std::int64_t, std::unique_ptr<LevelChunk>>> generatedChunks_;
  mutable std::mutex generatedMutex_;
  std::vector<LevelListener*> listeners_;
  std::unique_ptr<ChunkSource> chunkSource_;
  ThreadPool generationPool_;
};

}  // namespace mc
