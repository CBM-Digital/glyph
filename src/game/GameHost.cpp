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

GameHost::GameHost() {
  vm_.setInputSystem(&input_);
  vm_.setAudioSystem(&audio_);
  vm_.setNavigationSystem(&navigation_);
  audio_.setAssetManager(&assets_);
}

void GameHost::loadSource(std::string_view source, std::string file) {
  LoadedScript loaded = compileSource(source, std::move(file));
  vm_ = std::move(loaded.vm);
  vm_.setInputSystem(&input_);
  vm_.setAudioSystem(&audio_);
  vm_.setNavigationSystem(&navigation_);
  assets_ = std::move(loaded.assets);
  audio_.setAssetManager(&assets_);
  audio_.flush();
  navigation_.clear();
  instance_ = GameInstance{};
  instance_.definition = std::move(loaded.definition);
  instance_.state = instance_.definition.initialState;
  instance_.active = true;
  lastReloadError_.clear();
}

ReloadResult GameHost::reloadSourcePreservingState(std::string_view source, std::string file) {
  try {
    LoadedScript loaded = compileSource(source, std::move(file));
    script::Value preservedState = remapStateForReload(instance_.state, vm_.interner(), loaded.vm.interner());

    vm_ = std::move(loaded.vm);
    vm_.setInputSystem(&input_);
    vm_.setAudioSystem(&audio_);
    vm_.setNavigationSystem(&navigation_);
    assets_ = std::move(loaded.assets);
    instance_.definition = std::move(loaded.definition);
    instance_.state = std::move(preservedState);
    instance_.active = true;
    audio_.setAssetManager(&assets_);
    audio_.flush();
    lastReloadError_.clear();
    return ReloadResult{true, ""};
  } catch (const script::ScriptError& error) {
    lastReloadError_ = error.what();
    return ReloadResult{false, lastReloadError_};
  }
}

GameHost::LoadedScript GameHost::compileSource(std::string_view source, std::string file) {
  LoadedScript loaded;
  loaded.vm.setInputSystem(&input_);
  loaded.vm.setAudioSystem(&audio_);
  loaded.vm.setNavigationSystem(&navigation_);
  loaded.vm.evalSource(source, std::move(file));

  const auto metadata = loaded.vm.globals()->lookup(loaded.vm.interner().intern("__game__"));
  if (!metadata || metadata->kind != script::ValueKind::Map) {
    fail("script did not declare a game");
  }

  loaded.definition = extractDefinition(loaded.vm, *metadata);
  loaded.assets.loadManifest(loaded.definition.assetManifest, loaded.vm.interner());
  return loaded;
}

void GameHost::reset() {
  instance_.state = instance_.definition.initialState;
  instance_.accumulator = 0.0;
  instance_.active = true;
  instance_.paused = false;
  audio_.setAssetManager(&assets_);
  audio_.flush();
}

void GameHost::setPaused(bool paused) {
  instance_.paused = paused;
  if (paused) {
    audio_.pauseAll();
  } else {
    audio_.resumeAll();
  }
}

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
  render::RenderCompiler compiler(vm_.interner(), &assets_);
  return compiler.compile(tree);
}

const GameDefinition& GameHost::definition() const { return instance_.definition; }

const script::Value& GameHost::state() const { return instance_.state; }

void GameHost::setState(script::Value state) { instance_.state = std::move(state); }

script::VM& GameHost::vm() { return vm_; }

input::InputSystem& GameHost::input() { return input_; }

assets::AssetManager& GameHost::assets() { return assets_; }

audio::AudioSystem& GameHost::audio() { return audio_; }

NavigationSystem& GameHost::navigation() { return navigation_; }

const std::string& GameHost::lastReloadError() const { return lastReloadError_; }

script::Value GameHost::metadataField(script::VM& vm, const script::Value& metadata, std::string_view key) {
  auto found = metadata.map->find(vm.interner().intern(key));
  if (found == metadata.map->end()) {
    fail(std::string("game is missing required field ") + std::string(key));
  }
  return found->second;
}

script::Value GameHost::resolveMetadataValue(script::VM& vm, const script::Value& value,
                                             std::string_view fieldName) {
  if (value.kind != script::ValueKind::Keyword) {
    return value;
  }

  std::string name(vm.interner().resolve(value.id));
  if (!name.empty() && name.front() == ':') {
    name.erase(name.begin());
  }

  auto resolved = vm.globals()->lookup(vm.interner().intern(name));
  if (!resolved) {
    fail(std::string("game field ") + std::string(fieldName) + " references undefined symbol " + name);
  }
  return *resolved;
}

GameDefinition GameHost::extractDefinition(script::VM& vm, const script::Value& metadata) {
  GameDefinition definition;

  const script::Value id = metadataField(vm, metadata, ":id");
  if (id.kind != script::ValueKind::Keyword) {
    fail("game :id must be a keyword");
  }
  definition.id = id.id;

  const script::Value title = metadataField(vm, metadata, ":title");
  if (title.kind != script::ValueKind::String) {
    fail("game :title must be a string");
  }
  definition.title = title.text;

  const script::Value size = metadataField(vm, metadata, ":size");
  definition.logicalSize.x = static_cast<float>(vectorNumber(size, 0, ":size"));
  definition.logicalSize.y = static_cast<float>(vectorNumber(size, 1, ":size"));

  definition.initialState = resolveMetadataValue(vm, metadataField(vm, metadata, ":initial"), ":initial");
  definition.updateFn = resolveMetadataValue(vm, metadataField(vm, metadata, ":update"), ":update");
  definition.viewFn = resolveMetadataValue(vm, metadataField(vm, metadata, ":view"), ":view");

  if (definition.updateFn.kind != script::ValueKind::Function &&
      definition.updateFn.kind != script::ValueKind::NativeFunction) {
    fail("game :update must resolve to a function");
  }
  if (definition.viewFn.kind != script::ValueKind::Function &&
      definition.viewFn.kind != script::ValueKind::NativeFunction) {
    fail("game :view must resolve to a function");
  }

  auto assets = metadata.map->find(vm.interner().intern(":assets"));
  definition.assetManifest = assets == metadata.map->end() ? script::Value::nil() : assets->second;

  return definition;
}

script::Value GameHost::remapStateForReload(const script::Value& value, const StringInterner& oldInterner,
                                            StringInterner& newInterner) const {
  switch (value.kind) {
  case script::ValueKind::Nil:
    return script::Value::nil();
  case script::ValueKind::Bool:
    return script::Value::booleanValue(value.boolean);
  case script::ValueKind::Number:
    return script::Value::numberValue(value.number);
  case script::ValueKind::String:
    return script::Value::stringValue(value.text);
  case script::ValueKind::Keyword:
    return script::Value::keywordValue(newInterner.intern(oldInterner.resolve(value.id)));
  case script::ValueKind::Vector: {
    std::vector<script::Value> values;
    values.reserve(value.vector->size());
    for (const auto& item : *value.vector) {
      values.push_back(remapStateForReload(item, oldInterner, newInterner));
    }
    return script::Value::vectorValue(std::move(values));
  }
  case script::ValueKind::Map: {
    std::map<StringId, script::Value> entries;
    for (const auto& [key, item] : *value.map) {
      entries[newInterner.intern(oldInterner.resolve(key))] =
          remapStateForReload(item, oldInterner, newInterner);
    }
    return script::Value::mapValue(std::move(entries));
  }
  case script::ValueKind::Function:
  case script::ValueKind::NativeFunction:
    fail("cannot preserve non-serializable function value in game state during reload");
  }
}

void GameHost::fail(std::string_view message) const { throw script::RuntimeError(std::string(message)); }

} // namespace glyph::game
