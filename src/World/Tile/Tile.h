#pragma once

#include <cstdint>

namespace mc {

enum class TileId : std::uint8_t {
  Air = 0,
  Grass = 1,
  Dirt = 2,
  Stone = 3,
  Water = 4,
  Sand = 5,
  Bedrock = 6,
  Wood = 7,
  Leaves = 8,
  Cobblestone = 9,
  Planks = 10,
  Gravel = 11,
  Sandstone = 12,
  Ice = 13,
  Snow = 14,
  Clay = 15,
  Glass = 16,
  CoalOre = 17,
  IronOre = 18,
  GoldOre = 19,
  DiamondOre = 20,
  Cactus = 21,
  SpruceWood = 22,
  BirchWood = 23,
  SpruceLeaves = 24,
  BirchLeaves = 25,
  TallGrass = 26,
  Fern = 27,
  DeadBush = 28,
  FlowerYellow = 29,
  FlowerRed = 30,
  MushroomBrown = 31,
  MushroomRed = 32,
  SugarCane = 33,
};

inline bool isSolidTileId(int tile) {
  switch (static_cast<TileId>(tile)) {
    case TileId::Air:
    case TileId::Water:
    case TileId::TallGrass:
    case TileId::Fern:
    case TileId::DeadBush:
    case TileId::FlowerYellow:
    case TileId::FlowerRed:
    case TileId::MushroomBrown:
    case TileId::MushroomRed:
    case TileId::SugarCane:
      return false;
    default:
      return true;
  }
}

}  // namespace mc
