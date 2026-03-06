#pragma once

#include <simd/simd.h>

#include "World/Tile/Tile.h"

namespace mc::render {

enum class BlockFace : int {
  Top = 0,
  Bottom = 1,
  North = 2,
  South = 3,
  West = 4,
  East = 5,
};

inline bool isPlantRenderTile(int tile) {
  return tile == static_cast<int>(TileId::TallGrass) || tile == static_cast<int>(TileId::Fern) ||
         tile == static_cast<int>(TileId::DeadBush) || tile == static_cast<int>(TileId::FlowerYellow) ||
         tile == static_cast<int>(TileId::FlowerRed) || tile == static_cast<int>(TileId::MushroomBrown) ||
         tile == static_cast<int>(TileId::MushroomRed) || tile == static_cast<int>(TileId::SugarCane);
}

inline bool isLeafRenderTile(int tile) {
  return tile == static_cast<int>(TileId::Leaves) || tile == static_cast<int>(TileId::SpruceLeaves) ||
         tile == static_cast<int>(TileId::BirchLeaves);
}

inline bool isGrassTintedRenderTile(int tile) {
  return tile == static_cast<int>(TileId::Grass) || tile == static_cast<int>(TileId::TallGrass) ||
         tile == static_cast<int>(TileId::Fern);
}

inline bool isCactusRenderTile(int tile) {
  return tile == static_cast<int>(TileId::Cactus);
}

inline bool isWaterRenderTile(int tile) {
  return tile == static_cast<int>(TileId::Water);
}

inline bool shouldTintFace(int tile, BlockFace face) {
  if (tile == static_cast<int>(TileId::Grass)) {
    return face == BlockFace::Top;
  }
  return isLeafRenderTile(tile);
}

inline simd_float3 biomeTintForBlock(int tile, bool allowGrassTint) {
  if (!isLeafRenderTile(tile) && !isGrassTintedRenderTile(tile)) {
    return {1.0f, 1.0f, 1.0f};
  }
  if (tile == static_cast<int>(TileId::Grass) && !allowGrassTint) {
    return {1.0f, 1.0f, 1.0f};
  }
  return {0.42f, 0.72f, 0.29f};
}

inline bool usesBiomeTint(int tile, bool allowGrassTint) {
  const simd_float3 tint = biomeTintForBlock(tile, allowGrassTint);
  return tint.x < 0.999f || tint.y < 0.999f || tint.z < 0.999f;
}

inline bool cutoutChromaKeyGreen(float r, float g, float b) {
  return (g > 0.85f && r < 0.16f && b < 0.16f);
}

inline int textureForTileFace(int tile, BlockFace face) {
  if (tile == static_cast<int>(TileId::Cactus)) {
    if (face == BlockFace::Top) {
      return 69;
    }
    if (face == BlockFace::Bottom) {
      return 71;
    }
    return 70;
  }
  if (tile == static_cast<int>(TileId::Ice)) {
    return 67;
  }
  if (tile == static_cast<int>(TileId::Snow)) {
    if (face == BlockFace::Top) {
      return 66;
    }
    if (face == BlockFace::Bottom) {
      return 2;
    }
    return 68;
  }
  if (tile == static_cast<int>(TileId::Glass)) {
    return 49;
  }
  if (tile == static_cast<int>(TileId::Sandstone)) {
    if (face == BlockFace::Top) {
      return 176;
    }
    if (face == BlockFace::Bottom) {
      return 208;
    }
    return 192;
  }
  if (tile == static_cast<int>(TileId::Gravel)) {
    return 19;
  }
  if (tile == static_cast<int>(TileId::Cobblestone)) {
    return 16;
  }
  if (tile == static_cast<int>(TileId::Planks)) {
    return 4;
  }
  if (tile == static_cast<int>(TileId::CoalOre)) {
    return 34;
  }
  if (tile == static_cast<int>(TileId::IronOre)) {
    return 33;
  }
  if (tile == static_cast<int>(TileId::GoldOre)) {
    return 32;
  }
  if (tile == static_cast<int>(TileId::DiamondOre)) {
    return 50;
  }
  if (tile == static_cast<int>(TileId::Clay)) {
    return 72;
  }
  if (tile == static_cast<int>(TileId::Water)) {
    return 205;
  }
  if (tile == static_cast<int>(TileId::Sand)) {
    return 18;
  }
  if (tile == static_cast<int>(TileId::Bedrock)) {
    return 17;
  }
  if (tile == static_cast<int>(TileId::Wood)) {
    return (face == BlockFace::Top || face == BlockFace::Bottom) ? 21 : 20;
  }
  if (tile == static_cast<int>(TileId::SpruceWood)) {
    return (face == BlockFace::Top || face == BlockFace::Bottom) ? 21 : 116;
  }
  if (tile == static_cast<int>(TileId::BirchWood)) {
    return (face == BlockFace::Top || face == BlockFace::Bottom) ? 21 : 117;
  }
  if (tile == static_cast<int>(TileId::Leaves)) {
    return 53;
  }
  if (tile == static_cast<int>(TileId::SpruceLeaves)) {
    return 132;
  }
  if (tile == static_cast<int>(TileId::BirchLeaves)) {
    return 133;
  }
  if (tile == static_cast<int>(TileId::TallGrass)) {
    return 39;
  }
  if (tile == static_cast<int>(TileId::Fern)) {
    return 56;
  }
  if (tile == static_cast<int>(TileId::DeadBush)) {
    return 55;
  }
  if (tile == static_cast<int>(TileId::FlowerYellow)) {
    return 13;
  }
  if (tile == static_cast<int>(TileId::FlowerRed)) {
    return 12;
  }
  if (tile == static_cast<int>(TileId::MushroomBrown)) {
    return 29;
  }
  if (tile == static_cast<int>(TileId::MushroomRed)) {
    return 28;
  }
  if (tile == static_cast<int>(TileId::SugarCane)) {
    return 73;
  }
  if (tile == static_cast<int>(TileId::Stone)) {
    return 1;
  }
  if (tile == static_cast<int>(TileId::Dirt)) {
    return 2;
  }
  if (tile == static_cast<int>(TileId::Grass)) {
    if (face == BlockFace::Top) {
      return 0;
    }
    if (face == BlockFace::Bottom) {
      return 2;
    }
    return 3;
  }
  return 1;
}

inline float cactusSideInsetForFace(int tile, BlockFace face) {
  if (!isCactusRenderTile(tile)) {
    return 0.0f;
  }
  if (face == BlockFace::North || face == BlockFace::South || face == BlockFace::West || face == BlockFace::East) {
    return 1.0f / 16.0f;
  }
  return 0.0f;
}

inline float faceTopYForTile(int tile, float y, float inflate, bool waterHasSameAbove) {
  if (isWaterRenderTile(tile)) {
    if (waterHasSameAbove) {
      return y + 1.0f + inflate;
    }
    return y + (14.0f / 16.0f) + inflate;
  }
  return y + 1.0f + inflate;
}

struct FaceBounds {
  float x0 = 0.0f;
  float x1 = 1.0f;
  float z0 = 0.0f;
  float z1 = 1.0f;
  float minY = 0.0f;
  float maxY = 1.0f;
  float topX0 = 0.0f;
  float topX1 = 1.0f;
  float topZ0 = 0.0f;
  float topZ1 = 1.0f;
  float sideX0 = 0.0f;
  float sideX1 = 1.0f;
  float sideZ0 = 0.0f;
  float sideZ1 = 1.0f;
  float northZ = 0.0f;
  float southZ = 1.0f;
  float westX = 0.0f;
  float eastX = 1.0f;
};

inline FaceBounds computeFaceBounds(int tile, BlockFace face, float x, float y, float z, float inflate, bool waterHasSameAbove) {
  const bool isCactus = isCactusRenderTile(tile);
  const float cactusSideInset = cactusSideInsetForFace(tile, face);
  const float cactusCornerTrim = isCactus ? 0.016f : 0.0f;

  FaceBounds bounds;
  bounds.x0 = x - inflate;
  bounds.x1 = x + 1.0f + inflate;
  bounds.minY = y - inflate;
  bounds.maxY = faceTopYForTile(tile, y, inflate, waterHasSameAbove);
  bounds.z0 = z - inflate;
  bounds.z1 = z + 1.0f + inflate;
  bounds.sideX0 = bounds.x0 + cactusCornerTrim;
  bounds.sideX1 = bounds.x1 - cactusCornerTrim;
  bounds.sideZ0 = bounds.z0 + cactusCornerTrim;
  bounds.sideZ1 = bounds.z1 - cactusCornerTrim;
  bounds.topX0 = isCactus ? bounds.sideX0 : bounds.x0;
  bounds.topX1 = isCactus ? bounds.sideX1 : bounds.x1;
  bounds.topZ0 = isCactus ? bounds.sideZ0 : bounds.z0;
  bounds.topZ1 = isCactus ? bounds.sideZ1 : bounds.z1;
  bounds.northZ = bounds.z0 + cactusSideInset;
  bounds.southZ = bounds.z1 - cactusSideInset;
  bounds.westX = bounds.x0 + cactusSideInset;
  bounds.eastX = bounds.x1 - cactusSideInset;
  return bounds;
}

}  // namespace mc::render
