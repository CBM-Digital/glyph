#pragma once

#include "assets/AssetManager.h"
#include "core/StringInterner.h"
#include "render/DrawCommand.h"
#include "script/Value.h"

#include <string_view>
#include <utility>
#include <vector>

namespace glyph::render {

class RenderCompiler {
public:
  explicit RenderCompiler(StringInterner& interner, const assets::AssetManager* assets = nullptr);

  std::vector<DrawCommand> compile(const script::Value& root) const;

private:
  void compileNode(const script::Value& node, std::vector<DrawCommand>& commands) const;
  void compileNodeMap(const script::Value& node, std::vector<DrawCommand>& commands) const;
  DrawCommand compileClear(const script::Value& node) const;
  DrawCommand compileRect(const script::Value& node) const;
  DrawCommand compileCircle(const script::Value& node) const;
  DrawCommand compileLine(const script::Value& node) const;
  DrawCommand compileSprite(const script::Value& node) const;
  DrawCommand compileText(const script::Value& node) const;
  DrawCommand compileCamera(const script::Value& node) const;
  DrawCommand compileTransform(const script::Value& node) const;

  const script::Value* get(const script::Value& map, std::string_view key) const;
  double numberField(const script::Value& map, std::string_view key) const;
  double numberField(const script::Value& map, std::string_view key, double fallback) const;
  bool boolField(const script::Value& map, std::string_view key, bool fallback) const;
  std::pair<double, double> anchorField(const script::Value& map, std::string_view key,
                                        std::pair<double, double> fallback) const;
  assets::SourceRect sourceRectField(const script::Value& value, std::string_view key) const;
  StringId keywordField(const script::Value& map, std::string_view key, StringId fallback = 0) const;
  std::string stringField(const script::Value& map, std::string_view key,
                          std::string fallback = "") const;
  const std::vector<script::Value>& childrenField(const script::Value& map) const;
  bool nodeIs(const script::Value& map, std::string_view type) const;
  [[noreturn]] void fail(std::string_view message) const;

  StringInterner& interner_;
  const assets::AssetManager* assets_ = nullptr;
};

} // namespace glyph::render
