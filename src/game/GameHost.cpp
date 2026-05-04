#include "game/GameHost.h"

#include "script/Error.h"

#include <string>

namespace glyph::game {
namespace {

double vectorNumber(const script::Value& vector, std::size_t index, std::string_view field) {
  if (vector.kind != script::ValueKind::Vector || vector.vector->size() <= index ||
      (*vector.vector)[index].kind != script::ValueKind::Number) {
    throw script::RuntimeError(std::string(field) + " must be a numeric vector");
  }
  return (*vector.vector)[index].number;
}

} // namespace

GameHost::GameHost() { vm_.setInputSystem(&input_); }

void GameHost::loadSource(std::string_view source, std::string file) {
  vm_ = script::VM();
  vm_.setInputSystem(&input_);
  vm_.evalSource(source, std::move(file));

  const auto metadata = vm_.globals()->lookup(vm_.interner().intern("__game__"));
  if (!metadata || metadata->kind != script::ValueKind::Map) {
    fail("script did not declare a game");
  }

  instance_ = GameInstance{};
  instance_.definition = extractDefinition(*metadata);
  instance_.state = instance_.definition.initialState;
  instance_.active = true;
}

void GameHost::reset() {
  instance_.state = instance_.definition.initialState;
  instance_.accumulator = 0.0;
  instance_.active = true;
  instance_.paused = false;
}

void GameHost::setPaused(bool paused) { instance_.paused = paused; }

void GameHost::tick(double deltaSeconds) {
  if (!instance_.active || instance_.paused) {
    return;
  }

  instance_.accumulator += deltaSeconds;
  while (instance_.accumulator >= fixedDt) {
    instance_.state = vm_.call(instance_.definition.updateFn,
                              {script::Value::numberValue(fixedDt), instance_.state});
    instance_.accumulator -= fixedDt;
  }
}

std::vector<render::DrawCommand> GameHost::renderView() {
  if (!instance_.active) {
    return {};
  }
  const script::Value tree = vm_.call(instance_.definition.viewFn, {instance_.state});
  render::RenderCompiler compiler(vm_.interner());
  return compiler.compile(tree);
}

const GameDefinition& GameHost::definition() const { return instance_.definition; }

const script::Value& GameHost::state() const { return instance_.state; }

void GameHost::setState(script::Value state) { instance_.state = std::move(state); }

script::VM& GameHost::vm() { return vm_; }

input::InputSystem& GameHost::input() { return input_; }

script::Value GameHost::metadataField(const script::Value& metadata, std::string_view key) {
  auto found = metadata.map->find(vm_.interner().intern(key));
  if (found == metadata.map->end()) {
    fail(std::string("game is missing required field ") + std::string(key));
  }
  return found->second;
}

script::Value GameHost::resolveMetadataValue(const script::Value& value, std::string_view fieldName) {
  if (value.kind != script::ValueKind::Keyword) {
    return value;
  }

  std::string name(vm_.interner().resolve(value.id));
  if (!name.empty() && name.front() == ':') {
    name.erase(name.begin());
  }

  auto resolved = vm_.globals()->lookup(vm_.interner().intern(name));
  if (!resolved) {
    fail(std::string("game field ") + std::string(fieldName) + " references undefined symbol " + name);
  }
  return *resolved;
}

GameDefinition GameHost::extractDefinition(const script::Value& metadata) {
  GameDefinition definition;

  const script::Value id = metadataField(metadata, ":id");
  if (id.kind != script::ValueKind::Keyword) {
    fail("game :id must be a keyword");
  }
  definition.id = id.id;

  const script::Value title = metadataField(metadata, ":title");
  if (title.kind != script::ValueKind::String) {
    fail("game :title must be a string");
  }
  definition.title = title.text;

  const script::Value size = metadataField(metadata, ":size");
  definition.logicalSize.x = static_cast<float>(vectorNumber(size, 0, ":size"));
  definition.logicalSize.y = static_cast<float>(vectorNumber(size, 1, ":size"));

  definition.initialState = resolveMetadataValue(metadataField(metadata, ":initial"), ":initial");
  definition.updateFn = resolveMetadataValue(metadataField(metadata, ":update"), ":update");
  definition.viewFn = resolveMetadataValue(metadataField(metadata, ":view"), ":view");

  if (definition.updateFn.kind != script::ValueKind::Function &&
      definition.updateFn.kind != script::ValueKind::NativeFunction) {
    fail("game :update must resolve to a function");
  }
  if (definition.viewFn.kind != script::ValueKind::Function &&
      definition.viewFn.kind != script::ValueKind::NativeFunction) {
    fail("game :view must resolve to a function");
  }

  auto assets = metadata.map->find(vm_.interner().intern(":assets"));
  definition.assetManifest = assets == metadata.map->end() ? script::Value::nil() : assets->second;

  return definition;
}

void GameHost::fail(std::string_view message) const { throw script::RuntimeError(std::string(message)); }

} // namespace glyph::game
