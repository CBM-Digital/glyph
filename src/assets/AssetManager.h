#pragma once

#include "core/Types.h"
#include "script/Value.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace glyph::assets {

enum class AssetType {
  Texture,
  Font,
  Sound,
  Music,
  Script,
  Unknown
};

struct AssetHandle {
  u32 id = 0;
};

struct SourceRect {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
};

struct AssetInfo {
  StringId name = 0;
  AssetType type = AssetType::Unknown;
  std::string path;
  std::unordered_map<StringId, SourceRect> frames;
};

class AssetManager {
public:
  void loadManifest(const script::Value& manifest, StringInterner& interner);

  AssetHandle texture(StringId name) const;
  AssetHandle font(StringId name) const;
  AssetHandle sound(StringId name) const;
  AssetHandle music(StringId name) const;
  AssetHandle find(StringId name, AssetType expected) const;

  const AssetInfo* info(StringId name) const;
  const SourceRect* frame(StringId texture, StringId name) const;
  const std::vector<AssetInfo>& assets() const;

private:
  static AssetType inferType(const std::string& path);

  std::vector<AssetInfo> assets_;
  std::unordered_map<StringId, AssetHandle> byName_;
};

} // namespace glyph::assets
