#include "assets/AssetManager.h"

#include "script/Error.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <utility>

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

const script::Value* mapField(const script::Value& map, StringInterner& interner, std::string_view key) {
  if (map.kind != script::ValueKind::Map) {
    return nullptr;
  }
  auto found = map.map->find(interner.intern(key));
  return found == map.map->end() ? nullptr : &found->second;
}

int sourceRectNumber(const script::Value& value, std::string_view field) {
  if (value.kind != script::ValueKind::Number || std::floor(value.number) != value.number) {
    throw script::RuntimeError(std::string(field) + " source rect values must be integers");
  }
  return static_cast<int>(value.number);
}

SourceRect parseSourceRect(const script::Value& value, std::string_view field) {
  if (value.kind != script::ValueKind::Vector || value.vector->size() != 4) {
    throw script::RuntimeError(std::string(field) + " source rect must be [x y w h]");
  }
  SourceRect rect;
  rect.x = sourceRectNumber((*value.vector)[0], field);
  rect.y = sourceRectNumber((*value.vector)[1], field);
  rect.w = sourceRectNumber((*value.vector)[2], field);
  rect.h = sourceRectNumber((*value.vector)[3], field);
  if (rect.x < 0 || rect.y < 0 || rect.w <= 0 || rect.h <= 0) {
    throw script::RuntimeError(std::string(field) +
                               " source rect x/y must be non-negative and width/height positive");
  }
  return rect;
}

std::pair<int, int> parseGridPair(const script::Value& value, std::string_view field,
                                  std::pair<int, int> fallback) {
  if (value.kind == script::ValueKind::Nil) {
    return fallback;
  }
  if (value.kind != script::ValueKind::Vector || value.vector->size() != 2) {
    throw script::RuntimeError(std::string(field) + " must be [x y]");
  }
  const int x = sourceRectNumber((*value.vector)[0], field);
  const int y = sourceRectNumber((*value.vector)[1], field);
  if (x < 0 || y < 0) {
    throw script::RuntimeError(std::string(field) + " values must be non-negative");
  }
  return {x, y};
}

void parseGridFrames(AssetInfo& info, const script::Value& grid, StringInterner& interner) {
  if (grid.kind != script::ValueKind::Map) {
    throw script::RuntimeError("asset :grid must be a map");
  }

  const script::Value* tileValue = mapField(grid, interner, ":tile");
  if (!tileValue) {
    throw script::RuntimeError("asset :grid requires :tile [w h]");
  }
  const auto tile = parseGridPair(*tileValue, ":grid :tile", {0, 0});
  if (tile.first <= 0 || tile.second <= 0) {
    throw script::RuntimeError("asset :grid :tile width/height must be positive");
  }

  const auto spacing = mapField(grid, interner, ":spacing")
                           ? parseGridPair(*mapField(grid, interner, ":spacing"), ":grid :spacing", {0, 0})
                           : std::pair<int, int>{0, 0};
  const auto margin = mapField(grid, interner, ":margin")
                          ? parseGridPair(*mapField(grid, interner, ":margin"), ":grid :margin", {0, 0})
                          : std::pair<int, int>{0, 0};

  const script::Value* frames = mapField(grid, interner, ":frames");
  if (!frames || frames->kind != script::ValueKind::Map) {
    throw script::RuntimeError("asset :grid requires :frames map");
  }

  for (const auto& [frameName, frameCell] : *frames->map) {
    const auto cell = parseGridPair(frameCell, ":grid :frames", {0, 0});
    info.frames[frameName] = SourceRect{margin.first + cell.first * (tile.first + spacing.first),
                                        margin.second + cell.second * (tile.second + spacing.second),
                                        tile.first,
                                        tile.second};
  }
}

} // namespace

void AssetManager::loadManifest(const script::Value& manifest, StringInterner& interner) {
  assets_.clear();
  byName_.clear();

  if (manifest.kind == script::ValueKind::Nil) {
    return;
  }
  if (manifest.kind != script::ValueKind::Map) {
    throw script::RuntimeError("asset manifest must be a map");
  }

  for (const auto& [name, value] : *manifest.map) {
    AssetInfo info;
    info.name = name;
    if (value.kind == script::ValueKind::String) {
      info.path = value.text;
    } else if (value.kind == script::ValueKind::Map) {
      const script::Value* path = mapField(value, interner, ":path");
      if (!path || path->kind != script::ValueKind::String) {
        throw script::RuntimeError("asset map entries require string :path");
      }
      info.path = path->text;

      if (const script::Value* allowFullDraw = mapField(value, interner, ":allow-full-draw")) {
        if (allowFullDraw->kind != script::ValueKind::Bool) {
          throw script::RuntimeError("asset :allow-full-draw must be true or false");
        }
        info.allowFullDraw = allowFullDraw->boolean;
      }

      if (const script::Value* frames = mapField(value, interner, ":frames")) {
        if (frames->kind != script::ValueKind::Map) {
          throw script::RuntimeError("asset :frames must be a map");
        }
        for (const auto& [frameName, frameRect] : *frames->map) {
          info.frames[frameName] = parseSourceRect(frameRect, ":frames");
        }
      }
      if (const script::Value* grid = mapField(value, interner, ":grid")) {
        parseGridFrames(info, *grid, interner);
      }
      info.atlas = !info.frames.empty();
    } else {
      throw script::RuntimeError("asset manifest values must be paths or asset maps");
    }
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

const SourceRect* AssetManager::frame(StringId texture, StringId name) const {
  const AssetInfo* asset = info(texture);
  if (!asset) {
    return nullptr;
  }
  auto found = asset->frames.find(name);
  return found == asset->frames.end() ? nullptr : &found->second;
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
