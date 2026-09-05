#pragma once
#include "script/Value.h"
#include <filesystem>
#include <map>
#include <string>

namespace glyph::profile {
// Data-only serialization: profile files are never evaluated as programs.
std::string encode(const script::Value&, const StringInterner&);
script::Value decode(const std::string&, StringInterner&);
class ProfileStore {
public:
  explicit ProfileStore(std::filesystem::path path = {});
  script::Value get(const std::string& key, const script::Value& fallback, StringInterner&) const;
  bool set(const std::string& key, const script::Value&, const StringInterner&);
  bool complete(const script::Value& result, StringInterner&);
  const std::string& error() const { return error_; }
private:
  bool load(const std::filesystem::path&);
  bool flush();
  std::filesystem::path path_;
  std::map<std::string, std::string> entries_;
  std::string error_;
  bool writable_ = true;
};
}
