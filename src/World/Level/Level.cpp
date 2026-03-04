#include "World/Level/Level.h"

#include <algorithm>
#include <cstdint>
#include <thread>

#include "World/Chunk/LevelChunk.h"
#include "World/Level/Gen/Decorators.h"
#include "World/Tile/Tile.h"

namespace mc {

namespace {

struct ChunkLocalPos {
  int chunkX = 0;
  int chunkZ = 0;
  int localX = 0;
  int localZ = 0;
};

ChunkLocalPos toChunkLocal(int x, int z) {
  ChunkLocalPos out;
  out.chunkX = x >= 0 ? x / LevelChunk::kSizeX : (x - (LevelChunk::kSizeX - 1)) / LevelChunk::kSizeX;
  out.chunkZ = z >= 0 ? z / LevelChunk::kSizeZ : (z - (LevelChunk::kSizeZ - 1)) / LevelChunk::kSizeZ;
  out.localX = (x % LevelChunk::kSizeX + LevelChunk::kSizeX) % LevelChunk::kSizeX;
  out.localZ = (z % LevelChunk::kSizeZ + LevelChunk::kSizeZ) % LevelChunk::kSizeZ;
  return out;
}

}  // namespace

Level::Level()
    : chunkSource_(std::make_unique<TerrainChunkSource>(1337u)),
      generationPool_(std::max<std::size_t>(
          2, std::thread::hardware_concurrency() > 1
                 ? static_cast<std::size_t>((std::thread::hardware_concurrency() * 2) / 3)
                 : static_cast<std::size_t>(2))) {
}
Level::~Level() = default;

std::int64_t Level::chunkKey(int chunkX, int chunkZ) {
  return (static_cast<std::int64_t>(chunkX) << 32) ^ (static_cast<std::uint32_t>(chunkZ));
}

LevelChunk* Level::getChunk(int x, int z) {
  applyGeneratedChunks(6);

  const auto key = chunkKey(x, z);
  auto it = chunks_.find(key);
  if (it == chunks_.end()) {
    pendingChunks_.erase(key);
    auto chunk = std::make_unique<LevelChunk>(this, x, z);
    if (chunkSource_) {
      chunkSource_->fillChunk(*chunk);
    }
    auto* ptr = chunk.get();
    chunks_[key] = std::move(chunk);
    return ptr;
  }
  return it->second.get();
}

const LevelChunk* Level::getChunk(int x, int z) const {
  const auto key = chunkKey(x, z);
  auto it = chunks_.find(key);
  if (it == chunks_.end()) {
    return nullptr;
  }
  return it->second.get();
}

int Level::getTile(int x, int y, int z) const {
  if (y < minBuildHeight || y >= maxBuildHeight) {
    return 0;
  }

  const ChunkLocalPos pos = toChunkLocal(x, z);

  const LevelChunk* chunk = getChunk(pos.chunkX, pos.chunkZ);
  if (!chunk) {
    return static_cast<int>(TileId::Air);
  }
  return chunk->getTile(pos.localX, y, pos.localZ);
}

bool Level::isEmptyTile(int x, int y, int z) const {
  return getTile(x, y, z) == static_cast<int>(TileId::Air);
}

bool Level::setTile(int x, int y, int z, int tile) {
  applyGeneratedChunks(6);

  if (y < minBuildHeight || y >= maxBuildHeight) {
    return false;
  }

  const ChunkLocalPos pos = toChunkLocal(x, z);

  LevelChunk* chunk = getChunk(pos.chunkX, pos.chunkZ);
  if (!chunk->setTile(pos.localX, y, pos.localZ, static_cast<std::uint8_t>(tile))) {
    return false;
  }

  for (auto* listener : listeners_) {
    listener->tileChanged(x, y, z);
  }
  return true;
}

int Level::getTopSolidBlockY(int x, int z) const {
  for (int y = maxBuildHeight - 1; y >= minBuildHeight; --y) {
    if (!isEmptyTile(x, y, z)) {
      return y;
    }
  }
  return -1;
}

void Level::addListener(LevelListener* listener) {
  listeners_.push_back(listener);
}

void Level::removeListener(LevelListener* listener) {
  listeners_.erase(std::remove(listeners_.begin(), listeners_.end(), listener), listeners_.end());
}

void Level::generateFlatWorld() {
  for (int chunkX = -4; chunkX <= 4; ++chunkX) {
    for (int chunkZ = -4; chunkZ <= 4; ++chunkZ) {
      LevelChunk* chunk = getChunk(chunkX, chunkZ);
      for (int x = 0; x < LevelChunk::kSizeX; ++x) {
        for (int z = 0; z < LevelChunk::kSizeZ; ++z) {
          chunk->setTile(x, 0, z, static_cast<std::uint8_t>(TileId::Stone));
          chunk->setTile(x, 1, z, static_cast<std::uint8_t>(TileId::Dirt));
          chunk->setTile(x, 2, z, static_cast<std::uint8_t>(TileId::Grass));
        }
      }
    }
  }

  notifyAllChanged();
}

bool Level::ensureChunksInRange(int centerX, int centerZ, int radiusChunks) {
  applyGeneratedChunks(6);

  bool createdAny = false;
  for (int cx = centerX - radiusChunks; cx <= centerX + radiusChunks; ++cx) {
    for (int cz = centerZ - radiusChunks; cz <= centerZ + radiusChunks; ++cz) {
      const auto key = chunkKey(cx, cz);
      if (chunks_.find(key) == chunks_.end()) {
        getChunk(cx, cz);
        createdAny = true;
      }
    }
  }

  return createdAny;
}

int Level::ensureChunksInRangeBudget(int centerX, int centerZ, int radiusChunks, int maxNewChunks) {
  const int applied = applyGeneratedChunks(8);
  if (maxNewChunks <= 0) {
    return applied;
  }

  int queued = 0;
  const auto tryQueue = [&](int cx, int cz) {
    const auto key = chunkKey(cx, cz);
    if (chunks_.find(key) != chunks_.end() || pendingChunks_.find(key) != pendingChunks_.end()) {
      return;
    }
    queueChunkGeneration(cx, cz);
    ++queued;
  };

  tryQueue(centerX, centerZ);
  if (queued >= maxNewChunks) {
    return applied + queued;
  }

  for (int r = 1; r <= radiusChunks; ++r) {
    for (int dx = -r; dx <= r; ++dx) {
      const int dz0 = -r;
      const int dz1 = r;
      tryQueue(centerX + dx, centerZ + dz0);
      if (queued >= maxNewChunks) {
        return applied + queued;
      }
      if (dz1 != dz0) {
        tryQueue(centerX + dx, centerZ + dz1);
        if (queued >= maxNewChunks) {
          return applied + queued;
        }
      }
    }
    for (int dz = -r + 1; dz <= r - 1; ++dz) {
      const int dx0 = -r;
      const int dx1 = r;
      tryQueue(centerX + dx0, centerZ + dz);
      if (queued >= maxNewChunks) {
        return applied + queued;
      }
      if (dx1 != dx0) {
        tryQueue(centerX + dx1, centerZ + dz);
        if (queued >= maxNewChunks) {
          return applied + queued;
        }
      }
    }
  }

  return applied + queued;
}

void Level::setChunkSource(std::unique_ptr<ChunkSource> chunkSource) {
  applyGeneratedChunks(1024);
  chunkSource_ = std::move(chunkSource);
  chunks_.clear();
  pendingChunks_.clear();
  {
    std::lock_guard<std::mutex> lock(generatedMutex_);
    generatedChunks_ = {};
  }
  notifyAllChanged();
}

std::size_t Level::readyChunkCount() const {
  std::lock_guard<std::mutex> lock(generatedMutex_);
  return generatedChunks_.size();
}

void Level::queueChunkGeneration(int chunkX, int chunkZ) {
  const std::int64_t key = chunkKey(chunkX, chunkZ);
  if (pendingChunks_.find(key) != pendingChunks_.end() || chunks_.find(key) != chunks_.end()) {
    return;
  }
  pendingChunks_.insert(key);

  generationPool_.enqueue([this, chunkX, chunkZ, key]() {
    auto chunk = std::make_unique<LevelChunk>(this, chunkX, chunkZ);
    if (chunkSource_) {
      chunkSource_->fillChunk(*chunk);
    }
    std::lock_guard<std::mutex> lock(generatedMutex_);
    generatedChunks_.emplace(key, std::move(chunk));
  });
}

int Level::applyGeneratedChunks(int maxApply) {
  if (maxApply <= 0) {
    return 0;
  }

  int applied = 0;
  while (applied < maxApply) {
    std::pair<std::int64_t, std::unique_ptr<LevelChunk>> generated;
    {
      std::lock_guard<std::mutex> lock(generatedMutex_);
      if (generatedChunks_.empty()) {
        break;
      }
      generated = std::move(generatedChunks_.front());
      generatedChunks_.pop();
    }

    const std::int64_t key = generated.first;
    pendingChunks_.erase(key);
    if (chunks_.find(key) == chunks_.end()) {
      const int chunkX = generated.second->chunkX();
      const int chunkZ = generated.second->chunkZ();
      chunks_[key] = std::move(generated.second);

      // Deterministically flush cross-chunk vegetation writes regardless of
      // generation order (center chunk + immediate neighbors).
      for (int dx = -1; dx <= 1; ++dx) {
        for (int dz = -1; dz <= 1; ++dz) {
          const auto nk = chunkKey(chunkX + dx, chunkZ + dz);
          auto nit = chunks_.find(nk);
          if (nit != chunks_.end()) {
            mc::gen::applyDeferredDecorationWrites(*nit->second);
          }
        }
      }

      const int x0 = chunkX * LevelChunk::kSizeX;
      const int z0 = chunkZ * LevelChunk::kSizeZ;
      const int x1 = x0 + LevelChunk::kSizeX - 1;
      const int z1 = z0 + LevelChunk::kSizeZ - 1;
      for (auto* listener : listeners_) {
        listener->setTilesDirty(x0, minBuildHeight, z0, x1, maxBuildHeight - 1, z1, this);
      }
      ++applied;
    }
  }

  return applied;
}

void Level::notifyAllChanged() {
  for (auto* listener : listeners_) {
    listener->allChanged();
  }
}

}  // namespace mc
