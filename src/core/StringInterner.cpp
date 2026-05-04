#include "core/StringInterner.h"

#include <stdexcept>

namespace glyph {

StringId StringInterner::intern(std::string_view text) {
  auto found = toId_.find(std::string(text));
  if (found != toId_.end()) {
    return found->second;
  }

  const auto id = static_cast<StringId>(fromId_.size() + 1);
  fromId_.emplace_back(text);
  toId_.emplace(fromId_.back(), id);
  return id;
}

std::string_view StringInterner::resolve(StringId id) const {
  if (id == 0 || id > fromId_.size()) {
    throw std::out_of_range("invalid interned string id");
  }
  return fromId_[id - 1];
}

} // namespace glyph
