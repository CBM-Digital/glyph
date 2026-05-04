#pragma once

#include "core/Types.h"
#include "script/Value.h"

#include <memory>
#include <optional>
#include <unordered_map>

namespace glyph::script {

struct Env : std::enable_shared_from_this<Env> {
  explicit Env(std::shared_ptr<Env> parent = nullptr);

  void define(StringId name, Value value);
  std::optional<Value> lookup(StringId name) const;

  std::shared_ptr<Env> parent;
  std::unordered_map<StringId, Value> bindings;
};

} // namespace glyph::script
