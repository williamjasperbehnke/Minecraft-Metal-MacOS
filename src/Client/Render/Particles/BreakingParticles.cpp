#include "Client/Render/Particles/BreakingParticles.h"

#include <algorithm>
#include <cmath>

#include "World/Tile/Tile.h"

namespace mc {

namespace {

constexpr float kParticleGravity = 3.8f;
constexpr float kAtlasCols = 16.0f;
constexpr float kAtlasRows = 16.0f;

simd_float2 atlasTileOrigin(int textureIndex) {
  const int tx = textureIndex % 16;
  const int ty = textureIndex / 16;
  return {static_cast<float>(tx) / kAtlasCols, static_cast<float>(ty) / kAtlasRows};
}

simd_float3 normalize3(const simd_float3& v) {
  const float len2 = v.x * v.x + v.y * v.y + v.z * v.z;
  if (len2 <= 1e-8f) {
    return {1.0f, 0.0f, 0.0f};
  }
  const float invLen = 1.0f / std::sqrt(len2);
  return {v.x * invLen, v.y * invLen, v.z * invLen};
}

float particleDensityScaleForTile(int tile) {
  switch (static_cast<TileId>(tile)) {
    case TileId::Leaves:
    case TileId::SpruceLeaves:
    case TileId::BirchLeaves:
    case TileId::TallGrass:
    case TileId::Fern:
    case TileId::DeadBush:
    case TileId::FlowerYellow:
    case TileId::FlowerRed:
    case TileId::MushroomBrown:
    case TileId::MushroomRed:
    case TileId::SugarCane:
      return 1.45f;
    case TileId::Glass:
    case TileId::Ice:
      return 1.30f;
    case TileId::Sand:
    case TileId::Gravel:
    case TileId::Cactus:
      return 1.20f;
    default:
      return 1.0f;
  }
}

}  // namespace

void BreakingParticles::spawnBreakBurst(const SpawnContext& ctx) {
  std::uint32_t seed =
      static_cast<std::uint32_t>(ctx.x * 73856093 ^ ctx.y * 19349663 ^ ctx.z * 83492791 ^ ctx.tile * 2654435761u);
  const float densityScale = particleDensityScaleForTile(ctx.tile);
  const int spawnCount = std::max(6, static_cast<int>(std::round(12.0f * densityScale)));
  for (int i = 0; i < spawnCount; ++i) {
    const float rx = rand01(&seed);
    const float ry = rand01(&seed);
    const float rz = rand01(&seed);
    const float vx = (rand01(&seed) - 0.5f) * 1.4f;
    const float vy = 0.45f + rand01(&seed) * 0.95f;
    const float vz = (rand01(&seed) - 0.5f) * 1.4f;
    const float life = 0.24f + rand01(&seed) * 0.28f;
    const float size = 0.018f + rand01(&seed) * 0.024f;
    const float shade = 0.82f + rand01(&seed) * 0.18f;
    const int facePick = static_cast<int>(rand01(&seed) * 6.0f) % 6;
    const int subX = static_cast<int>(rand01(&seed) * 4.0f) & 3;
    const int subY = static_cast<int>(rand01(&seed) * 4.0f) & 3;

    Particle p;
    p.position = {static_cast<float>(ctx.x) + 0.1f + rx * 0.8f, static_cast<float>(ctx.y) + 0.1f + ry * 0.8f,
                  static_cast<float>(ctx.z) + 0.1f + rz * 0.8f};
    p.velocity = {vx, vy, vz};
    const simd_float3 faceTint = ctx.faceTints[static_cast<std::size_t>(facePick)];
    p.color = {faceTint.x * shade, faceTint.y * shade, faceTint.z * shade};
    p.textureIndex = ctx.faceTextures[static_cast<std::size_t>(facePick)];
    p.uvMin = {subX * 0.25f, subY * 0.25f};
    p.uvMax = {p.uvMin.x + 0.25f, p.uvMin.y + 0.25f};
    p.ageSeconds = 0.0f;
    p.lifeSeconds = life;
    p.size = size;
    particles_.push_back(p);
  }
  trimOverflow();
}

void BreakingParticles::spawnMiningChip(const SpawnContext& ctx, int prevX, int prevY, int prevZ) {
  std::uint32_t seed = static_cast<std::uint32_t>(ctx.x * 73856093 ^ ctx.y * 19349663 ^ ctx.z * 83492791 ^
                                                  prevX * 2654435761u ^ prevZ * 2246822519u ^ ctx.tile ^
                                                  (emissionSerial_++ * 1597334677u));
  const int dx = prevX - ctx.x;
  const int dy = prevY - ctx.y;
  const int dz = prevZ - ctx.z;
  const float densityScale = particleDensityScaleForTile(ctx.tile);
  int hitFace = static_cast<int>(Face::Top);
  if (dx > 0) {
    hitFace = static_cast<int>(Face::East);
  } else if (dx < 0) {
    hitFace = static_cast<int>(Face::West);
  } else if (dy > 0) {
    hitFace = static_cast<int>(Face::Top);
  } else if (dy < 0) {
    hitFace = static_cast<int>(Face::Bottom);
  } else if (dz > 0) {
    hitFace = static_cast<int>(Face::South);
  } else if (dz < 0) {
    hitFace = static_cast<int>(Face::North);
  }

  const int baseCount = 1 + (static_cast<int>(rand01(&seed) * 3.0f) % 3);  // 1..3
  const int spawnCount = std::max(1, static_cast<int>(std::round(static_cast<float>(baseCount) * densityScale)));
  for (int i = 0; i < spawnCount; ++i) {
    const float u = rand01(&seed);
    const float v = rand01(&seed);
    const float jitterX = (rand01(&seed) - 0.5f) * 0.14f;
    const float jitterY = (rand01(&seed) - 0.5f) * 0.10f;
    const float jitterZ = (rand01(&seed) - 0.5f) * 0.14f;
    simd_float3 pos{static_cast<float>(ctx.x) + 0.5f, static_cast<float>(ctx.y) + 0.5f, static_cast<float>(ctx.z) + 0.5f};
    simd_float3 outward{0.0f, 0.0f, 0.0f};
    if (std::abs(dx) + std::abs(dy) + std::abs(dz) == 1) {
      // `prev` is the cell outside the hit face. Emit from that face and push debris outward.
      outward = {static_cast<float>(dx), static_cast<float>(dy), static_cast<float>(dz)};
      if (dx != 0) {
        pos.x = static_cast<float>(ctx.x) + (dx > 0 ? 1.02f : -0.02f);
        pos.y = static_cast<float>(ctx.y) + 0.18f + u * 0.64f;
        pos.z = static_cast<float>(ctx.z) + 0.18f + v * 0.64f;
      } else if (dy != 0) {
        pos.y = static_cast<float>(ctx.y) + (dy > 0 ? 1.02f : -0.02f);
        pos.x = static_cast<float>(ctx.x) + 0.18f + u * 0.64f;
        pos.z = static_cast<float>(ctx.z) + 0.18f + v * 0.64f;
      } else {
        pos.z = static_cast<float>(ctx.z) + (dz > 0 ? 1.02f : -0.02f);
        pos.x = static_cast<float>(ctx.x) + 0.18f + u * 0.64f;
        pos.y = static_cast<float>(ctx.y) + 0.18f + v * 0.64f;
      }
    } else {
      pos.x = static_cast<float>(ctx.x) + 0.2f + u * 0.6f;
      pos.y = static_cast<float>(ctx.y) + 0.2f + v * 0.6f;
      pos.z = static_cast<float>(ctx.z) + 0.2f + rand01(&seed) * 0.6f;
      outward = {0.0f, 0.6f, 0.0f};
    }

    const int subX = static_cast<int>(rand01(&seed) * 4.0f) & 3;
    const int subY = static_cast<int>(rand01(&seed) * 4.0f) & 3;
    const float shade = 0.88f + rand01(&seed) * 0.12f;
    Particle p;
    p.position = pos;
    p.velocity = outward * (0.28f + rand01(&seed) * 0.22f) +
                 simd_float3{jitterX, 0.05f + rand01(&seed) * 0.25f + jitterY, jitterZ};
    const simd_float3 faceTint = ctx.faceTints[static_cast<std::size_t>(hitFace)];
    p.color = {faceTint.x * shade, faceTint.y * shade, faceTint.z * shade};
    p.textureIndex = ctx.faceTextures[static_cast<std::size_t>(hitFace)];
    p.uvMin = {subX * 0.25f, subY * 0.25f};
    p.uvMax = {p.uvMin.x + 0.25f, p.uvMin.y + 0.25f};
    p.ageSeconds = 0.0f;
    p.lifeSeconds = 0.14f + rand01(&seed) * 0.26f;
    p.size = 0.016f + rand01(&seed) * 0.026f;
    particles_.push_back(p);
  }
  trimOverflow();
}

bool BreakingParticles::tick(float dtSeconds) {
  if (particles_.empty()) {
    return false;
  }
  bool changed = false;
  for (Particle& p : particles_) {
    p.ageSeconds += dtSeconds;
    if (p.ageSeconds >= p.lifeSeconds) {
      continue;
    }
    p.velocity.y -= kParticleGravity * dtSeconds;
    p.velocity.x *= 0.97f;
    p.velocity.z *= 0.97f;
    p.position += p.velocity * dtSeconds;
    changed = true;
  }
  const auto oldSize = particles_.size();
  particles_.erase(std::remove_if(particles_.begin(), particles_.end(), [](const Particle& p) { return p.ageSeconds >= p.lifeSeconds; }),
                   particles_.end());
  return changed || (particles_.size() != oldSize);
}

void BreakingParticles::appendVertices(std::vector<TerrainVertex>& out, const simd_float3& cameraPos) const {
  constexpr simd_float3 kWorldUp{0.0f, 1.0f, 0.0f};
  for (const Particle& p : particles_) {
    const simd_float3 toCam = normalize3(cameraPos - p.position);
    simd_float3 right = normalize3(simd_float3{kWorldUp.y * toCam.z - kWorldUp.z * toCam.y,
                                               kWorldUp.z * toCam.x - kWorldUp.x * toCam.z,
                                               kWorldUp.x * toCam.y - kWorldUp.y * toCam.x});
    if (std::abs(right.x) + std::abs(right.y) + std::abs(right.z) < 1e-4f) {
      right = {1.0f, 0.0f, 0.0f};
    }
    simd_float3 up = normalize3(simd_float3{toCam.y * right.z - toCam.z * right.y, toCam.z * right.x - toCam.x * right.z,
                                            toCam.x * right.y - toCam.y * right.x});
    Particle shaded = p;
    // Negative red channel marks cutout in terrain fragment shader.
    shaded.color.x = -std::abs(shaded.color.x);
    appendParticleQuad(out, shaded, right, up);
  }
}

void BreakingParticles::clear() {
  particles_.clear();
}

void BreakingParticles::appendParticleQuad(std::vector<TerrainVertex>& out, const Particle& p, const simd_float3& right,
                                           const simd_float3& up) {
  const float x = p.position.x;
  const float y = p.position.y;
  const float z = p.position.z;
  const float s = p.size;
  const simd_float2 tileOrigin = atlasTileOrigin(p.textureIndex);
  const simd_float3 r = right * s;
  const simd_float3 u = up * s;
  const simd_float3 c{x, y, z};

  TerrainVertex v0{};
  TerrainVertex v1{};
  TerrainVertex v2{};
  TerrainVertex v3{};
  v0.position = c - r - u;
  v1.position = c + r - u;
  v2.position = c + r + u;
  v3.position = c - r + u;
  v0.color = p.color;
  v1.color = p.color;
  v2.color = p.color;
  v3.color = p.color;
  v0.uv = {p.uvMin.x, p.uvMax.y};
  v1.uv = {p.uvMax.x, p.uvMax.y};
  v2.uv = {p.uvMax.x, p.uvMin.y};
  v3.uv = {p.uvMin.x, p.uvMin.y};
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
  // Keep billboards visible with back-face culling enabled.
  out.push_back(v0);
  out.push_back(v2);
  out.push_back(v1);
  out.push_back(v0);
  out.push_back(v3);
  out.push_back(v2);
}

std::uint32_t BreakingParticles::hashU32(std::uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352dU;
  x ^= x >> 15;
  x *= 0x846ca68bU;
  x ^= x >> 16;
  return x;
}

float BreakingParticles::rand01(std::uint32_t* state) {
  *state = hashU32(*state + 0x9e3779b9U);
  return static_cast<float>(*state & 0x00ffffffU) / static_cast<float>(0x01000000U);
}

void BreakingParticles::trimOverflow() {
  if (particles_.size() <= kMaxParticles) {
    return;
  }
  const std::size_t overflow = particles_.size() - kMaxParticles;
  particles_.erase(particles_.begin(), particles_.begin() + static_cast<long>(overflow));
}

}  // namespace mc
