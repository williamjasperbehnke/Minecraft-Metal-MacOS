cmake_minimum_required(VERSION 3.24)

if(APPLE)
  if(NOT CMAKE_C_COMPILER)
    execute_process(
      COMMAND xcrun --find clang
      OUTPUT_VARIABLE _clang_path
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )
    if(_clang_path)
      set(CMAKE_C_COMPILER "${_clang_path}" CACHE FILEPATH "C compiler" FORCE)
    endif()
  endif()

  if(NOT CMAKE_CXX_COMPILER)
    execute_process(
      COMMAND xcrun --find clang++
      OUTPUT_VARIABLE _clangxx_path
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )
    if(_clangxx_path)
      set(CMAKE_CXX_COMPILER "${_clangxx_path}" CACHE FILEPATH "C++ compiler" FORCE)
    endif()
  endif()

  if(NOT CMAKE_OBJCXX_COMPILER)
    execute_process(
      COMMAND xcrun --find clang++
      OUTPUT_VARIABLE _objcxx_path
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
    )
    if(_objcxx_path)
      set(CMAKE_OBJCXX_COMPILER "${_objcxx_path}" CACHE FILEPATH "ObjC++ compiler" FORCE)
    endif()
  endif()
endif()

project(MinecraftMetal LANGUAGES CXX OBJCXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_STANDARD 20)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)

set(SRC
  src/Client/App/main.mm
  src/Client/App/AppDelegate.mm
  src/Client/App/AppInputState.mm
  src/Client/App/AppWorldRenderer.mm
  src/Client/App/CrosshairView.mm
  src/Client/App/DebugHudController.mm
  src/Client/App/DebugHudFormatter.cpp
  src/Client/App/MetalRendererBridge.mm
  src/Client/App/TerrainRenderResources.mm
  src/Client/Core/BlockInteractionController.cpp
  src/Client/Core/Minecraft.cpp
  src/Client/Core/PlayerController.cpp
  src/Client/Debug/ChunkBorderOverlay.cpp
  src/Client/Debug/RenderDebugController.cpp
  src/Client/GameMode/GameMode.cpp
  src/Client/GameMode/CreativeGameMode.cpp
  src/Client/GameMode/SpectatorGameMode.cpp
  src/Client/GameMode/SurvivalGameMode.cpp
  src/Client/Render/Particles/BreakingParticles.cpp
  src/Client/Render/Metal/MetalRenderer.mm
  src/Client/Render/Metal/LevelRenderer.cpp
  src/World/Level/ChunkSource.cpp
  src/World/Level/Gen/BiomeLayers.cpp
  src/World/Level/Gen/BiomeProvider.cpp
  src/World/Level/Gen/Decorators.cpp
  src/World/Level/Gen/OverworldGenerator.cpp
  src/World/Level/Gen/StructureGenerator.cpp
  src/World/Level/ThreadPool.cpp
  src/World/Level/Level.cpp
  src/World/Chunk/LevelChunk.cpp
  src/World/Entity/Entity.cpp
  src/World/Entity/Mob.cpp
  src/World/Entity/Player.cpp
)

add_executable(MinecraftMetal MACOSX_BUNDLE ${SRC})

target_include_directories(MinecraftMetal PRIVATE src)

find_library(COCOA_FRAMEWORK Cocoa)
find_library(METAL_FRAMEWORK Metal)
find_library(METALKIT_FRAMEWORK MetalKit)
find_library(QUARTZCORE_FRAMEWORK QuartzCore)

if(NOT COCOA_FRAMEWORK OR NOT METAL_FRAMEWORK OR NOT METALKIT_FRAMEWORK OR NOT QUARTZCORE_FRAMEWORK)
  message(FATAL_ERROR "Required macOS frameworks not found")
endif()

target_link_libraries(MinecraftMetal PRIVATE
  ${COCOA_FRAMEWORK}
  ${METAL_FRAMEWORK}
  ${METALKIT_FRAMEWORK}
  ${QUARTZCORE_FRAMEWORK}
)

set_target_properties(MinecraftMetal PROPERTIES
  MACOSX_BUNDLE_BUNDLE_NAME "MinecraftMetal"
  MACOSX_BUNDLE_GUI_IDENTIFIER "local.minecraft.metal"
)
