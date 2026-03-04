#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <random>
#include <vector>

namespace mc::gen {

inline double noiseLerp(double a, double b, double t) {
  return a + (b - a) * t;
}

class ImprovedNoise {
public:
  explicit ImprovedNoise(std::mt19937_64& rng) {
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    xo_ = unit(rng) * 256.0;
    yo_ = unit(rng) * 256.0;
    zo_ = unit(rng) * 256.0;

    std::array<int, 256> p{};
    for (int i = 0; i < 256; ++i) {
      p[i] = i;
    }
    for (int i = 255; i >= 0; --i) {
      std::uniform_int_distribution<int> pick(0, i);
      const int j = pick(rng);
      std::swap(p[i], p[j]);
    }
    for (int i = 0; i < 256; ++i) {
      perm_[i] = p[i];
      perm_[i + 256] = p[i];
    }
  }

  double sample(double x, double y, double z) const {
    x += xo_;
    y += yo_;
    z += zo_;

    const int xi = static_cast<int>(std::floor(x)) & 255;
    const int yi = static_cast<int>(std::floor(y)) & 255;
    const int zi = static_cast<int>(std::floor(z)) & 255;

    x -= std::floor(x);
    y -= std::floor(y);
    z -= std::floor(z);

    const double u = fade(x);
    const double v = fade(y);
    const double w = fade(z);

    const int a = perm_[xi] + yi;
    const int aa = perm_[a] + zi;
    const int ab = perm_[a + 1] + zi;
    const int b = perm_[xi + 1] + yi;
    const int ba = perm_[b] + zi;
    const int bb = perm_[b + 1] + zi;

    return noiseLerp(
        noiseLerp(noiseLerp(grad(perm_[aa], x, y, z), grad(perm_[ba], x - 1.0, y, z), u),
                  noiseLerp(grad(perm_[ab], x, y - 1.0, z), grad(perm_[bb], x - 1.0, y - 1.0, z), u), v),
        noiseLerp(noiseLerp(grad(perm_[aa + 1], x, y, z - 1.0), grad(perm_[ba + 1], x - 1.0, y, z - 1.0), u),
                  noiseLerp(grad(perm_[ab + 1], x, y - 1.0, z - 1.0),
                            grad(perm_[bb + 1], x - 1.0, y - 1.0, z - 1.0), u),
                  v),
        w);
  }

private:
  static double fade(double t) {
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
  }

  static double grad(int hash, double x, double y, double z) {
    const int h = hash & 15;
    const double u = h < 8 ? x : y;
    const double v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
  }

  std::array<int, 512> perm_{};
  double xo_ = 0.0;
  double yo_ = 0.0;
  double zo_ = 0.0;
};

class PerlinNoise {
public:
  PerlinNoise(std::mt19937_64& rng, int levels) {
    levels_.reserve(levels);
    for (int i = 0; i < levels; ++i) {
      levels_.emplace_back(rng);
    }
  }

  void getRegion3D(std::vector<double>& out, int x, int y, int z, int xSize, int ySize, int zSize, double xScale,
                   double yScale, double zScale) const {
    const std::size_t size =
        static_cast<std::size_t>(xSize) * static_cast<std::size_t>(ySize) * static_cast<std::size_t>(zSize);
    out.assign(size, 0.0);

    double amplitude = 1.0;
    double frequency = 1.0;
    for (const ImprovedNoise& level : levels_) {
      std::size_t p = 0;
      for (int xx = 0; xx < xSize; ++xx) {
        const double nx = (static_cast<double>(x + xx) * xScale) * frequency;
        for (int zz = 0; zz < zSize; ++zz) {
          const double nz = (static_cast<double>(z + zz) * zScale) * frequency;
          for (int yy = 0; yy < ySize; ++yy) {
            const double ny = (static_cast<double>(y + yy) * yScale) * frequency;
            out[p++] += level.sample(nx, ny, nz) * amplitude;
          }
        }
      }
      frequency *= 2.0;
      amplitude *= 0.5;
    }
  }

  void getRegion2D(std::vector<double>& out, int x, int z, int xSize, int zSize, double xScale, double zScale) const {
    const std::size_t size = static_cast<std::size_t>(xSize) * static_cast<std::size_t>(zSize);
    out.assign(size, 0.0);

    double amplitude = 1.0;
    double frequency = 1.0;
    for (const ImprovedNoise& level : levels_) {
      std::size_t p = 0;
      for (int xx = 0; xx < xSize; ++xx) {
        const double nx = (static_cast<double>(x + xx) * xScale) * frequency;
        for (int zz = 0; zz < zSize; ++zz) {
          const double nz = (static_cast<double>(z + zz) * zScale) * frequency;
          out[p++] += level.sample(nx, 10.0 * frequency, nz) * amplitude;
        }
      }
      frequency *= 2.0;
      amplitude *= 0.5;
    }
  }

  double sample2D(double x, double z) const {
    double value = 0.0;
    double amplitude = 1.0;
    double frequency = 1.0;
    for (const ImprovedNoise& level : levels_) {
      value += level.sample(x * frequency, 10.0 * frequency, z * frequency) * amplitude;
      frequency *= 2.0;
      amplitude *= 0.5;
    }
    return value;
  }

private:
  std::vector<ImprovedNoise> levels_;
};

}  // namespace mc::gen
