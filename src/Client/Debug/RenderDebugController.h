#pragma once

namespace mc {

class RenderDebugController {
public:
  enum class RenderMode {
    Textured = 0,
    Flat = 1,
    Wireframe = 2,
  };

  void toggleChunkBorders();
  void cycleRenderMode();

  bool showChunkBorders() const { return showChunkBorders_; }
  RenderMode renderMode() const { return renderMode_; }

private:
  bool showChunkBorders_ = false;
  RenderMode renderMode_ = RenderMode::Textured;
};

}  // namespace mc
