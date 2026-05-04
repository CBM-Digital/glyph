#include "script/Env.h"

namespace glyph::script {

Env::Env(std::shared_ptr<Env> parentEnv) : parent(std::move(parentEnv)) {}

void Env::define(StringId name, Value value) { bindings[name] = std::move(value); }

std::optional<Value> Env::lookup(StringId name) const {
  auto found = bindings.find(name);
  if (found != bindings.end()) {
    return found->second;
  }
  if (parent) {
    return parent->lookup(name);
  }
  return std::nullopt;
}

} // namespace glyph::script
