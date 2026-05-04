#include "render/RenderCompiler.h"

#include "script/Error.h"

#include <string>

namespace glyph::render {
namespace {

const std::vector<script::Value>& emptyChildren() {
  static const std::vector<script::Value> empty;
  return empty;
}

} // namespace

RenderCompiler::RenderCompiler(StringInterner& interner, const assets::AssetManager* assets)
    : interner_(interner), assets_(assets) {}

std::vector<DrawCommand> RenderCompiler::compile(const script::Value& root) const {
  std::vector<DrawCommand> commands;
  compileNode(root, commands);
  return commands;
}

void RenderCompiler::compileNode(const script::Value& node, std::vector<DrawCommand>& commands) const {
  if (node.kind == script::ValueKind::Nil) {
    return;
  }
  if (node.kind == script::ValueKind::Vector) {
    for (const auto& child : *node.vector) {
      compileNode(child, commands);
    }
    return;
  }
  if (node.kind != script::ValueKind::Map) {
    fail("render node must be nil, vector, or map");
  }
  compileNodeMap(node, commands);
}

void RenderCompiler::compileNodeMap(const script::Value& node, std::vector<DrawCommand>& commands) const {
  if (nodeIs(node, ":empty")) {
    return;
  }
  if (nodeIs(node, ":group") || nodeIs(node, ":layer")) {
    for (const auto& child : childrenField(node)) {
      compileNode(child, commands);
    }
    return;
  }
  if (nodeIs(node, ":clear")) {
    commands.push_back(compileClear(node));
    return;
  }
  if (nodeIs(node, ":rect")) {
    commands.push_back(compileRect(node));
    return;
  }
  if (nodeIs(node, ":circle")) {
    commands.push_back(compileCircle(node));
    return;
  }
  if (nodeIs(node, ":line")) {
    commands.push_back(compileLine(node));
    return;
  }
  if (nodeIs(node, ":sprite")) {
    commands.push_back(compileSprite(node));
    return;
  }
  if (nodeIs(node, ":text")) {
    commands.push_back(compileText(node));
    return;
  }
  if (nodeIs(node, ":camera")) {
    commands.push_back(compileCamera(node));
    for (const auto& child : childrenField(node)) {
      compileNode(child, commands);
    }
    commands.push_back(DrawCommand{.type = DrawCommandType::PopCamera});
    return;
  }
  if (nodeIs(node, ":transform")) {
    commands.push_back(compileTransform(node));
    for (const auto& child : childrenField(node)) {
      compileNode(child, commands);
    }
    commands.push_back(DrawCommand{.type = DrawCommandType::PopTransform});
    return;
  }

  fail("unknown render node type");
}

DrawCommand RenderCompiler::compileClear(const script::Value& node) const {
  DrawCommand command;
  command.type = DrawCommandType::Clear;
  command.color = stringField(node, ":color", "#000");
  return command;
}

DrawCommand RenderCompiler::compileRect(const script::Value& node) const {
  DrawCommand command;
  command.type = DrawCommandType::Rect;
  command.x = numberField(node, ":x");
  command.y = numberField(node, ":y");
  command.w = numberField(node, ":w");
  command.h = numberField(node, ":h");
  command.color = stringField(node, ":color", "#fff");
  return command;
}

DrawCommand RenderCompiler::compileCircle(const script::Value& node) const {
  DrawCommand command;
  command.type = DrawCommandType::Circle;
  command.x = numberField(node, ":x");
  command.y = numberField(node, ":y");
  command.r = numberField(node, ":r");
  command.color = stringField(node, ":color", "#fff");
  return command;
}

DrawCommand RenderCompiler::compileLine(const script::Value& node) const {
  DrawCommand command;
  command.type = DrawCommandType::Line;
  command.x1 = numberField(node, ":x1");
  command.y1 = numberField(node, ":y1");
  command.x2 = numberField(node, ":x2");
  command.y2 = numberField(node, ":y2");
  command.width = numberField(node, ":width", 1.0);
  command.color = stringField(node, ":color", "#fff");
  return command;
}

DrawCommand RenderCompiler::compileSprite(const script::Value& node) const {
  DrawCommand command;
  command.type = DrawCommandType::Sprite;
  command.image = keywordField(node, ":image");
  if (assets_) {
    command.asset = assets_->texture(command.image);
    if (command.asset.id == 0) {
      fail("sprite references an unknown texture asset");
    }
  }
  command.frame = keywordField(node, ":frame", 0);
  command.x = numberField(node, ":x");
  command.y = numberField(node, ":y");
  command.scale = numberField(node, ":scale", 1.0);
  command.rotation = numberField(node, ":rotation", 0.0);
  command.color = stringField(node, ":color", "#fff");
  return command;
}

DrawCommand RenderCompiler::compileText(const script::Value& node) const {
  DrawCommand command;
  command.type = DrawCommandType::Text;
  command.text = stringField(node, ":value");
  command.font = keywordField(node, ":font", 0);
  if (assets_ && command.font != 0) {
    command.asset = assets_->font(command.font);
    if (command.asset.id == 0) {
      fail("text references an unknown font asset");
    }
  }
  command.x = numberField(node, ":x");
  command.y = numberField(node, ":y");
  command.scale = numberField(node, ":size", 16.0);
  command.color = stringField(node, ":color", "#fff");
  return command;
}

DrawCommand RenderCompiler::compileCamera(const script::Value& node) const {
  DrawCommand command;
  command.type = DrawCommandType::PushCamera;
  command.x = numberField(node, ":x", 0.0);
  command.y = numberField(node, ":y", 0.0);
  command.zoom = numberField(node, ":zoom", 1.0);
  return command;
}

DrawCommand RenderCompiler::compileTransform(const script::Value& node) const {
  DrawCommand command;
  command.type = DrawCommandType::PushTransform;
  command.x = numberField(node, ":x", 0.0);
  command.y = numberField(node, ":y", 0.0);
  command.rotation = numberField(node, ":rotation", 0.0);
  command.scale = numberField(node, ":scale", 1.0);
  return command;
}

const script::Value* RenderCompiler::get(const script::Value& map, std::string_view key) const {
  if (map.kind != script::ValueKind::Map) {
    return nullptr;
  }
  const auto id = interner_.intern(key);
  auto found = map.map->find(id);
  if (found == map.map->end()) {
    return nullptr;
  }
  return &found->second;
}

double RenderCompiler::numberField(const script::Value& map, std::string_view key) const {
  const script::Value* value = get(map, key);
  if (!value || value->kind != script::ValueKind::Number) {
    fail(std::string("missing numeric render field ") + std::string(key));
  }
  return value->number;
}

double RenderCompiler::numberField(const script::Value& map, std::string_view key, double fallback) const {
  const script::Value* value = get(map, key);
  if (!value) {
    return fallback;
  }
  if (value->kind != script::ValueKind::Number) {
    fail(std::string("render field must be numeric ") + std::string(key));
  }
  return value->number;
}

StringId RenderCompiler::keywordField(const script::Value& map, std::string_view key, StringId fallback) const {
  const script::Value* value = get(map, key);
  if (!value) {
    return fallback;
  }
  if (value->kind != script::ValueKind::Keyword) {
    fail(std::string("render field must be keyword ") + std::string(key));
  }
  return value->id;
}

std::string RenderCompiler::stringField(const script::Value& map, std::string_view key,
                                        std::string fallback) const {
  const script::Value* value = get(map, key);
  if (!value) {
    return fallback;
  }
  if (value->kind == script::ValueKind::String) {
    return value->text;
  }
  if (value->kind == script::ValueKind::Keyword) {
    return std::string(interner_.resolve(value->id));
  }
  if (value->kind == script::ValueKind::Number || value->kind == script::ValueKind::Bool ||
      value->kind == script::ValueKind::Nil) {
    return valueToString(*value, interner_);
  }
  fail(std::string("render field must be printable ") + std::string(key));
}

const std::vector<script::Value>& RenderCompiler::childrenField(const script::Value& map) const {
  const script::Value* value = get(map, ":children");
  if (!value) {
    return emptyChildren();
  }
  if (value->kind != script::ValueKind::Vector) {
    fail("render children field must be a vector");
  }
  return *value->vector;
}

bool RenderCompiler::nodeIs(const script::Value& map, std::string_view type) const {
  const script::Value* value = get(map, ":node");
  return value && value->kind == script::ValueKind::Keyword && interner_.resolve(value->id) == type;
}

void RenderCompiler::fail(std::string_view message) const {
  throw script::RuntimeError(std::string(message));
}

} // namespace glyph::render
