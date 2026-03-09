#include "Client/Debug/ChunkBorderOverlay.h"

namespace mc {

namespace {

inline void pushLine(std::vector<TerrainVertex>& out, float x0, float y0, float z0, float x1, float y1, float z1,
                     float r, float g, float b) {
  TerrainVertex a{};
  a.position = {x0, y0, z0};
  a.color = {r, g, b};
  a.uv = {0.0f, 0.0f};

  TerrainVertex c{};
  c.position = {x1, y1, z1};
  c.color = {r, g, b};
  c.uv = {0.0f, 0.0f};

  out.push_back(a);
  out.push_back(c);
}

inline void appendCurrentChunkBorderGrid(std::vector<TerrainVertex>& out, int chunkX, int chunkZ) {
  constexpr float y = 0.10f;
  constexpr float markDepth = 1.0f;
  constexpr float borderR = 1.0f;
  constexpr float borderG = 0.95f;
  constexpr float borderB = 0.20f;
  constexpr float markR = 0.95f;
  constexpr float markG = 0.85f;
  constexpr float markB = 0.15f;

  const float xMin = static_cast<float>(chunkX * 16);
  const float xMax = xMin + 16.0f;
  const float zMin = static_cast<float>(chunkZ * 16);
  const float zMax = zMin + 16.0f;

  // Bright perimeter for the active chunk.
  pushLine(out, xMin, y, zMin, xMax, y, zMin, borderR, borderG, borderB);
  pushLine(out, xMax, y, zMin, xMax, y, zMax, borderR, borderG, borderB);
  pushLine(out, xMax, y, zMax, xMin, y, zMax, borderR, borderG, borderB);
  pushLine(out, xMin, y, zMax, xMin, y, zMin, borderR, borderG, borderB);

  // One-block tick grid around the border so chunk edges are easy to read.
  for (int i = 0; i <= 16; ++i) {
    const float fi = static_cast<float>(i);
    pushLine(out, xMin + fi, y, zMin, xMin + fi, y, zMin + markDepth, markR, markG, markB);
    pushLine(out, xMin + fi, y, zMax, xMin + fi, y, zMax - markDepth, markR, markG, markB);
    pushLine(out, xMin, y, zMin + fi, xMin + markDepth, y, zMin + fi, markR, markG, markB);
    pushLine(out, xMax, y, zMin + fi, xMax - markDepth, y, zMin + fi, markR, markG, markB);
  }
}

inline void appendCurrentChunkBlockGrid(std::vector<TerrainVertex>& out, int chunkX, int chunkZ) {
  constexpr float y = 0.11f;
  constexpr float gridR = 1.0f;
  constexpr float gridG = 0.92f;
  constexpr float gridB = 0.18f;
  constexpr float yMin = 0.02f;
  constexpr float yMax = 128.0f;

  const float xMin = static_cast<float>(chunkX * 16);
  const float xMax = xMin + 16.0f;
  const float zMin = static_cast<float>(chunkZ * 16);
  const float zMax = zMin + 16.0f;

  // 2-block grid in the active chunk.
  for (int i = 0; i <= 16; i += 2) {
    const float fi = static_cast<float>(i);
    pushLine(out, xMin + fi, y, zMin, xMin + fi, y, zMax, gridR, gridG, gridB);
    pushLine(out, xMin, y, zMin + fi, xMax, y, zMin + fi, gridR, gridG, gridB);

    // Vertical border lines at each block interval.
    pushLine(out, xMin + fi, yMin, zMin, xMin + fi, yMax, zMin, gridR, gridG, gridB);
    pushLine(out, xMin + fi, yMin, zMax, xMin + fi, yMax, zMax, gridR, gridG, gridB);
    pushLine(out, xMin, yMin, zMin + fi, xMin, yMax, zMin + fi, gridR, gridG, gridB);
    pushLine(out, xMax, yMin, zMin + fi, xMax, yMax, zMin + fi, gridR, gridG, gridB);
  }

  // Horizontal border lines every 2 blocks in height.
  for (int yi = 0; yi <= 128; yi += 2) {
    const float fy = static_cast<float>(yi) + 0.02f;
    pushLine(out, xMin, fy, zMin, xMax, fy, zMin, gridR, gridG, gridB);
    pushLine(out, xMin, fy, zMax, xMax, fy, zMax, gridR, gridG, gridB);
    pushLine(out, xMin, fy, zMin, xMin, fy, zMax, gridR, gridG, gridB);
    pushLine(out, xMax, fy, zMin, xMax, fy, zMax, gridR, gridG, gridB);
  }
}

}  // namespace

void ChunkBorderOverlay::build(int centerChunkX, int centerChunkZ, int radiusChunks, std::vector<TerrainVertex>& out) const {
  out.clear();
  constexpr float y0 = 0.02f;
  constexpr float y1 = 128.0f;
  constexpr float colorR = 1.0f;
  constexpr float colorG = 0.25f;
  constexpr float colorB = 0.25f;

  // Keep the red chunk cage nearby only so the debug overlay stays readable.
  constexpr int kNearbyBorderRadius = 4;
  const int borderRadius = std::min(radiusChunks, kNearbyBorderRadius);
  const int side = borderRadius * 2 + 1;
  out.reserve(static_cast<std::size_t>(side * side * 24 + 200));

  for (int cx = centerChunkX - borderRadius; cx <= centerChunkX + borderRadius; ++cx) {
    for (int cz = centerChunkZ - borderRadius; cz <= centerChunkZ + borderRadius; ++cz) {
      const float xMin = static_cast<float>(cx * 16);
      const float xMax = xMin + 16.0f;
      const float zMin = static_cast<float>(cz * 16);
      const float zMax = zMin + 16.0f;

      // Verticals.
      pushLine(out, xMin, y0, zMin, xMin, y1, zMin, colorR, colorG, colorB);
      pushLine(out, xMax, y0, zMin, xMax, y1, zMin, colorR, colorG, colorB);
      pushLine(out, xMax, y0, zMax, xMax, y1, zMax, colorR, colorG, colorB);
      pushLine(out, xMin, y0, zMax, xMin, y1, zMax, colorR, colorG, colorB);
    }
  }

  appendCurrentChunkBorderGrid(out, centerChunkX, centerChunkZ);
  appendCurrentChunkBlockGrid(out, centerChunkX, centerChunkZ);
}

}  // namespace mc
