#include "platform/SDLDesktopApp.h"

#include "assets/AssetSource.h"
#include "script/Error.h"

#include <SDL.h>
#include <SDL_main.h>

#include <algorithm>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

bool hasDrivePrefix(std::string_view path) {
  return path.size() >= 2 && ((path[0] >= 'a' && path[0] <= 'z') || (path[0] >= 'A' && path[0] <= 'Z')) &&
         path[1] == ':';
}

std::optional<std::string> normalizeAssetPath(std::string path) {
  std::replace(path.begin(), path.end(), '\\', '/');
  if (path.empty() || path[0] == '/' || hasDrivePrefix(path)) {
    return std::nullopt;
  }

  std::vector<std::string> parts;
  std::size_t start = 0;
  while (start <= path.size()) {
    const std::size_t end = path.find('/', start);
    const std::string part = path.substr(start, end == std::string::npos ? std::string::npos : end - start);
    if (!part.empty() && part != ".") {
      if (part == "..") {
        if (parts.empty()) {
          return std::nullopt;
        }
        parts.pop_back();
      } else {
        parts.push_back(part);
      }
    }
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }

  if (parts.empty()) {
    return std::nullopt;
  }

  std::string normalized;
  for (const auto& part : parts) {
    if (!normalized.empty()) {
      normalized += '/';
    }
    normalized += part;
  }
  return normalized;
}

class SDLAndroidAssetSource final : public glyph::assets::IAssetSource {
public:
  explicit SDLAndroidAssetSource(std::string root) {
    if (auto normalized = normalizeAssetPath(std::move(root))) {
      root_ = *normalized;
    }
  }

  std::optional<std::vector<std::uint8_t>> readBytes(std::string_view path) const override {
    const auto resolved = resolve(path);
    if (!resolved) {
      return std::nullopt;
    }

    SDL_RWops* rw = SDL_RWFromFile(resolved->c_str(), "rb");
    if (!rw) {
      return std::nullopt;
    }

    std::vector<std::uint8_t> bytes;
    const Sint64 size = SDL_RWsize(rw);
    if (size >= 0) {
      bytes.resize(static_cast<std::size_t>(size));
      const std::size_t read = SDL_RWread(rw, bytes.data(), 1, bytes.size());
      SDL_RWclose(rw);
      if (read != bytes.size()) {
        return std::nullopt;
      }
      return bytes;
    }

    std::uint8_t buffer[4096];
    for (;;) {
      const std::size_t read = SDL_RWread(rw, buffer, 1, sizeof(buffer));
      if (read == 0) {
        break;
      }
      bytes.insert(bytes.end(), buffer, buffer + read);
    }
    SDL_RWclose(rw);
    return bytes;
  }

  bool exists(std::string_view path) const override {
    const auto resolved = resolve(path);
    if (!resolved) {
      return false;
    }

    SDL_RWops* rw = SDL_RWFromFile(resolved->c_str(), "rb");
    if (!rw) {
      return false;
    }
    SDL_RWclose(rw);
    return true;
  }

  std::optional<std::filesystem::path> physicalPath(std::string_view path) const override {
    const auto resolved = resolve(path);
    if (!resolved) {
      return std::nullopt;
    }
    return std::filesystem::path(*resolved);
  }

private:
  std::optional<std::string> resolve(std::string_view path) const {
    auto normalized = normalizeAssetPath(std::string(path));
    if (!normalized) {
      return std::nullopt;
    }
    if (root_.empty()) {
      return normalized;
    }
    return root_ + '/' + *normalized;
  }

  std::string root_;
};

int runGlyphAndroid() {
  SDL_SetMainReady();

  glyph::platform::SDLAppOptions options;
  options.fullscreen = true;

  auto assets = std::make_shared<SDLAndroidAssetSource>("examples/arcade");
  return glyph::platform::runSDLApp(std::move(assets), "index.glyph", -1, options);
}

} // namespace

extern "C" int SDL_main(int, char**) {
  try {
    return runGlyphAndroid();
  } catch (const glyph::script::ScriptError& error) {
    std::cerr << error.what() << '\n';
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
  }
  return 1;
}
