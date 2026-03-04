# MinecraftMetal

A macOS + Metal voxel game project with a modular C++/Objective-C++ architecture.

## Build

```bash
cd MinecraftMetal
cmake -S . -B build -G "Unix Makefiles" \
  -DCMAKE_CXX_COMPILER="$(xcrun --find clang++)" \
  -DCMAKE_OBJCXX_COMPILER="$(xcrun --find clang++)"
cmake --build build -j 8
open build/MinecraftMetal.app
```

## Runtime Architecture

### Application Layer

- `src/Client/App/AppDelegate.mm`
  - Owns window, `MTKView`, Metal pipelines/sampler/texture, and frame loop.
  - Captures keyboard/mouse input and forwards normalized state into gameplay.
  - Bridges CPU mesh data to GPU buffers via `MetalRendererImpl`.
- `src/Client/App/main.mm`
  - Cocoa app bootstrap.

### Gameplay Composition Root

- `src/Client/Core/Minecraft.{h,cpp}`
  - Top-level orchestrator for game state.
  - Owns/coordinates:
    - `Level` (world data + chunk lifecycle)
    - `GameMode` policy object (survival/creative/spectator)
    - `LevelRenderer` (meshing + draw list generation)
    - `PlayerController` (movement/look state application)
    - `BlockInteractionController` (raycast break/place sequencing)

### Controllers

- `src/Client/Core/PlayerController.{h,cpp}`
  - Converts `InputState` + `GameModeType` into movement intent/FOV target behavior.
- `src/Client/Core/BlockInteractionController.{h,cpp}`
  - Handles break hold progression, cooldown, destroy stages, and block edit calls.
  - Updates destroy overlay state via `LevelRenderer` API.

### Game Modes

- `src/Client/GameMode/GameMode.h`
  - Base interface for block interaction and mode-specific behavior.
- `SurvivalGameMode`, `CreativeGameMode`, `SpectatorGameMode`
  - Encapsulate mode rules behind a single polymorphic boundary.

### World Layer

- `src/World/Level/Level.{h,cpp}`
  - Chunk cache, generation queues, listeners, and world tile access API.
- `src/World/Level/ChunkSource.{h,cpp}`
  - World generation entrypoint for chunk creation.
- `src/World/Chunk/LevelChunk.{h,cpp}`
  - 16x16 column with vertical tile storage.
- `src/World/Level/Gen/*`
  - Terrain/biome/noise/decorator/structure pipeline.

### Entity Layer

- `src/World/Entity/Entity.{h,cpp}`
  - Base transform/physics state.
- `src/World/Entity/Mob.{h,cpp}`
  - Collision, stepping, crouch edge behavior, movement resolution.
- `src/World/Entity/Player.{h,cpp}`
  - Player-specific behavior and camera-facing movement coupling.

### Rendering Layer

- `src/Client/Render/Metal/MetalRenderer.h`
  - Renderer bridge interface consumed by `LevelRenderer`.
- `src/Client/Render/Metal/LevelRenderer.{h,cpp}`
  - Level listener implementation, chunk dirty tracking, chunk rebuild scheduling.
  - Produces draw lists and overlay meshes.
- `src/Client/Render/Metal/ChunkMeshers.h`
  - Meshing helpers split by responsibility:
    - `mc::detail::FloraMesher`
    - `mc::detail::TransparentMesher`
    - `mc::detail::OpaqueGreedyMesher`

## Data/Control Flow

### Per-Frame Loop

1. `AppDelegate` gathers input and mouse deltas.
2. Input is passed to `Minecraft` (`InputState`, break/place hold state).
3. `Minecraft::tick()` updates controllers, player state, and world interactions.
4. `LevelRenderer::tick()` processes dirty chunk rebuild work and draw list updates.
5. `AppDelegate` issues Metal draws for opaque, transparent, overlays, and debug lines.

### Block Edit Path

1. `BlockInteractionController` resolves raycast target and interaction timing.
2. `GameMode` mutates world tiles via `Level`.
3. `Level` notifies `LevelListener`.
4. `LevelRenderer::tileChanged()` marks only necessary chunks dirty/urgent.
5. Rebuild queue is drained under per-frame work/time budgets.

### Chunk Meshing Strategy

- Opaque blocks: greedy meshing to reduce vertex count and draw overhead.
- Transparent blocks: per-face pass to preserve material boundaries.
- Flora cutouts: cross-quad emission in opaque pass (depth-writing) to minimize sorting artifacts.

## Assets

- Local runtime terrain atlas:
  - `Assets/terrain.png`

## Input Summary

- Move: `WASD` / arrows
- Jump: `Space`
- Crouch: `Shift`
- Sprint: `Control` + double-tap forward latch
- Break: hold left mouse
- Place: hold right mouse (repeat interval)
- Debug borders: `B`
- Render mode cycle: `M`
- Toggle creative/spectator: `G` / `V`
