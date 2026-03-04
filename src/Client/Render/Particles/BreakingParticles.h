#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <simd/simd.h>
#include <vector>

#include "Client/Render/Metal/MetalRenderer.h"

namespace mc {

class BreakingParticles {
public:
  enum class Face : int {
    Top = 0,
    Bottom = 1,
    North = 2,
    South = 3,
    West = 4,
    East = 5,
  };

  struct SpawnContext {
    int x = 0;
    int y = 0;
    int z = 0;
    int tile = 0;
    std::array<int, 6> faceTextures{};
    std::array<simd_float3, 6> faceTints{};
  };

  void spawnBreakBurst(const SpawnContext& ctx);
  void spawnMiningChip(const SpawnContext& ctx, int prevX, int prevY, int prevZ);
  bool tick(float dtSeconds);
  void appendVertices(std::vector<TerrainVertex>& out, const simd_float3& cameraPos) const;
  void clear();

private:
  struct Particle {
    simd_float3 position{};
    simd_float3 velocity{};
    simd_float3 color{};
    int textureIndex = 0;
    simd_float2 uvMin{0.0f, 0.0f};
    simd_float2 uvMax{1.0f, 1.0f};
    float ageSeconds = 0.0f;
    float lifeSeconds = 0.4f;
    float size = 0.03f;
  };

  static void appendParticleQuad(std::vector<TerrainVertex>& out, const Particle& p, const simd_float3& right,
                                 const simd_float3& up);
  static std::uint32_t hashU32(std::uint32_t x);
  static float rand01(std::uint32_t* state);
  void trimOverflow();

  std::vector<Particle> particles_;
  std::uint32_t emissionSerial_ = 1u;

  static constexpr std::size_t kMaxParticles = 2048;
};

}  // namespace mc
