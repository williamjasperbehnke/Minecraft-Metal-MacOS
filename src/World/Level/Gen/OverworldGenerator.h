#pragma once

#include <cstdint>
#include <memory>

namespace mc {

class LevelChunk;

namespace gen {

class OverworldGenerator {
public:
  explicit OverworldGenerator(std::uint32_t seed);
  ~OverworldGenerator();

  void fillChunk(LevelChunk& chunk);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace gen

}  // namespace mc
