#pragma once

#include <cstddef>
#include <vector>

#include "Client/Render/Metal/MetalRenderer.h"
#include "World/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Tile/Tile.h"

namespace mc::detail {

constexpr float kAtlasCols = 16.0f;
constexpr float kAtlasRows = 16.0f;
constexpr int kChunkSide = 16;
constexpr int kChunkArea = kChunkSide * kChunkSide;
constexpr int kTileUnknown = -1;

enum class FaceDir : int {
  Top = 0,
  Bottom = 1,
  North = 2,
  South = 3,
  West = 4,
  East = 5,
};

inline bool isTransparentTile(int tile) {
  return tile == static_cast<int>(TileId::Water) || tile == static_cast<int>(TileId::Glass) ||
         tile == static_cast<int>(TileId::Ice) ||
         tile == static_cast<int>(TileId::TallGrass) || tile == static_cast<int>(TileId::Fern) ||
         tile == static_cast<int>(TileId::DeadBush) || tile == static_cast<int>(TileId::FlowerYellow) ||
         tile == static_cast<int>(TileId::FlowerRed) || tile == static_cast<int>(TileId::MushroomBrown) ||
         tile == static_cast<int>(TileId::MushroomRed) || tile == static_cast<int>(TileId::SugarCane);
}

inline bool isPlantTile(int tile) {
  return tile == static_cast<int>(TileId::TallGrass) || tile == static_cast<int>(TileId::Fern) ||
         tile == static_cast<int>(TileId::DeadBush) || tile == static_cast<int>(TileId::FlowerYellow) ||
         tile == static_cast<int>(TileId::FlowerRed) || tile == static_cast<int>(TileId::MushroomBrown) ||
         tile == static_cast<int>(TileId::MushroomRed) || tile == static_cast<int>(TileId::SugarCane);
}

inline bool isCactusTile(int tile) {
  return tile == static_cast<int>(TileId::Cactus);
}

inline bool isNonOccludingTileForFaceCulling(int tile) {
  return isTransparentTile(tile) || isPlantTile(tile) || isCactusTile(tile);
}

inline bool shouldRenderFaceForTile(int tile, int neighborTile) {
  const bool tileTransparent = isNonOccludingTileForFaceCulling(tile);
  if (neighborTile == kTileUnknown) {
    return !tileTransparent;
  }
  if (neighborTile == static_cast<int>(TileId::Air)) {
    return true;
  }

  // Water should behave like a connected volume and avoid emitting
  // internal faces against non-occluding neighbors (flora/cactus/glass/etc.),
  // which can appear as interior seam lines.
  if (tile == static_cast<int>(TileId::Water) && isNonOccludingTileForFaceCulling(neighborTile)) {
    return false;
  }

  const bool neighborTransparent = isNonOccludingTileForFaceCulling(neighborTile);
  if (!tileTransparent) {
    return neighborTransparent;
  }
  if (tile == neighborTile) {
    return false;
  }
  if (neighborTransparent) {
    return true;
  }
  return false;
}

inline simd_float2 atlasTileOrigin(int textureIndex) {
  const int tx = textureIndex % kChunkSide;
  const int ty = textureIndex / kChunkSide;
  return {static_cast<float>(tx) / kAtlasCols, static_cast<float>(ty) / kAtlasRows};
}

inline simd_float3 biomeTintForBlock(int tile, int /*x*/, int /*z*/, bool allowGrassTint) {
  if (tile != static_cast<int>(TileId::Grass) && tile != static_cast<int>(TileId::Leaves) &&
      tile != static_cast<int>(TileId::SpruceLeaves) && tile != static_cast<int>(TileId::BirchLeaves) &&
      tile != static_cast<int>(TileId::TallGrass) && tile != static_cast<int>(TileId::Fern)) {
    return {1.0f, 1.0f, 1.0f};
  }
  if (tile == static_cast<int>(TileId::Grass) && !allowGrassTint) {
    return {1.0f, 1.0f, 1.0f};
  }
  return {0.42f, 0.72f, 0.29f};
}

struct ChunkBuildView {
  const LevelChunk* center = nullptr;
  const LevelChunk* west = nullptr;
  const LevelChunk* east = nullptr;
  const LevelChunk* north = nullptr;
  const LevelChunk* south = nullptr;
  int x0 = 0;
  int z0 = 0;

  int tileAt(int lx, int y, int lz) const {
    if (y < Level::minBuildHeight || y >= Level::maxBuildHeight) {
      return static_cast<int>(TileId::Air);
    }
    if (lx >= 0 && lx < kChunkSide && lz >= 0 && lz < kChunkSide) {
      return center->getTile(lx, y, lz);
    }
    if (lx < 0 && lz >= 0 && lz < kChunkSide) {
      return west ? west->getTile(kChunkSide - 1, y, lz) : kTileUnknown;
    }
    if (lx >= kChunkSide && lz >= 0 && lz < kChunkSide) {
      return east ? east->getTile(0, y, lz) : kTileUnknown;
    }
    if (lz < 0 && lx >= 0 && lx < kChunkSide) {
      return north ? north->getTile(lx, y, kChunkSide - 1) : kTileUnknown;
    }
    if (lz >= kChunkSide && lx >= 0 && lx < kChunkSide) {
      return south ? south->getTile(lx, y, 0) : kTileUnknown;
    }
    return static_cast<int>(TileId::Air);
  }
};

class FloraMesher {
public:
  template <typename TextureLookupFn>
  void build(const ChunkBuildView& view, std::vector<TerrainVertex>& opaqueOut, TextureLookupFn&& textureForFace) const {
    for (int lx = 0; lx < kChunkSide; ++lx) {
      for (int lz = 0; lz < kChunkSide; ++lz) {
        const int worldX = view.x0 + lx;
        const int worldZ = view.z0 + lz;
        for (int y = Level::minBuildHeight; y < Level::maxBuildHeight; ++y) {
          const int tile = view.center->getTile(lx, y, lz);
          if (!isPlantTile(tile)) {
            continue;
          }
          emitPlantCross(opaqueOut, worldX, y, worldZ, tile, textureForFace);
        }
      }
    }
  }

private:
  template <typename TextureLookupFn>
  static void emitPlantCross(std::vector<TerrainVertex>& out, int worldX, int y, int worldZ, int tile,
                             TextureLookupFn&& textureForFace) {
    const float fx = static_cast<float>(worldX);
    const float fy = static_cast<float>(y);
    const float fz = static_cast<float>(worldZ);
    const int tex = textureForFace(tile, FaceDir::North);
    const simd_float2 tileOrigin = atlasTileOrigin(tex);
    const simd_float3 tint = biomeTintForBlock(tile, worldX, worldZ, true);
    const simd_float3 color = {-tint.x, tint.y, tint.z};

    auto pushQuad = [&](simd_float3 a, simd_float3 b, simd_float3 c, simd_float3 d) {
      TerrainVertex v0{};
      TerrainVertex v1{};
      TerrainVertex v2{};
      TerrainVertex v3{};
      v0.position = a;
      v1.position = b;
      v2.position = c;
      v3.position = d;
      v0.color = color;
      v1.color = color;
      v2.color = color;
      v3.color = color;
      v0.uv = {0.0f, 1.0f};
      v1.uv = {1.0f, 1.0f};
      v2.uv = {1.0f, 0.0f};
      v3.uv = {0.0f, 0.0f};
      v0.tileOrigin = tileOrigin;
      v1.tileOrigin = tileOrigin;
      v2.tileOrigin = tileOrigin;
      v3.tileOrigin = tileOrigin;
      out.push_back(v0);
      out.push_back(v1);
      out.push_back(v2);
      out.push_back(v0);
      out.push_back(v2);
      out.push_back(v3);
      // Flora quads must be visible from both sides while terrain culling is enabled.
      out.push_back(v0);
      out.push_back(v2);
      out.push_back(v1);
      out.push_back(v0);
      out.push_back(v3);
      out.push_back(v2);
    };

    constexpr float inset = 0.08f;
    pushQuad(simd_float3{fx + inset, fy, fz + inset}, simd_float3{fx + 1.0f - inset, fy, fz + 1.0f - inset},
             simd_float3{fx + 1.0f - inset, fy + 1.0f, fz + 1.0f - inset}, simd_float3{fx + inset, fy + 1.0f, fz + inset});
    pushQuad(simd_float3{fx + 1.0f - inset, fy, fz + inset}, simd_float3{fx + inset, fy, fz + 1.0f - inset},
             simd_float3{fx + inset, fy + 1.0f, fz + 1.0f - inset}, simd_float3{fx + 1.0f - inset, fy + 1.0f, fz + inset});
  }
};

class TransparentMesher {
public:
  template <typename TextureLookupFn, typename EmitFaceFn>
  void build(const ChunkBuildView& view, std::vector<TerrainVertex>& transparentOut, TextureLookupFn&& textureForFace,
             EmitFaceFn&& emitFace) const {
    for (int lx = 0; lx < kChunkSide; ++lx) {
      for (int lz = 0; lz < kChunkSide; ++lz) {
        const int worldX = view.x0 + lx;
        const int worldZ = view.z0 + lz;
        for (int y = Level::minBuildHeight; y < Level::maxBuildHeight; ++y) {
          const int tile = view.center->getTile(lx, y, lz);
          if (!isTransparentTile(tile) || isPlantTile(tile)) {
            continue;
          }
          if (shouldRenderFaceForTile(tile, view.tileAt(lx, y + 1, lz))) {
            emitFace(transparentOut, worldX, y, worldZ, FaceDir::Top, textureForFace(tile, FaceDir::Top), 1.0f, tile);
          }
          if (!(tile == static_cast<int>(TileId::Bedrock) && y == Level::minBuildHeight) &&
              shouldRenderFaceForTile(tile, view.tileAt(lx, y - 1, lz))) {
            emitFace(transparentOut, worldX, y, worldZ, FaceDir::Bottom, textureForFace(tile, FaceDir::Bottom), 0.55f, tile);
          }
          if (shouldRenderFaceForTile(tile, view.tileAt(lx, y, lz - 1))) {
            emitFace(transparentOut, worldX, y, worldZ, FaceDir::North, textureForFace(tile, FaceDir::North), 0.78f, tile);
          }
          if (shouldRenderFaceForTile(tile, view.tileAt(lx, y, lz + 1))) {
            emitFace(transparentOut, worldX, y, worldZ, FaceDir::South, textureForFace(tile, FaceDir::South), 0.72f, tile);
          }
          if (shouldRenderFaceForTile(tile, view.tileAt(lx - 1, y, lz))) {
            emitFace(transparentOut, worldX, y, worldZ, FaceDir::West, textureForFace(tile, FaceDir::West), 0.84f, tile);
          }
          if (shouldRenderFaceForTile(tile, view.tileAt(lx + 1, y, lz))) {
            emitFace(transparentOut, worldX, y, worldZ, FaceDir::East, textureForFace(tile, FaceDir::East), 0.88f, tile);
          }
        }
      }
    }
  }
};

class OpaqueGreedyMesher {
public:
  struct MaskCell {
    int tile = 0;
    int tex = 0;
  };

  template <typename TextureLookupFn, typename EmitRectFn>
  void build(const ChunkBuildView& view, std::vector<TerrainVertex>& opaqueOut, TextureLookupFn&& textureForFace, EmitRectFn&& emitRect) const {
    std::vector<MaskCell> mask(kChunkArea);

    for (int y = Level::minBuildHeight; y < Level::maxBuildHeight; ++y) {
      for (int lz = 0; lz < kChunkSide; ++lz) {
        for (int lx = 0; lx < kChunkSide; ++lx) {
          const int tile = view.center->getTile(lx, y, lz);
          const int n = view.tileAt(lx, y + 1, lz);
          mask[lx + lz * kChunkSide] =
              (isOpaqueTile(tile) && shouldRenderFaceForTile(tile, n)) ? MaskCell{tile, textureForFace(tile, FaceDir::Top)} : MaskCell{};
        }
      }
      greedyEmit(mask, kChunkSide, kChunkSide, [&](int i, int j, int w, int h, int tile, int tex) {
        emitRect(opaqueOut, FaceDir::Top, tile, tex, 1.0f, view.x0 + i, y, view.z0 + j, w, h);
      });
    }

    for (int y = Level::minBuildHeight; y < Level::maxBuildHeight; ++y) {
      for (int lz = 0; lz < kChunkSide; ++lz) {
        for (int lx = 0; lx < kChunkSide; ++lx) {
          const int tile = view.center->getTile(lx, y, lz);
          const int n = view.tileAt(lx, y - 1, lz);
          mask[lx + lz * kChunkSide] =
              (isOpaqueTile(tile) && !(tile == static_cast<int>(TileId::Bedrock) && y == Level::minBuildHeight) &&
               shouldRenderFaceForTile(tile, n))
                  ? MaskCell{tile, textureForFace(tile, FaceDir::Bottom)}
                  : MaskCell{};
        }
      }
      greedyEmit(mask, kChunkSide, kChunkSide, [&](int i, int j, int w, int h, int tile, int tex) {
        emitRect(opaqueOut, FaceDir::Bottom, tile, tex, 0.55f, view.x0 + i, y, view.z0 + j, w, h);
      });
    }

    const int ySpan = Level::maxBuildHeight - Level::minBuildHeight;
    std::vector<MaskCell> sideMask(static_cast<std::size_t>(16 * ySpan));
    for (int lz = 0; lz < 16; ++lz) {
      for (int y = Level::minBuildHeight; y < Level::maxBuildHeight; ++y) {
        for (int lx = 0; lx < 16; ++lx) {
          const int tile = view.center->getTile(lx, y, lz);
          const int nn = view.tileAt(lx, y, lz - 1);
          sideMask[lx + (y - Level::minBuildHeight) * 16] =
              (isOpaqueTile(tile) && shouldRenderFaceForTile(tile, nn)) ? MaskCell{tile, textureForFace(tile, FaceDir::North)}
                                                                         : MaskCell{};
        }
      }
      greedyEmit(sideMask, 16, ySpan, [&](int i, int j, int w, int h, int tile, int tex) {
        emitRect(opaqueOut, FaceDir::North, tile, tex, 0.78f, view.x0 + i, j + Level::minBuildHeight, view.z0 + lz, w, h);
      });

      for (int y = Level::minBuildHeight; y < Level::maxBuildHeight; ++y) {
        for (int lx = 0; lx < 16; ++lx) {
          const int tile = view.center->getTile(lx, y, lz);
          const int ns = view.tileAt(lx, y, lz + 1);
          sideMask[lx + (y - Level::minBuildHeight) * 16] =
              (isOpaqueTile(tile) && shouldRenderFaceForTile(tile, ns)) ? MaskCell{tile, textureForFace(tile, FaceDir::South)}
                                                                         : MaskCell{};
        }
      }
      greedyEmit(sideMask, 16, ySpan, [&](int i, int j, int w, int h, int tile, int tex) {
        emitRect(opaqueOut, FaceDir::South, tile, tex, 0.72f, view.x0 + i, j + Level::minBuildHeight, view.z0 + lz, w, h);
      });
    }

    for (int lx = 0; lx < 16; ++lx) {
      for (int y = Level::minBuildHeight; y < Level::maxBuildHeight; ++y) {
        for (int lz = 0; lz < 16; ++lz) {
          const int tile = view.center->getTile(lx, y, lz);
          const int nw = view.tileAt(lx - 1, y, lz);
          sideMask[lz + (y - Level::minBuildHeight) * 16] =
              (isOpaqueTile(tile) && shouldRenderFaceForTile(tile, nw)) ? MaskCell{tile, textureForFace(tile, FaceDir::West)}
                                                                         : MaskCell{};
        }
      }
      greedyEmit(sideMask, 16, ySpan, [&](int i, int j, int w, int h, int tile, int tex) {
        emitRect(opaqueOut, FaceDir::West, tile, tex, 0.84f, view.x0 + lx, j + Level::minBuildHeight, view.z0 + i, w, h);
      });

      for (int y = Level::minBuildHeight; y < Level::maxBuildHeight; ++y) {
        for (int lz = 0; lz < 16; ++lz) {
          const int tile = view.center->getTile(lx, y, lz);
          const int ne = view.tileAt(lx + 1, y, lz);
          sideMask[lz + (y - Level::minBuildHeight) * 16] =
              (isOpaqueTile(tile) && shouldRenderFaceForTile(tile, ne)) ? MaskCell{tile, textureForFace(tile, FaceDir::East)}
                                                                         : MaskCell{};
        }
      }
      greedyEmit(sideMask, 16, ySpan, [&](int i, int j, int w, int h, int tile, int tex) {
        emitRect(opaqueOut, FaceDir::East, tile, tex, 0.88f, view.x0 + lx, j + Level::minBuildHeight, view.z0 + i, w, h);
      });
    }
  }

private:
  static bool isOpaqueTile(int tile) {
    return tile > static_cast<int>(TileId::Air) && !isTransparentTile(tile) && !isPlantTile(tile) && !isCactusTile(tile);
  }

  static bool sameCell(const MaskCell& a, const MaskCell& b) {
    return a.tile == b.tile && a.tex == b.tex;
  }

  template <typename EmitFn>
  static void greedyEmit(std::vector<MaskCell>& mask, int wDim, int hDim, EmitFn&& emitter) {
    for (int j = 0; j < hDim; ++j) {
      for (int i = 0; i < wDim;) {
        const MaskCell c = mask[i + j * wDim];
        if (c.tile <= 0) {
          ++i;
          continue;
        }
        int w = 1;
        while (i + w < wDim && sameCell(mask[i + w + j * wDim], c)) {
          ++w;
        }
        int h = 1;
        bool done = false;
        while (j + h < hDim && !done) {
          for (int k = 0; k < w; ++k) {
            if (!sameCell(mask[i + k + (j + h) * wDim], c)) {
              done = true;
              break;
            }
          }
          if (!done) {
            ++h;
          }
        }

        emitter(i, j, w, h, c.tile, c.tex);

        for (int yy = 0; yy < h; ++yy) {
          for (int xx = 0; xx < w; ++xx) {
            mask[i + xx + (j + yy) * wDim] = {};
          }
        }
        i += w;
      }
    }
  }
};

}  // namespace mc::detail
