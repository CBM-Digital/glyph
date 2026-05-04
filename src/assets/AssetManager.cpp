#include "assets/AssetManager.h"

#include "script/Error.h"

#include <algorithm>
#include <cctype>

namespace glyph::assets {
namespace {

bool hasExtension(const std::string& path, std::initializer_list<std::string_view> extensions) {
  auto dot = path.find_last_of('.');
  if (dot == std::string::npos) {
    return false;
  }

  std::string ext = path.substr(dot + 1);
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });

  for (std::string_view candidate : extensions) {
    if (ext == candidate) {
      return true;
    }
  }
  return false;
}

} // namespace

void AssetManager::loadManifest(const script::Value& manifest, StringInterner&) {
  assets_.clear();
  byName_.clear();

  if (manifest.kind == script::ValueKind::Nil) {
    return;
  }
  if (manifest.kind != script::ValueKind::Map) {
    throw script::RuntimeError("asset manifest must be a map");
  }

  for (const auto& [name, value] : *manifest.map) {
    if (value.kind != script::ValueKind::String) {
      throw script::RuntimeError("asset manifest values must be paths");
    }

    AssetInfo info;
    info.name = name;
    info.path = value.text;
    info.type = inferType(info.path);

    const auto handle = AssetHandle{static_cast<u32>(assets_.size() + 1)};
    assets_.push_back(std::move(info));
    byName_[name] = handle;
  }
}

AssetHandle AssetManager::texture(StringId name) const { return find(name, AssetType::Texture); }

AssetHandle AssetManager::font(StringId name) const { return find(name, AssetType::Font); }

AssetHandle AssetManager::sound(StringId name) const { return find(name, AssetType::Sound); }

AssetHandle AssetManager::music(StringId name) const { return find(name, AssetType::Music); }

AssetHandle AssetManager::find(StringId name, AssetType expected) const {
  auto found = byName_.find(name);
  if (found == byName_.end()) {
    return {};
  }

  const AssetInfo* asset = info(name);
  if (!asset || (expected != AssetType::Unknown && asset->type != expected)) {
    return {};
  }
  return found->second;
}

const AssetInfo* AssetManager::info(StringId name) const {
  auto found = byName_.find(name);
  if (found == byName_.end() || found->second.id == 0 || found->second.id > assets_.size()) {
    return nullptr;
  }
  return &assets_[found->second.id - 1];
}

const std::vector<AssetInfo>& AssetManager::assets() const { return assets_; }

AssetType AssetManager::inferType(const std::string& path) {
  if (hasExtension(path, {"png", "jpg", "jpeg", "bmp", "gif", "webp", "ppm"})) {
    return AssetType::Texture;
  }
  if (hasExtension(path, {"ttf", "otf", "font"})) {
    return AssetType::Font;
  }
  if (hasExtension(path, {"wav", "flac", "tone"})) {
    return AssetType::Sound;
  }
  if (hasExtension(path, {"mp3", "ogg", "oga", "xm", "mod", "music"})) {
    return AssetType::Music;
  }
  if (hasExtension(path, {"glyph"})) {
    return AssetType::Script;
  }
  return AssetType::Unknown;
}

} // namespace glyph::assets
