#include "Client/App/DebugHudFormatter.h"

#include <array>
#include <cmath>
#include <sstream>

#include "World/Tile/Tile.h"

namespace mc::app {

const char* DebugHudFormatter::renderModeName(RenderDebugController::RenderMode mode) {
  switch (mode) {
    case RenderDebugController::RenderMode::Textured: return "Textured";
    case RenderDebugController::RenderMode::Flat: return "Flat";
    case RenderDebugController::RenderMode::Wireframe: return "Wireframe";
  }
  return "Textured";
}

const char* DebugHudFormatter::gameModeName(GameModeType mode) {
  switch (mode) {
    case GameModeType::Survival: return "Survival";
    case GameModeType::Creative: return "Creative";
    case GameModeType::Spectator: return "Spectator";
  }
  return "Survival";
}

const char* DebugHudFormatter::biomeName(gen::BiomeKind kind) {
  switch (kind) {
    case gen::BiomeKind::Ocean: return "Ocean";
    case gen::BiomeKind::River: return "River";
    case gen::BiomeKind::Plains: return "Plains";
    case gen::BiomeKind::Forest: return "Forest";
    case gen::BiomeKind::Desert: return "Desert";
    case gen::BiomeKind::Taiga: return "Taiga";
    case gen::BiomeKind::Mountains: return "Mountains";
  }
  return "Unknown";
}

const char* DebugHudFormatter::tileName(int tile) {
  switch (static_cast<TileId>(tile)) {
    case TileId::Air: return "Air";
    case TileId::Grass: return "Grass";
    case TileId::Dirt: return "Dirt";
    case TileId::Stone: return "Stone";
    case TileId::Water: return "Water";
    case TileId::Sand: return "Sand";
    case TileId::Bedrock: return "Bedrock";
    case TileId::Wood: return "Wood";
    case TileId::Leaves: return "Leaves";
    case TileId::Cobblestone: return "Cobblestone";
    case TileId::Planks: return "Planks";
    case TileId::Gravel: return "Gravel";
    case TileId::Sandstone: return "Sandstone";
    case TileId::Ice: return "Ice";
    case TileId::Snow: return "Snow";
    case TileId::Clay: return "Clay";
    case TileId::Glass: return "Glass";
    case TileId::CoalOre: return "Coal Ore";
    case TileId::IronOre: return "Iron Ore";
    case TileId::GoldOre: return "Gold Ore";
    case TileId::DiamondOre: return "Diamond Ore";
    case TileId::Cactus: return "Cactus";
    case TileId::SpruceWood: return "Spruce Wood";
    case TileId::BirchWood: return "Birch Wood";
    case TileId::SpruceLeaves: return "Spruce Leaves";
    case TileId::BirchLeaves: return "Birch Leaves";
    case TileId::TallGrass: return "Tall Grass";
    case TileId::Fern: return "Fern";
    case TileId::DeadBush: return "Dead Bush";
    case TileId::FlowerYellow: return "Dandelion";
    case TileId::FlowerRed: return "Rose";
    case TileId::MushroomBrown: return "Brown Mushroom";
    case TileId::MushroomRed: return "Red Mushroom";
    case TileId::SugarCane: return "Sugar Cane";
  }
  return "Unknown";
}

const char* DebugHudFormatter::facingDirection(float yawDegrees) {
  static constexpr std::array<const char*, 8> kDirections = {
      "S", "SW", "W", "NW", "N", "NE", "E", "SE",
  };
  float normalized = std::fmod(yawDegrees, 360.0f);
  if (normalized < 0.0f) {
    normalized += 360.0f;
  }
  const int idx = static_cast<int>((normalized + 22.5f) / 45.0f) & 7;
  return kDirections[static_cast<std::size_t>(idx)];
}

std::string DebugHudFormatter::format(const DebugHudData& data) {
  std::ostringstream out;
  out << "XYZ: " << data.playerX << " / " << data.playerY << " / " << data.playerZ;
  out << "   Facing: " << facingDirection(data.yawDegrees);
  out << "   Yaw/Pitch: " << data.yawDegrees << " / " << data.pitchDegrees << "\n";
  out << "FPS: " << data.fps << " (" << data.frameMs << " ms)\n";
  out << "Render: " << renderModeName(data.renderMode);
  out << "   Mode: " << gameModeName(data.gameMode);
  out << "   Biome: " << data.biomeName << "\n";
  out << "Looking At: ";
  if (data.hasLookTarget) {
    out << data.lookedBlockName << " @ " << data.lookX << ", " << data.lookY << ", " << data.lookZ;
  } else {
    out << "none";
  }
  out << "\n";
  out << "Chunks: L " << data.loadedChunks << "  P " << data.pendingChunks << "  R " << data.readyChunks;
  out << "  Threads " << data.generationThreads << "\n";
  out << "Verts: O " << data.opaqueVertices << "  T " << data.transparentVertices;
  return out.str();
}

}  // namespace mc::app
