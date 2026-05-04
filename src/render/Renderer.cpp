#include "render/Renderer.h"

namespace glyph::render {

void CommandBufferRenderer::beginFrame() { commands_.clear(); }

void CommandBufferRenderer::submit(const DrawCommand& command) { commands_.push_back(command); }

void CommandBufferRenderer::endFrame() {}

const std::vector<DrawCommand>& CommandBufferRenderer::commands() const { return commands_; }

} // namespace glyph::render
