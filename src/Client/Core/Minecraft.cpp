#include "Client/Core/Minecraft.h"

#include <algorithm>
#include <cmath>

#include "Client/GameMode/CreativeGameMode.h"
#include "Client/GameMode/SpectatorGameMode.h"
#include "Client/GameMode/SurvivalGameMode.h"
#include "Client/Render/Metal/LevelRenderer.h"
#include "Client/Render/Metal/MetalRenderer.h"
#include "World/Entity/Player.h"
#include "World/Level/Level.h"
#include "World/Tile/Tile.h"

namespace mc {

namespace {

int floorDiv16(int v) {
  return v >= 0 ? v / 16 : (v - 15) / 16;
}

bool findSafeSpawn(Level* level, int* outX, int* outY, int* outZ) {
  if (!level || !outX || !outY || !outZ) {
    return false;
  }

  constexpr int kRadius = 24;
  int bestY = -1;
  int bestX = 0;
  int bestZ = 0;

  for (int r = 0; r <= kRadius; ++r) {
    for (int dz = -r; dz <= r; ++dz) {
      for (int dx = -r; dx <= r; ++dx) {
        if (std::abs(dx) != r && std::abs(dz) != r) {
          continue;
        }
        const int x = dx;
        const int z = dz;
        const int top = level->getTopSolidBlockY(x, z);
        if (top < Level::minBuildHeight) {
          continue;
        }
        const int feetY = top + 1;
        const int headY = feetY + 1;
        if (headY >= Level::maxBuildHeight) {
          continue;
        }
        if (!level->isEmptyTile(x, feetY, z) || !level->isEmptyTile(x, headY, z)) {
          continue;
        }
        if (top > bestY) {
          bestY = top;
          bestX = x;
          bestZ = z;
        }
      }
    }
    if (bestY >= 0) {
      *outX = bestX;
      *outY = bestY + 1;
      *outZ = bestZ;
      return true;
    }
  }

  return false;
}

simd_float3 add(const simd_float3& a, const simd_float3& b) {
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}

simd_float3 sub(const simd_float3& a, const simd_float3& b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}

simd_float3 mul(const simd_float3& v, float s) {
  return {v.x * s, v.y * s, v.z * s};
}

float dot(const simd_float3& a, const simd_float3& b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

simd_float3 cross(const simd_float3& a, const simd_float3& b) {
  return {
      a.y * b.z - a.z * b.y,
      a.z * b.x - a.x * b.z,
      a.x * b.y - a.y * b.x,
  };
}

simd_float3 normalize(const simd_float3& v) {
  const float len = std::sqrt(dot(v, v));
  if (len < 1e-6f) {
    return {0.0f, 0.0f, 0.0f};
  }
  return {v.x / len, v.y / len, v.z / len};
}

simd_float4x4 perspective(float fovy, float aspect, float nearZ, float farZ) {
  const float yScale = 1.0f / std::tan(fovy * 0.5f);
  const float xScale = yScale / aspect;
  const float zRange = farZ - nearZ;

  simd_float4x4 m{};
  m.columns[0] = {xScale, 0.0f, 0.0f, 0.0f};
  m.columns[1] = {0.0f, yScale, 0.0f, 0.0f};
  m.columns[2] = {0.0f, 0.0f, -(farZ + nearZ) / zRange, -1.0f};
  m.columns[3] = {0.0f, 0.0f, -(2.0f * farZ * nearZ) / zRange, 0.0f};
  return m;
}

simd_float4x4 lookAt(const simd_float3& eye, const simd_float3& target, const simd_float3& up) {
  const simd_float3 f = normalize(sub(target, eye));
  const simd_float3 s = normalize(cross(f, up));
  const simd_float3 u = cross(s, f);

  simd_float4x4 m{};
  m.columns[0] = {s.x, u.x, -f.x, 0.0f};
  m.columns[1] = {s.y, u.y, -f.y, 0.0f};
  m.columns[2] = {s.z, u.z, -f.z, 0.0f};
  m.columns[3] = {-dot(s, eye), -dot(u, eye), dot(f, eye), 1.0f};
  return m;
}

simd_float4x4 mulMat(const simd_float4x4& a, const simd_float4x4& b) {
  simd_float4x4 out{};
  for (int c = 0; c < 4; ++c) {
    for (int r = 0; r < 4; ++r) {
      out.columns[c][r] =
          a.columns[0][r] * b.columns[c][0] +
          a.columns[1][r] * b.columns[c][1] +
          a.columns[2][r] * b.columns[c][2] +
          a.columns[3][r] * b.columns[c][3];
    }
  }
  return out;
}

}  // namespace

Minecraft::Minecraft() = default;
Minecraft::~Minecraft() = default;

void Minecraft::init(MetalRenderer* renderer) {
  renderer_ = renderer;

  level_ = std::make_unique<Level>();
  levelRenderer_ = std::make_unique<LevelRenderer>(renderer_);
  levelRenderer_->setLevel(level_.get());

  level_->ensureChunksInRange(0, 0, 2);

  localPlayer_ = std::make_unique<LocalPlayer>();
  int spawnX = 0;
  int spawnY = 8;
  int spawnZ = 0;
  if (findSafeSpawn(level_.get(), &spawnX, &spawnY, &spawnZ)) {
    localPlayer_->setPosition(static_cast<double>(spawnX) + 0.5, static_cast<double>(spawnY),
                              static_cast<double>(spawnZ) + 0.5);
  } else {
    localPlayer_->setPosition(0.5, 8.0, 0.5);
  }

  rebuildGameMode(GameModeType::Survival);

  const int playerChunkX = floorDiv16(static_cast<int>(localPlayer_->x()));
  const int playerChunkZ = floorDiv16(static_cast<int>(localPlayer_->z()));
  loadedChunkX_ = playerChunkX;
  loadedChunkZ_ = playerChunkZ;
  chunksPrimed_ = true;

  levelRenderer_->setRenderCenter(static_cast<int>(localPlayer_->x()), static_cast<int>(localPlayer_->z()));
  levelRenderer_->setRenderDistanceChunks(kRenderDistanceChunks);
  {
    const simd_float3 eye = cameraPosition();
    levelRenderer_->setCameraPosition(eye.x, eye.y, eye.z);
  }
  levelRenderer_->setViewProj(viewParams(viewAspect_).viewProj);
  levelRenderer_->tick();
  renderer_->setViewParams(viewParams(16.0f / 9.0f));
}

void Minecraft::tick(double dtSeconds) {
  if (!level_ || !localPlayer_) {
    return;
  }

  const GameModeType mode = currentGameModeType();
  playerController_.applyToPlayer(*localPlayer_, mode);
  const float targetFov = playerController_.targetFovRadians(mode);
  const float blend = std::min(1.0f, static_cast<float>(dtSeconds * 10.0));
  currentFovRadians_ += (targetFov - currentFovRadians_) * blend;

  localPlayer_->tick(level_.get(), dtSeconds);
  updateWorldStreaming(dtSeconds);
  const std::optional<BlockInteractionController::Hit> hit = raycastCrosshairHit();
  blockInteractionController_.updateLookTarget(hit);
  blockInteractionController_.tickBreaking(
      dtSeconds, mode, hit,
      [this](int x, int y, int z) { return level_->getTile(x, y, z); },
      [this](int x, int y, int z) { return destroyBlockAt(x, y, z); },
      [this]() { return raycastCrosshairHit(); }, levelRenderer_.get());
  updateRendererState();
}

void Minecraft::updateWorldStreaming(double dtSeconds) {
  if (!level_ || !localPlayer_) {
    return;
  }

  const int playerChunkX = floorDiv16(static_cast<int>(localPlayer_->x()));
  const int playerChunkZ = floorDiv16(static_cast<int>(localPlayer_->z()));
  if (!chunksPrimed_ || playerChunkX != loadedChunkX_ || playerChunkZ != loadedChunkZ_) {
    loadedChunkX_ = playerChunkX;
    loadedChunkZ_ = playerChunkZ;
    chunksPrimed_ = true;
  }

  int streamBudget = 16;
  if (dtSeconds > 0.030) {
    streamBudget = 5;
  } else if (dtSeconds > 0.022) {
    streamBudget = 8;
  } else if (dtSeconds > 0.016) {
    streamBudget = 12;
  }

  level_->ensureChunksInRangeBudget(loadedChunkX_, loadedChunkZ_, kRenderDistanceChunks, streamBudget);
}

void Minecraft::updateRendererState() {
  if (!localPlayer_ || !levelRenderer_) {
    return;
  }

  levelRenderer_->setRenderCenter(static_cast<int>(localPlayer_->x()), static_cast<int>(localPlayer_->z()));
  const simd_float3 eye = cameraPosition();
  levelRenderer_->setCameraPosition(eye.x, eye.y, eye.z);
  levelRenderer_->setViewProj(viewParams(viewAspect_).viewProj);
  levelRenderer_->tick();
}

void Minecraft::setInputState(const InputState& input) {
  playerController_.setInputState(input);
}

void Minecraft::setCreativeMode(bool enabled) {
  if (enabled == isCreativeMode() && !isSpectatorMode()) {
    return;
  }
  rebuildGameMode(enabled ? GameModeType::Creative : GameModeType::Survival);
  if (!localPlayer_) {
    return;
  }
  localPlayer_->setNoClip(false);
  localPlayer_->setFlying(enabled);
  localPlayer_->setFlyVerticalInput(0.0f);
  localPlayer_->setCrouching(false);
}

void Minecraft::toggleCreativeMode() {
  setCreativeMode(!isCreativeMode());
}

bool Minecraft::isCreativeMode() const {
  return gameMode_ && gameMode_->isCreative();
}

void Minecraft::setSpectatorMode(bool enabled) {
  if (enabled == isSpectatorMode()) {
    return;
  }
  rebuildGameMode(enabled ? GameModeType::Spectator : GameModeType::Survival);
  if (!localPlayer_) {
    return;
  }
  localPlayer_->setNoClip(enabled);
  localPlayer_->setFlying(enabled);
  localPlayer_->setFlyVerticalInput(0.0f);
  localPlayer_->setCrouching(false);
}

void Minecraft::toggleSpectatorMode() {
  setSpectatorMode(!isSpectatorMode());
}

bool Minecraft::isSpectatorMode() const {
  return gameMode_ && gameMode_->isSpectator();
}

void Minecraft::addLookInput(float deltaX, float deltaY) {
  playerController_.addLookInput(deltaX, deltaY);
}

void Minecraft::setBreakHeld(bool held) {
  blockInteractionController_.setBreakHeld(held, levelRenderer_.get());
}

void Minecraft::setViewAspect(float aspect) {
  if (aspect > 0.001f) {
    viewAspect_ = aspect;
  }
}

void Minecraft::toggleInventory() {
  if (!localPlayer_) {
    return;
  }
  localPlayer_->inventory().toggleOpen();
}

void Minecraft::setInventoryOpen(bool open) {
  if (!localPlayer_) {
    return;
  }
  localPlayer_->inventory().setOpen(open);
}

bool Minecraft::isInventoryOpen() const {
  return localPlayer_ ? localPlayer_->inventory().isOpen() : false;
}

void Minecraft::selectHotbarSlot(int slotIndex) {
  if (!localPlayer_) {
    return;
  }
  localPlayer_->inventory().selectHotbarIndex(slotIndex);
}

int Minecraft::selectedHotbarSlot() const {
  return localPlayer_ ? localPlayer_->inventory().selectedHotbarIndex() : 0;
}

int Minecraft::selectedPlaceTile() const {
  return localPlayer_ ? localPlayer_->inventory().selectedTile() : static_cast<int>(TileId::Grass);
}

const Inventory& Minecraft::inventory() const {
  static const Inventory kFallbackInventory;
  return localPlayer_ ? localPlayer_->inventory() : kFallbackInventory;
}

bool Minecraft::destroyBlockAt(int x, int y, int z) {
  if (!gameMode_ || !level_) {
    return false;
  }
  if (level_->getTile(x, y, z) == static_cast<int>(TileId::Air)) {
    return false;
  }
  return gameMode_->destroyBlockAt(x, y, z);
}

bool Minecraft::placeBlockAt(int x, int y, int z) {
  if (!gameMode_) {
    return false;
  }
  return gameMode_->placeBlockAt(x, y, z);
}

simd_float3 Minecraft::cameraPosition() const {
  if (!localPlayer_) {
    return {0.0f, 5.0f, 0.0f};
  }
  const float eyeHeight = kEyeHeight - (playerController_.isCrouching() ? kCrouchEyeOffset : 0.0f);
  return {static_cast<float>(localPlayer_->x()), static_cast<float>(localPlayer_->y()) + eyeHeight,
          static_cast<float>(localPlayer_->z())};
}

simd_float3 Minecraft::forwardVector() const {
  const float pitch = playerController_.pitchRadians();
  const float yaw = playerController_.yawRadians();
  const float cp = std::cos(pitch);
  const simd_float3 f{std::sin(yaw) * cp, std::sin(pitch), std::cos(yaw) * cp};
  return normalize(f);
}

simd_float3 Minecraft::rightVector() const {
  const simd_float3 up{0.0f, 1.0f, 0.0f};
  return normalize(cross(up, forwardVector()));
}

simd_float3 Minecraft::upVector() const {
  return normalize(cross(rightVector(), forwardVector()));
}

TerrainViewParams Minecraft::viewParams(float aspect) const {
  const simd_float3 eye = cameraPosition();
  const simd_float3 forward = forwardVector();
  const simd_float3 target = add(eye, forward);
  const simd_float3 up{0.0f, 1.0f, 0.0f};
  const simd_float4x4 view = lookAt(eye, target, up);
  const simd_float4x4 proj = perspective(currentFovRadians_, aspect > 0.001f ? aspect : 1.0f, 0.05f, 300.0f);

  TerrainViewParams params{};
  params.viewProj = mulMat(proj, view);
  return params;
}

bool Minecraft::raycast(float normalizedX, float normalizedY, float aspect, int* hitX, int* hitY, int* hitZ,
                        int* prevX, int* prevY, int* prevZ) const {
  if (!level_) {
    return false;
  }

  const simd_float3 eye = cameraPosition();
  const simd_float3 forward = forwardVector();
  const simd_float3 right = rightVector();
  const simd_float3 up = upVector();

  const float ndcX = normalizedX * 2.0f - 1.0f;
  const float ndcY = normalizedY * 2.0f - 1.0f;
  const float t = std::tan(currentFovRadians_ * 0.5f);

  simd_float3 ray = add(forward, add(mul(right, ndcX * aspect * t), mul(up, ndcY * t)));
  ray = normalize(ray);

  int lastX = static_cast<int>(std::floor(eye.x));
  int lastY = static_cast<int>(std::floor(eye.y));
  int lastZ = static_cast<int>(std::floor(eye.z));

  constexpr float maxDistance = 8.0f;
  constexpr float step = 0.05f;

  for (float d = 0.0f; d <= maxDistance; d += step) {
    const simd_float3 p = add(eye, mul(ray, d));
    const int x = static_cast<int>(std::floor(p.x));
    const int y = static_cast<int>(std::floor(p.y));
    const int z = static_cast<int>(std::floor(p.z));

    if (x == lastX && y == lastY && z == lastZ) {
      continue;
    }

    const int tile = level_->getTile(x, y, z);
    if (tile != static_cast<int>(TileId::Air) && tile != static_cast<int>(TileId::Water)) {
      if (hitX) *hitX = x;
      if (hitY) *hitY = y;
      if (hitZ) *hitZ = z;
      if (prevX) *prevX = lastX;
      if (prevY) *prevY = lastY;
      if (prevZ) *prevZ = lastZ;
      return true;
    }

    lastX = x;
    lastY = y;
    lastZ = z;
  }

  return false;
}

std::optional<BlockInteractionController::Hit> Minecraft::raycastCrosshairHit() const {
  BlockInteractionController::Hit hit{};
  if (!raycast(0.5f, 0.5f, viewAspect_, &hit.x, &hit.y, &hit.z, &hit.prevX, &hit.prevY, &hit.prevZ)) {
    return std::nullopt;
  }
  return hit;
}

GameModeType Minecraft::currentGameModeType() const {
  return gameMode_ ? gameMode_->type() : GameModeType::Survival;
}

bool Minecraft::interactAtCrosshair(bool place) {
  return blockInteractionController_.interactAtCrosshair(
      place, raycastCrosshairHit(), [this](int x, int y, int z) { return placeBlockAt(x, y, z); },
      [this](int x, int y, int z) { return destroyBlockAt(x, y, z); });
}

void Minecraft::rebuildGameMode(GameModeType mode) {
  switch (mode) {
    case GameModeType::Survival: gameMode_ = std::make_unique<SurvivalGameMode>(this); break;
    case GameModeType::Creative: gameMode_ = std::make_unique<CreativeGameMode>(this); break;
    case GameModeType::Spectator: gameMode_ = std::make_unique<SpectatorGameMode>(this); break;
  }

  if (gameMode_) {
    gameMode_->initLevel(level_.get());
    gameMode_->initPlayer(localPlayer_.get());
  }
}

bool Minecraft::lookTargetBlock(int* x, int* y, int* z) const {
  return blockInteractionController_.lookTargetBlock(x, y, z);
}

simd_float3 Minecraft::cameraWorldPosition() const {
  return cameraPosition();
}

float Minecraft::lookYawDegrees() const {
  constexpr float kRadToDeg = 180.0f / 3.1415926535f;
  return playerController_.yawRadians() * kRadToDeg;
}

float Minecraft::lookPitchDegrees() const {
  constexpr float kRadToDeg = 180.0f / 3.1415926535f;
  return playerController_.pitchRadians() * kRadToDeg;
}

bool Minecraft::isCameraUnderwater() const {
  if (!level_ || !localPlayer_) {
    return false;
  }
  const simd_float3 eye = cameraPosition();
  const int tx = static_cast<int>(std::floor(eye.x));
  const int ty = static_cast<int>(std::floor(eye.y));
  const int tz = static_cast<int>(std::floor(eye.z));
  return level_->getTile(tx, ty, tz) == static_cast<int>(TileId::Water);
}

}  // namespace mc
