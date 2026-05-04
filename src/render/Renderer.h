#pragma once

#include "render/DrawCommand.h"

#include <vector>

namespace glyph::render {

class Renderer {
public:
  virtual ~Renderer() = default;

  virtual void beginFrame() = 0;
  virtual void submit(const DrawCommand& command) = 0;
  virtual void endFrame() = 0;
};

class CommandBufferRenderer final : public Renderer {
public:
  void beginFrame() override;
  void submit(const DrawCommand& command) override;
  void endFrame() override;

  const std::vector<DrawCommand>& commands() const;

private:
  std::vector<DrawCommand> commands_;
};

} // namespace glyph::render
