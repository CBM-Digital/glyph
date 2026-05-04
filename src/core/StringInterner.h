#pragma once

#include "core/Types.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace glyph {

class StringInterner {
public:
  StringId intern(std::string_view text);
  std::string_view resolve(StringId id) const;

private:
  std::unordered_map<std::string, StringId> toId_;
  std::vector<std::string> fromId_;
};

} // namespace glyph
