# Architecture Map

This document describes the current class structure, ownership model, and runtime boundaries.

## Top-Level Ownership

`AppDelegate` (ObjC++) is the platform entrypoint. It owns rendering device objects and one `mc::Minecraft` instance.

`mc::Minecraft` is the gameplay composition root. It owns or coordinates:

- `Level` world state and chunk generation lifecycle
- `GameMode` strategy (`Survival`, `Creative`, `Spectator`)
- `LevelRenderer` world meshing and draw-list management
- `PlayerController` input-to-player state application
- `BlockInteractionController` break/place interaction sequencing

## Class Responsibilities

### Client/App

- `AppDelegate`
  - Window/view setup, shader pipeline creation, texture loading
  - Event routing only (delegates input policy to `AppInputState`)
  - Per-frame render submission
- `MetalRendererBridge` (inside `AppDelegate.mm`)
  - GPU buffer ownership for chunk meshes and overlays
  - Transfer bridge implementing `mc::MetalRenderer`
- `AppInputState`
  - Translates key/mouse/scroll events into gameplay input state
  - Owns hotbar selection input semantics and inventory gesture logic
- `InventoryView`
  - Draws hotbar/inventory UI, item icons, counts, and tooltips
  - Owns tooltip timeout and inventory hover visuals
- `UiImageHelpers.h`
  - Shared UI image helpers for asset lookup and atlas rect conversion

### Client/Core

- `Minecraft`
  - Orchestrates tick order and interactions between world, controllers, and renderer
  - Keeps mode switching, player wiring, and cross-system policies centralized
- `PlayerController`
  - Applies mode-aware movement/look/crouch/sprint behavior to local player
- `BlockInteractionController`
  - Break hold timing, crack stage progression, cooldowns
  - Mode-aware break semantics (instant creative break, spectator restrictions)

### Client/GameMode

- `GameMode` abstract interface
  - Behavior boundary for block destroy/use logic
- `SurvivalGameMode`, `CreativeGameMode`, `SpectatorGameMode`
  - Concrete policy implementations

### World/Level + World/Chunk

- `Level`
  - World API (`getTile`, `setTile`) + chunk cache + generation queues
  - Listener fanout (`LevelListener`) for render updates
- `ChunkSource`
  - Generator gateway used by `Level`
- `LevelChunk`
  - 16x16 chunk column tile container

### World/Entity

- `Entity`
  - Base transform and motion state
- `Mob`
  - Collision stepping, crouch edge clipping, movement resolution
- `Player`
  - Player-facing movement/camera interactions

### Render/Metal

- `MetalRenderer` interface
  - API boundary from meshing system to backend renderer
- `LevelRenderer`
  - Receives level changes via listener callbacks
  - Maintains visible/dirty/urgent chunk sets
  - Schedules rebuild work under per-frame caps and time budgets
  - Builds overlays (selection + destroy crack)

## Meshing Architecture

`LevelRenderer::buildChunkMesh()` delegates to mesher helpers in `ChunkMeshers.h`:

- `mc::detail::FloraMesher`
  - Emits crossed quads for plant-style cutout tiles
- `mc::detail::TransparentMesher`
  - Emits transparent faces (water/glass/ice/cutout-translucent classes)
- `mc::detail::OpaqueGreedyMesher`
  - Greedy-merges opaque faces to reduce triangle/vertex count

Shared meshing utilities in `mc::detail`:

- `ChunkBuildView` neighbor-aware tile sampling view
- Tile classification helpers (`isTransparentTile`, `isPlantTile`)
- Face visibility helper (`shouldRenderFaceForTile`)
- Atlas/tint helpers (`atlasTileOrigin`, `biomeTintForBlock`)
- Specialized block bounds/tint helpers live in `Client/Render/BlockRender.h` and are consumed by both world and UI icon rendering.

## Tick + Render Pipeline

1. `AppDelegate` gathers input and mouse deltas.
2. `AppInputState` consumes events, updates movement/action state, and emits transient UI triggers.
3. Input state is passed to `Minecraft`.
4. `Minecraft::tick()` updates controllers and world interactions.
5. Block edits trigger `LevelListener` callbacks into `LevelRenderer`.
6. `LevelRenderer::tick()` drains rebuild queues (urgent first, budget-limited).
7. `LevelRenderer` updates draw list and overlays.
8. `AppDelegate` renders opaque, transparent, overlay, and debug passes.

## Chunk Dirtying Policy

- Block edits mark center chunk as urgent.
- Neighbor chunks are marked urgent only when the edited block lies on a chunk border.
- This avoids unnecessary multi-chunk urgent rebuild spikes during rapid mining/placing.

## Asset Loading Policy

- Runtime atlas path:
  - `Assets/terrain.png`

## Extension Guidance

When adding features, keep these boundaries:

- Put platform/render API code in `Client/App`.
- Keep gameplay orchestration in `Minecraft`.
- Keep input translation in controllers, not in `GameMode`.
- Keep mode-specific behavior behind `GameMode` polymorphism.
- Keep meshing algorithms in `ChunkMeshers.h` (or split further by file) and keep `LevelRenderer` as orchestrator.
- Keep world mutation/listener behavior inside `Level`.
