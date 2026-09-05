#include "assets/AssetManager.h"
#include "game/GameHost.h"
#include "input/InputSystem.h"
#include "render/DrawCommand.h"
#include "script/Error.h"

#if GLYPH_VISUAL_SNAPSHOT_HAS_SDL
#include <SDL.h>
#include <SDL_image.h>
#if GLYPH_VISUAL_SNAPSHOT_HAS_TTF
#include <SDL_ttf.h>
#endif
#endif

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

std::filesystem::path repoRoot() {
  const auto cwd = std::filesystem::current_path();
  if (std::filesystem::exists(cwd / "examples")) {
    return cwd;
  }
  if (std::filesystem::exists(cwd.parent_path() / "examples")) {
    return cwd.parent_path();
  }
  return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}

std::string readFile(const std::filesystem::path& path) {
  std::ifstream input(path);
  require(static_cast<bool>(input), "open " + path.string());
  std::ostringstream source;
  source << input.rdbuf();
  return source.str();
}

std::filesystem::path snapshotRoot() {
  return repoRoot() / "tests" / "integration" / "snapshots" / "arcade";
}

std::filesystem::path snapshotPath(const std::string& name) {
  return snapshotRoot() / (name + ".png");
}

struct Pixel {
  glyph::u8 r = 0;
  glyph::u8 g = 0;
  glyph::u8 b = 0;
};

struct Bitmap {
  int width = 0;
  int height = 0;
  std::vector<Pixel> pixels;
};

Bitmap makeBitmap(int width, int height) {
  return Bitmap{width, height, std::vector<Pixel>(static_cast<std::size_t>(width * height))};
}

bool samePixel(const Pixel& lhs, const Pixel& rhs) {
  return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b;
}

#if GLYPH_VISUAL_SNAPSHOT_HAS_SDL

void writePNG(const Bitmap& bitmap, const std::filesystem::path& path) {
  std::filesystem::create_directories(path.parent_path());
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, bitmap.width, bitmap.height, 24,
                                                        SDL_PIXELFORMAT_RGB24);
  require(surface != nullptr, std::string("SDL_CreateRGBSurfaceWithFormat: ") + SDL_GetError());
  require(SDL_LockSurface(surface) == 0, std::string("SDL_LockSurface: ") + SDL_GetError());

  auto* rows = static_cast<glyph::u8*>(surface->pixels);
  for (int y = 0; y < bitmap.height; ++y) {
    auto* row = rows + y * surface->pitch;
    for (int x = 0; x < bitmap.width; ++x) {
      const Pixel& pixel = bitmap.pixels[static_cast<std::size_t>(y * bitmap.width + x)];
      row[x * 3 + 0] = pixel.r;
      row[x * 3 + 1] = pixel.g;
      row[x * 3 + 2] = pixel.b;
    }
  }

  SDL_UnlockSurface(surface);
  const int result = IMG_SavePNG(surface, path.string().c_str());
  SDL_FreeSurface(surface);
  require(result == 0, "write snapshot " + path.string() + ": " + IMG_GetError());
}

Bitmap readPNG(const std::filesystem::path& path) {
  SDL_Surface* loaded = IMG_Load(path.string().c_str());
  require(loaded != nullptr, "open snapshot " + path.string() + ": " + IMG_GetError());
  SDL_Surface* surface = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_RGB24, 0);
  SDL_FreeSurface(loaded);
  require(surface != nullptr, "convert snapshot " + path.string() + ": " + SDL_GetError());

  Bitmap bitmap = makeBitmap(surface->w, surface->h);
  require(SDL_LockSurface(surface) == 0, std::string("SDL_LockSurface: ") + SDL_GetError());
  auto* rows = static_cast<glyph::u8*>(surface->pixels);
  for (int y = 0; y < bitmap.height; ++y) {
    const auto* row = rows + y * surface->pitch;
    for (int x = 0; x < bitmap.width; ++x) {
      bitmap.pixels[static_cast<std::size_t>(y * bitmap.width + x)] =
          Pixel{row[x * 3 + 0], row[x * 3 + 1], row[x * 3 + 2]};
    }
  }
  SDL_UnlockSurface(surface);
  SDL_FreeSurface(surface);
  return bitmap;
}

void requireSnapshot(const Bitmap& actual, const std::string& name) {
  const auto expectedPath = snapshotPath(name);
  if (std::getenv("GLYPH_UPDATE_SNAPSHOTS")) {
    writePNG(actual, expectedPath);
    return;
  }

  const Bitmap expected = readPNG(expectedPath);
  if (actual.width != expected.width || actual.height != expected.height) {
    const auto actualPath = std::filesystem::current_path() / "snapshot_failures" / (name + ".actual.png");
    writePNG(actual, actualPath);
    require(false, "snapshot dimensions differ for " + name + "; wrote " + actualPath.string());
  }

  for (std::size_t i = 0; i < actual.pixels.size(); ++i) {
    if (!samePixel(actual.pixels[i], expected.pixels[i])) {
      const auto actualPath = std::filesystem::current_path() / "snapshot_failures" / (name + ".actual.png");
      writePNG(actual, actualPath);
      const int x = static_cast<int>(i % static_cast<std::size_t>(actual.width));
      const int y = static_cast<int>(i / static_cast<std::size_t>(actual.width));
      std::ostringstream message;
      message << "snapshot pixel differs for " << name << " at " << x << "," << y
              << "; wrote " << actualPath;
      require(false, message.str());
    }
  }
}

constexpr float pi = 3.14159265358979323846f;

struct Color {
  glyph::u8 r = 255;
  glyph::u8 g = 255;
  glyph::u8 b = 255;
  glyph::u8 a = 255;
};

struct Transform {
  float a = 1.0f;
  float b = 0.0f;
  float c = 0.0f;
  float d = 1.0f;
  float tx = 0.0f;
  float ty = 0.0f;
};

struct Texture {
  SDL_Texture* texture = nullptr;
  int w = 0;
  int h = 0;
};

struct Font {
  std::string path;
#if GLYPH_VISUAL_SNAPSHOT_HAS_TTF
  std::map<int, TTF_Font*> sizes;
#endif
};

int hexValue(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return 10 + c - 'a';
  }
  if (c >= 'A' && c <= 'F') {
    return 10 + c - 'A';
  }
  return 0;
}

Color parseColor(const std::string& value) {
  if (value.size() == 4 && value[0] == '#') {
    return Color{static_cast<glyph::u8>(hexValue(value[1]) * 17),
                 static_cast<glyph::u8>(hexValue(value[2]) * 17),
                 static_cast<glyph::u8>(hexValue(value[3]) * 17), 255};
  }
  if (value.size() == 7 && value[0] == '#') {
    return Color{static_cast<glyph::u8>(hexValue(value[1]) * 16 + hexValue(value[2])),
                 static_cast<glyph::u8>(hexValue(value[3]) * 16 + hexValue(value[4])),
                 static_cast<glyph::u8>(hexValue(value[5]) * 16 + hexValue(value[6])), 255};
  }
  return Color{};
}

void setColor(SDL_Renderer* renderer, const std::string& value) {
  const Color color = parseColor(value);
  SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
}

SDL_FPoint apply(const Transform& t, float x, float y) {
  return SDL_FPoint{t.a * x + t.c * y + t.tx, t.b * x + t.d * y + t.ty};
}

Transform multiply(const Transform& lhs, const Transform& rhs) {
  return Transform{lhs.a * rhs.a + lhs.c * rhs.b,
                   lhs.b * rhs.a + lhs.d * rhs.b,
                   lhs.a * rhs.c + lhs.c * rhs.d,
                   lhs.b * rhs.c + lhs.d * rhs.d,
                   lhs.a * rhs.tx + lhs.c * rhs.ty + lhs.tx,
                   lhs.b * rhs.tx + lhs.d * rhs.ty + lhs.ty};
}

Transform translateScaleRotate(float x, float y, float scale, float rotationDegrees) {
  const float radians = rotationDegrees * pi / 180.0f;
  const float cs = std::cos(radians) * scale;
  const float sn = std::sin(radians) * scale;
  return Transform{cs, sn, -sn, cs, x, y};
}

std::array<std::string_view, 7> glyphFor(char c) {
  switch (c) {
  case '0':
    return {"111", "101", "101", "101", "101", "101", "111"};
  case '1':
    return {"010", "110", "010", "010", "010", "010", "111"};
  case '2':
    return {"111", "001", "001", "111", "100", "100", "111"};
  case '3':
    return {"111", "001", "001", "111", "001", "001", "111"};
  case '4':
    return {"101", "101", "101", "111", "001", "001", "001"};
  case '5':
    return {"111", "100", "100", "111", "001", "001", "111"};
  case '6':
    return {"111", "100", "100", "111", "101", "101", "111"};
  case '7':
    return {"111", "001", "001", "010", "010", "010", "010"};
  case '8':
    return {"111", "101", "101", "111", "101", "101", "111"};
  case '9':
    return {"111", "101", "101", "111", "001", "001", "111"};
  case 'A':
    return {"010", "101", "101", "111", "101", "101", "101"};
  case 'B':
    return {"110", "101", "101", "110", "101", "101", "110"};
  case 'C':
    return {"111", "100", "100", "100", "100", "100", "111"};
  case 'D':
    return {"110", "101", "101", "101", "101", "101", "110"};
  case 'E':
    return {"111", "100", "100", "111", "100", "100", "111"};
  case 'F':
    return {"111", "100", "100", "111", "100", "100", "100"};
  case 'G':
    return {"111", "100", "100", "101", "101", "101", "111"};
  case 'H':
    return {"101", "101", "101", "111", "101", "101", "101"};
  case 'I':
    return {"111", "010", "010", "010", "010", "010", "111"};
  case 'J':
    return {"001", "001", "001", "001", "001", "101", "111"};
  case 'K':
    return {"101", "101", "110", "100", "110", "101", "101"};
  case 'L':
    return {"100", "100", "100", "100", "100", "100", "111"};
  case 'M':
    return {"101", "111", "111", "101", "101", "101", "101"};
  case 'N':
    return {"101", "111", "111", "111", "101", "101", "101"};
  case 'O':
    return {"111", "101", "101", "101", "101", "101", "111"};
  case 'P':
    return {"111", "101", "101", "111", "100", "100", "100"};
  case 'Q':
    return {"111", "101", "101", "101", "111", "001", "001"};
  case 'R':
    return {"110", "101", "101", "110", "101", "101", "101"};
  case 'S':
    return {"111", "100", "100", "111", "001", "001", "111"};
  case 'T':
    return {"111", "010", "010", "010", "010", "010", "010"};
  case 'U':
    return {"101", "101", "101", "101", "101", "101", "111"};
  case 'V':
    return {"101", "101", "101", "101", "101", "101", "010"};
  case 'W':
    return {"101", "101", "101", "101", "111", "111", "101"};
  case 'X':
    return {"101", "101", "101", "010", "101", "101", "101"};
  case 'Y':
    return {"101", "101", "101", "010", "010", "010", "010"};
  case 'Z':
    return {"111", "001", "001", "010", "100", "100", "111"};
  default:
    return {"000", "000", "000", "000", "000", "000", "000"};
  }
}

void drawText(SDL_Renderer* renderer, const Transform& transform,
              const glyph::render::DrawCommand& command) {
  setColor(renderer, command.color.empty() ? "#fff" : command.color);
  const float pixel = std::max(1.0f, static_cast<float>(command.scale) / 8.0f);
  float cursor = static_cast<float>(command.x);
  for (char raw : command.text) {
    const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(raw)));
    if (c == ' ') {
      cursor += pixel * 4.0f;
      continue;
    }
    const auto glyph = glyphFor(c);
    for (int y = 0; y < 7; ++y) {
      for (int x = 0; x < 3; ++x) {
        if (glyph[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] == '1') {
          const SDL_FPoint p = apply(transform, cursor + x * pixel,
                                     static_cast<float>(command.y) + y * pixel);
          SDL_FRect rect{p.x, p.y, pixel, pixel};
          SDL_RenderFillRectF(renderer, &rect);
        }
      }
    }
    cursor += pixel * 4.0f;
  }
}

#if GLYPH_VISUAL_SNAPSHOT_HAS_TTF
TTF_Font* ttfFont(Font& font, int size) {
  if (font.path.empty()) {
    return nullptr;
  }
  size = std::max(8, size);
  auto found = font.sizes.find(size);
  if (found != font.sizes.end()) {
    return found->second;
  }
  TTF_Font* loaded = TTF_OpenFont(font.path.c_str(), size);
  font.sizes[size] = loaded;
  return loaded;
}
#endif

void drawCircle(SDL_Renderer* renderer, const Transform& transform,
                const glyph::render::DrawCommand& command) {
  setColor(renderer, command.color.empty() ? "#fff" : command.color);
  const SDL_FPoint center = apply(transform, static_cast<float>(command.x),
                                  static_cast<float>(command.y));
  const int radius = static_cast<int>(std::abs(command.r * transform.a));
  for (int y = -radius; y <= radius; ++y) {
    for (int x = -radius; x <= radius; ++x) {
      if (x * x + y * y <= radius * radius) {
        SDL_RenderDrawPointF(renderer, center.x + static_cast<float>(x),
                             center.y + static_cast<float>(y));
      }
    }
  }
}

void drawLine(SDL_Renderer* renderer, const SDL_FPoint& a, const SDL_FPoint& b, float width) {
  width = std::max(1.0f, width);
  const float dx = b.x - a.x;
  const float dy = b.y - a.y;
  const float length = std::sqrt(dx * dx + dy * dy);
  if (length <= 0.0f || width <= 1.0f) {
    SDL_RenderDrawLineF(renderer, a.x, a.y, b.x, b.y);
    return;
  }

  const float nx = -dy / length;
  const float ny = dx / length;
  const int steps = static_cast<int>(std::ceil(width));
  const float start = (static_cast<float>(steps) - 1.0f) * -0.5f;
  for (int i = 0; i < steps; ++i) {
    const float offset = start + static_cast<float>(i);
    SDL_RenderDrawLineF(renderer, a.x + nx * offset, a.y + ny * offset, b.x + nx * offset,
                        b.y + ny * offset);
  }
}

int frameIndex(const glyph::StringInterner& interner, glyph::StringId frame) {
  if (frame == 0) {
    return 0;
  }
  const auto name = interner.resolve(frame);
  if (name.size() > 2 && name[0] == ':' && name[1] == 'f') {
    int index = 0;
    for (std::size_t i = 2; i < name.size(); ++i) {
      if (!std::isdigit(static_cast<unsigned char>(name[i]))) {
        return static_cast<int>(frame % 4);
      }
      index = index * 10 + (name[i] - '0');
    }
    return index;
  }
  if (name == ":block") {
    return 0;
  }
  if (name == ":player") {
    return 1;
  }
  if (name == ":obstacle") {
    return 2;
  }
  if (name == ":target") {
    return 3;
  }
  if (name == ":marker") {
    return 4;
  }
  return static_cast<int>(frame % 4);
}

class ArcadeScreen {
public:
  ArcadeScreen() {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    require(SDL_Init(0) == 0, std::string("SDL_Init: ") + SDL_GetError());
    const int imageFlags = IMG_INIT_PNG;
    require((IMG_Init(imageFlags) & imageFlags) == imageFlags,
            std::string("IMG_Init: ") + IMG_GetError());
#if GLYPH_VISUAL_SNAPSHOT_HAS_TTF
    require(TTF_Init() == 0, std::string("TTF_Init: ") + TTF_GetError());
#endif
  }

  ~ArcadeScreen() {
    destroyAssets();
    destroySurface();
#if GLYPH_VISUAL_SNAPSHOT_HAS_TTF
    TTF_Quit();
#endif
    IMG_Quit();
    SDL_Quit();
  }

  void loadAssets(glyph::game::GameHost& host, const std::filesystem::path& scene) {
    const int logicalWidth = static_cast<int>(host.definition().logicalSize.x);
    const int logicalHeight = static_cast<int>(host.definition().logicalSize.y);
    ensureSurface(logicalWidth, logicalHeight);
    destroyAssets();

    for (const auto& asset : host.assets().assets()) {
      std::filesystem::path path(asset.path);
      if (!path.is_absolute()) {
        path = scene.parent_path() / path;
      }
      if (asset.type == glyph::assets::AssetType::Texture) {
        SDL_Surface* loaded = IMG_Load(path.string().c_str());
        require(loaded != nullptr, std::string("IMG_Load ") + path.string() + ": " + IMG_GetError());
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, loaded);
        const int width = loaded->w;
        const int height = loaded->h;
        SDL_FreeSurface(loaded);
        require(texture != nullptr,
                std::string("SDL_CreateTextureFromSurface ") + path.string() + ": " + SDL_GetError());
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        textures_[asset.name] = Texture{texture, width, height};
      } else if (asset.type == glyph::assets::AssetType::Font) {
        fonts_[asset.name] = Font{path.string()};
      }
    }
  }

  Bitmap render(glyph::StringInterner& interner, const glyph::game::GameHost& host,
                const std::vector<glyph::render::DrawCommand>& commands) {
    const int logicalWidth = static_cast<int>(host.definition().logicalSize.x);
    const int logicalHeight = static_cast<int>(host.definition().logicalSize.y);
    ensureSurface(logicalWidth, logicalHeight);
    SDL_RenderSetLogicalSize(renderer_, logicalWidth, logicalHeight);

    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);

    std::vector<Transform> stack;
    stack.push_back(Transform{});
    for (const auto& command : commands) {
      const Transform current = stack.back();
      switch (command.type) {
      case glyph::render::DrawCommandType::Clear:
        setColor(renderer_, command.color.empty() ? "#000" : command.color);
        SDL_RenderClear(renderer_);
        break;
      case glyph::render::DrawCommandType::Rect: {
        setColor(renderer_, command.color.empty() ? "#fff" : command.color);
        const SDL_FPoint p = apply(current, static_cast<float>(command.x),
                                   static_cast<float>(command.y));
        SDL_FRect rect{p.x, p.y, static_cast<float>(command.w * current.a),
                       static_cast<float>(command.h * current.d)};
        SDL_RenderFillRectF(renderer_, &rect);
        break;
      }
      case glyph::render::DrawCommandType::Circle:
        drawCircle(renderer_, current, command);
        break;
      case glyph::render::DrawCommandType::Line: {
        setColor(renderer_, command.color.empty() ? "#fff" : command.color);
        const SDL_FPoint a = apply(current, static_cast<float>(command.x1),
                                   static_cast<float>(command.y1));
        const SDL_FPoint b = apply(current, static_cast<float>(command.x2),
                                   static_cast<float>(command.y2));
        drawLine(renderer_, a, b, static_cast<float>(command.width * std::abs(current.a)));
        break;
      }
      case glyph::render::DrawCommandType::Sprite:
        drawSprite(interner, current, command);
        break;
      case glyph::render::DrawCommandType::Text:
#if GLYPH_VISUAL_SNAPSHOT_HAS_TTF
        if (drawTTFText(current, command)) {
          break;
        }
#endif
        drawText(renderer_, current, command);
        break;
      case glyph::render::DrawCommandType::PushCamera:
        stack.push_back(multiply(current, Transform{static_cast<float>(command.zoom), 0.0f, 0.0f,
                                                    static_cast<float>(command.zoom),
                                                    static_cast<float>(-command.x * command.zoom),
                                                    static_cast<float>(-command.y * command.zoom)}));
        break;
      case glyph::render::DrawCommandType::PopCamera:
        if (stack.size() > 1) {
          stack.pop_back();
        }
        break;
      case glyph::render::DrawCommandType::PushTransform:
        stack.push_back(multiply(current, translateScaleRotate(static_cast<float>(command.x),
                                                               static_cast<float>(command.y),
                                                               static_cast<float>(command.scale),
                                                               static_cast<float>(command.rotation))));
        break;
      case glyph::render::DrawCommandType::PopTransform:
        if (stack.size() > 1) {
          stack.pop_back();
        }
        break;
      }
    }
    SDL_RenderPresent(renderer_);

    std::vector<std::uint32_t> raw(static_cast<std::size_t>(targetWidth_ * targetHeight_));
    require(SDL_RenderReadPixels(renderer_, nullptr, SDL_PIXELFORMAT_ARGB8888, raw.data(),
                                 targetWidth_ * static_cast<int>(sizeof(std::uint32_t))) == 0,
            std::string("SDL_RenderReadPixels: ") + SDL_GetError());

    Bitmap bitmap = makeBitmap(targetWidth_, targetHeight_);
    for (std::size_t i = 0; i < raw.size(); ++i) {
      const std::uint32_t pixel = raw[i];
      bitmap.pixels[i] = Pixel{static_cast<glyph::u8>((pixel >> 16u) & 0xffu),
                               static_cast<glyph::u8>((pixel >> 8u) & 0xffu),
                               static_cast<glyph::u8>(pixel & 0xffu)};
    }
    return bitmap;
  }

private:
  void ensureSurface(int width, int height) {
    require(width > 0 && height > 0, "snapshot surface size must be positive");
    if (surface_ && renderer_ && width == targetWidth_ && height == targetHeight_) {
      return;
    }
    destroyAssets();
    destroySurface();
    surface_ = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
    require(surface_ != nullptr, std::string("SDL_CreateRGBSurfaceWithFormat: ") + SDL_GetError());
    renderer_ = SDL_CreateSoftwareRenderer(surface_);
    require(renderer_ != nullptr, std::string("SDL_CreateSoftwareRenderer: ") + SDL_GetError());
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    targetWidth_ = width;
    targetHeight_ = height;
  }

  void destroyAssets() {
    for (auto& [_, texture] : textures_) {
      if (texture.texture) {
        SDL_DestroyTexture(texture.texture);
      }
    }
    textures_.clear();
#if GLYPH_VISUAL_SNAPSHOT_HAS_TTF
    for (auto& [_, font] : fonts_) {
      for (auto& [__, size] : font.sizes) {
        if (size) {
          TTF_CloseFont(size);
        }
      }
    }
#endif
    fonts_.clear();
  }

  void destroySurface() {
    if (renderer_) {
      SDL_DestroyRenderer(renderer_);
      renderer_ = nullptr;
    }
    if (surface_) {
      SDL_FreeSurface(surface_);
      surface_ = nullptr;
    }
    targetWidth_ = 0;
    targetHeight_ = 0;
  }

  void drawSprite(const glyph::StringInterner& interner, const Transform& current,
                  const glyph::render::DrawCommand& command) {
    const auto found = textures_.find(command.image);
    if (found == textures_.end() || !found->second.texture) {
      return;
    }

    const SDL_FPoint p = apply(current, static_cast<float>(command.x), static_cast<float>(command.y));
    const float scale = static_cast<float>(command.scale);
    SDL_Rect src{0, 0, found->second.w, found->second.h};
    if (command.hasSourceRect) {
      src.x = command.sourceRect.x;
      src.y = command.sourceRect.y;
      src.w = command.sourceRect.w;
      src.h = command.sourceRect.h;
    } else if (command.frame != 0 && found->second.w >= found->second.h && found->second.h > 0) {
      const int tile = found->second.h;
      const int frames = std::max(1, found->second.w / tile);
      src.x = (frameIndex(interner, command.frame) % frames) * tile;
      src.w = tile;
      src.h = tile;
    }

    SDL_FRect dst{p.x, p.y, src.w * scale, src.h * scale};
    SDL_FPoint pivot{static_cast<float>(command.pivotX * dst.w),
                     static_cast<float>(command.pivotY * dst.h)};
    if (command.hasOrigin) {
      const float originX = static_cast<float>(command.originX * dst.w);
      const float originY = static_cast<float>(command.originY * dst.h);
      if (command.hasPivot && std::abs(command.rotation) > 0.0001) {
        const float radians = static_cast<float>(command.rotation) * pi / 180.0f;
        const float cs = std::cos(radians);
        const float sn = std::sin(radians);
        const float dx = originX - pivot.x;
        const float dy = originY - pivot.y;
        dst.x = p.x - pivot.x - (cs * dx - sn * dy);
        dst.y = p.y - pivot.y - (sn * dx + cs * dy);
      } else {
        dst.x = p.x - originX;
        dst.y = p.y - originY;
      }
    }

    SDL_RendererFlip flip = SDL_FLIP_NONE;
    if (command.flipX) {
      flip = static_cast<SDL_RendererFlip>(flip | SDL_FLIP_HORIZONTAL);
    }
    if (command.flipY) {
      flip = static_cast<SDL_RendererFlip>(flip | SDL_FLIP_VERTICAL);
    }
    SDL_RenderCopyExF(renderer_, found->second.texture, &src, &dst, command.rotation, &pivot, flip);
  }

#if GLYPH_VISUAL_SNAPSHOT_HAS_TTF
  bool drawTTFText(const Transform& current, const glyph::render::DrawCommand& command) {
    auto found = fonts_.find(command.font);
    if (found == fonts_.end()) {
      return false;
    }
    const Color color = parseColor(command.color.empty() ? "#fff" : command.color);
    const int size = static_cast<int>(std::max(8.0, command.scale));
    TTF_Font* font = ttfFont(found->second, size);
    if (!font) {
      return false;
    }
    SDL_Color sdlColor{color.r, color.g, color.b, color.a};
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, command.text.c_str(), sdlColor);
    if (!surface) {
      return false;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    const SDL_FPoint p = apply(current, static_cast<float>(command.x), static_cast<float>(command.y));
    SDL_FRect dst{p.x, p.y, static_cast<float>(surface->w), static_cast<float>(surface->h)};
    SDL_FreeSurface(surface);
    if (!texture) {
      return false;
    }
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_RenderCopyF(renderer_, texture, nullptr, &dst);
    SDL_DestroyTexture(texture);
    return true;
  }
#endif

  SDL_Surface* surface_ = nullptr;
  SDL_Renderer* renderer_ = nullptr;
  int targetWidth_ = 0;
  int targetHeight_ = 0;
  std::unordered_map<glyph::StringId, Texture> textures_;
  std::unordered_map<glyph::StringId, Font> fonts_;
};

struct InputStep {
  std::string name;
  double seconds = 0.0;
  std::vector<std::string_view> press;
  std::vector<std::string_view> release;
  std::map<std::string_view, float> axes;
  std::optional<std::pair<bool, glyph::Vec2>> pointer;
  bool snapshot = true;
  std::string stateExpression;
};

struct Scenario {
  std::string slug;
  std::filesystem::path scene;
  std::vector<InputStep> steps;
};

void applyInput(glyph::game::GameHost& host, const InputStep& step) {
  for (std::string_view action : step.press) {
    host.input().setActionDown(host.vm().interner().intern(std::string(":") + std::string(action)), true);
  }
  for (std::string_view action : step.release) {
    host.input().setActionDown(host.vm().interner().intern(std::string(":") + std::string(action)), false);
  }
  for (const auto& [axis, value] : step.axes) {
    host.input().setAxis(host.vm().interner().intern(std::string(":") + std::string(axis)), value);
  }
  if (step.pointer) {
    host.input().setPointerDown(step.pointer->first, step.pointer->second);
  }
}

void advance(glyph::game::GameHost& host, const InputStep& step) {
  if (!step.stateExpression.empty()) host.setState(host.vm().evalSource(step.stateExpression));
  int frames = static_cast<int>(std::round(step.seconds / glyph::game::GameHost::fixedDt));
  if (step.seconds > 0.0) {
    frames = std::max(1, frames);
  }
  if (frames == 0) {
    return;
  }
  for (int frame = 0; frame < frames; ++frame) {
    host.input().beginFrame();
    if (frame == 0) {
      applyInput(host, step);
    } else {
      for (const auto& [axis, value] : step.axes) {
        host.input().setAxis(host.vm().interner().intern(std::string(":") + std::string(axis)), value);
      }
      if (step.pointer) {
        host.input().setPointerPosition(step.pointer->second);
      }
    }
    host.tick(glyph::game::GameHost::fixedDt);
    host.input().endFrame();
  }
}

void setDeterministicRandomSeed() {
#if defined(_WIN32)
  _putenv_s("GLYPH_RANDOM_SEED", "12648430");
#else
  setenv("GLYPH_RANDOM_SEED", "12648430", 1);
#endif
}

std::vector<Scenario> arcadeScenarios() {
  const auto root = repoRoot() / "examples" / "arcade";
  const std::vector<std::string> slugs{
      "asteroid-belt", "chef-chaos",   "circuit-keep", "crown-cavern", "fishing-cove",
      "fussball-fever", "lane-dodger", "particle-swirl", "perfect-shot", "platform-hop",
      "ski-slalom",    "stack-tower",  "tank-siege",   "rune-relay",   "pirate-plunder-push",
      "market-mayhem", "chip-flip-blitz", "comet-courier", "potion-panic", "harbor-switchyard",
      "starforge-spin"};

  std::vector<Scenario> scenarios;
  scenarios.push_back(Scenario{"demo", root / "demo.glyph", {InputStep{"initial"}}});
  scenarios.push_back(Scenario{"comet_results", root / "comet-courier" / "game.glyph",
    {InputStep{"win",0,{}, {}, {}, {},true,"(assoc (prepare initial) :phase :win :score 1850 :rings 3 :stage 3 :message \"Delivery +550\")"},
     InputStep{"lost",0,{}, {}, {}, {},true,"(assoc (prepare initial) :phase :lost :hull 2 :message \"Docked too fast. Use your brake before arrival.\")"}}});
  scenarios.push_back(Scenario{"alpine_results", root / "ski-slalom" / "game.glyph",
    {InputStep{"win",0,{}, {}, {}, {},true,"(assoc initial :phase :win :score 12733 :passed 24 :best-combo 24 :gate 24 :y 12200)"},
     InputStep{"lost",0,{}, {}, {}, {},true,"(assoc initial :phase :over :score 1250 :passed 6 :best-combo 4 :health 0 :message \"Hit a rock. Jump or take a wider line.\")"}}});
  scenarios.push_back(Scenario{"index", root / "index.glyph",
                               {InputStep{"initial"}, InputStep{"after_0_5s", 0.5}}});
  scenarios.push_back(Scenario{"index_input", root / "index.glyph",
                               {InputStep{"focus_alpine", glyph::game::GameHost::fixedDt, {"right"}}}});
  scenarios.push_back(Scenario{"alpine_input", root / "ski-slalom" / "game.glyph",
                               {InputStep{"briefing"},
                                InputStep{"start", glyph::game::GameHost::fixedDt, {"confirm"}},
                                InputStep{"carve", 2.0, {}, {"confirm"}, {{"move-x",0.35f}}},
                                InputStep{"jump", .2, {"confirm"}, {}, {{"move-x",-0.4f},{"move-y",1.0f}}}}});
  for (const auto& slug : slugs) {
    scenarios.push_back(Scenario{slug, root / slug / "game.glyph",
                                 {InputStep{"initial"}, InputStep{"after_0_5s", 0.5}}});
  }

  scenarios.push_back(Scenario{"lane-dodger_input", root / "lane-dodger" / "game.glyph",
                               {InputStep{"initial"},
                                InputStep{"press_right", glyph::game::GameHost::fixedDt, {"right"}},
                                InputStep{"after_release", 0.35, {}, {"right"}}}});
  scenarios.push_back(Scenario{"platform-hop_input", root / "platform-hop" / "game.glyph",
                               {InputStep{"initial"},
                                InputStep{"hold_left", 0.35, {}, {}, {{"move-x", -1.0f}}},
                                InputStep{"settle", 0.25, {}, {}, {{"move-x", 0.0f}}}}});
  scenarios.push_back(Scenario{"asteroid-belt_input", root / "asteroid-belt" / "game.glyph",
                               {InputStep{"title"},
                                InputStep{"start", glyph::game::GameHost::fixedDt, {"tap"}},
                                InputStep{"thrust_turn", 0.35, {"up", "right"}, {"tap"}},
                                InputStep{"coast", 0.25, {}, {"up", "right"}}}});
  scenarios.push_back(Scenario{"perfect-shot_input", root / "perfect-shot" / "game.glyph",
                               {InputStep{"initial"},
                                InputStep{"charge", 0.25, {"tap"}},
                                InputStep{"release", glyph::game::GameHost::fixedDt, {}, {"tap"}},
                                InputStep{"after_shot", 0.35}}});
  scenarios.push_back(Scenario{"rune-relay_input", root / "rune-relay" / "game.glyph",
                               {InputStep{"initial"},
                                InputStep{"rotate_lane", glyph::game::GameHost::fixedDt, {}, {}, {},
                                          std::pair<bool, glyph::Vec2>{false, glyph::Vec2{240.0f, 276.0f}}},
                                InputStep{"fall", 0.35}}});
  scenarios.push_back(Scenario{"pirate-plunder-push_input", root / "pirate-plunder-push" / "game.glyph",
                               {InputStep{"initial"},
                                InputStep{"dash_right", glyph::game::GameHost::fixedDt, {"right"}},
                                InputStep{"slide", 0.35, {}, {"right"}}}});
  scenarios.push_back(Scenario{"market-mayhem_input", root / "market-mayhem" / "game.glyph",
                               {InputStep{"initial"},
                                InputStep{"draw_route", 0.12, {}, {}, {},
                                          std::pair<bool, glyph::Vec2>{true, glyph::Vec2{372.0f, 100.0f}}},
                                InputStep{"release_route", glyph::game::GameHost::fixedDt, {}, {}, {},
                                          std::pair<bool, glyph::Vec2>{false, glyph::Vec2{372.0f, 100.0f}}},
                                InputStep{"courier_run", 0.35}}});
  scenarios.push_back(Scenario{"chip-flip-blitz_input", root / "chip-flip-blitz" / "game.glyph",
                               {InputStep{"initial"},
                                InputStep{"flip_center", glyph::game::GameHost::fixedDt, {}, {}, {},
                                          std::pair<bool, glyph::Vec2>{false, glyph::Vec2{228.0f, 182.0f}}},
                                InputStep{"settle", 0.35}}});
  scenarios.push_back(Scenario{"comet-courier_input", root / "comet-courier" / "game.glyph",
                               {InputStep{"initial"},
                                InputStep{"ready", glyph::game::GameHost::fixedDt, {"confirm"}},
                                InputStep{"aim_hold", .12, {}, {"confirm"}, {},
                                          std::pair<bool, glyph::Vec2>{true, glyph::Vec2{24.0f, 290.0f}}},
                                InputStep{"launch", glyph::game::GameHost::fixedDt, {}, {}, {},
                                          std::pair<bool, glyph::Vec2>{false, glyph::Vec2{24.0f, 290.0f}}},
                                InputStep{"flight", .65},
                                InputStep{"brake", .1, {"confirm"}}}});
  scenarios.push_back(Scenario{"potion-panic_input", root / "potion-panic" / "game.glyph",
                               {InputStep{"initial"},
                                InputStep{"tap_ingredient", glyph::game::GameHost::fixedDt, {}, {}, {},
                                          std::pair<bool, glyph::Vec2>{false, glyph::Vec2{84.0f, 240.0f}}},
                                InputStep{"splash", 0.35}}});
  scenarios.push_back(Scenario{"harbor-switchyard_input", root / "harbor-switchyard" / "game.glyph",
                               {InputStep{"initial"},
                                InputStep{"toggle_switch", glyph::game::GameHost::fixedDt, {}, {}, {},
                                          std::pair<bool, glyph::Vec2>{false, glyph::Vec2{244.0f, 190.0f}}},
                                InputStep{"boats_move", 0.35}}});
  scenarios.push_back(Scenario{"starforge-spin_input", root / "starforge-spin" / "game.glyph",
                               {InputStep{"initial"},
                                InputStep{"reverse", glyph::game::GameHost::fixedDt, {"tap"}},
                                InputStep{"expand", 0.35, {}, {}, {}, std::pair<bool, glyph::Vec2>{true, glyph::Vec2{240.0f, 198.0f}}},
                                InputStep{"release", glyph::game::GameHost::fixedDt, {}, {"tap"}, {},
                                          std::pair<bool, glyph::Vec2>{false, glyph::Vec2{240.0f, 198.0f}}}}});

  return scenarios;
}

void testArcadeEndToEndSnapshots() {
  setDeterministicRandomSeed();
  ArcadeScreen screen;
  for (const auto& scenario : arcadeScenarios()) {
    glyph::game::GameHost host;
    host.loadSource(readFile(scenario.scene), scenario.scene.string());
    screen.loadAssets(host, scenario.scene);

    for (const auto& step : scenario.steps) {
      advance(host, step);
      if (!step.snapshot) {
        continue;
      }
      requireSnapshot(screen.render(host.vm().interner(), host, host.renderView()),
                      scenario.slug + "_" + step.name);
    }
  }
}

#endif

} // namespace

int main() {
#if GLYPH_VISUAL_SNAPSHOT_HAS_SDL
  try {
    testArcadeEndToEndSnapshots();
  } catch (const glyph::script::ScriptError& error) {
    std::cerr << "ScriptError: " << error.what() << '\n';
    return 1;
  }
  std::cout << "glyph arcade end-to-end snapshot tests passed\n";
  return 0;
#else
  std::cout << "glyph arcade end-to-end snapshot tests skipped: SDL2_image unavailable\n";
  return 0;
#endif
}
