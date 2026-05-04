#include "platform/SDLDesktopApp.h"

#include "audio/AudioSystem.h"
#include "game/GameHost.h"
#include "render/DrawCommand.h"
#include "script/Error.h"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace glyph::platform {
namespace {

constexpr float pi = 3.14159265358979323846f;

struct Color {
  Uint8 r = 255;
  Uint8 g = 255;
  Uint8 b = 255;
  Uint8 a = 255;
};

struct Transform {
  float a = 1.0f;
  float b = 0.0f;
  float c = 0.0f;
  float d = 1.0f;
  float tx = 0.0f;
  float ty = 0.0f;
};

struct TextureAsset {
  SDL_Texture* texture = nullptr;
  int w = 0;
  int h = 0;
};

struct AudioAsset {
  std::vector<float> samples;
};

struct SDLState {
  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;
  SDL_AudioDeviceID audioDevice = 0;
  SDL_AudioSpec audioSpec {};
  std::unordered_map<StringId, TextureAsset> textures;
  std::unordered_map<StringId, AudioAsset> audio;
};

std::string readFile(const std::string& file) {
  std::ifstream input(file);
  if (!input) {
    throw script::RuntimeError("unable to open " + file);
  }
  std::ostringstream source;
  source << input.rdbuf();
  return source.str();
}

std::filesystem::path resolveAssetPath(const std::filesystem::path& gameFile, const std::string& assetPath) {
  const std::filesystem::path raw(assetPath);
  if (raw.is_absolute()) {
    return raw;
  }
  return gameFile.parent_path() / raw;
}

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
    return Color{static_cast<Uint8>(hexValue(value[1]) * 17),
                 static_cast<Uint8>(hexValue(value[2]) * 17),
                 static_cast<Uint8>(hexValue(value[3]) * 17), 255};
  }
  if (value.size() == 7 && value[0] == '#') {
    return Color{static_cast<Uint8>(hexValue(value[1]) * 16 + hexValue(value[2])),
                 static_cast<Uint8>(hexValue(value[3]) * 16 + hexValue(value[4])),
                 static_cast<Uint8>(hexValue(value[5]) * 16 + hexValue(value[6])), 255};
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
  Transform out;
  out.a = lhs.a * rhs.a + lhs.c * rhs.b;
  out.b = lhs.b * rhs.a + lhs.d * rhs.b;
  out.c = lhs.a * rhs.c + lhs.c * rhs.d;
  out.d = lhs.b * rhs.c + lhs.d * rhs.d;
  out.tx = lhs.a * rhs.tx + lhs.c * rhs.ty + lhs.tx;
  out.ty = lhs.b * rhs.tx + lhs.d * rhs.ty + lhs.ty;
  return out;
}

Transform translateScaleRotate(float x, float y, float scale, float rotationDegrees) {
  const float radians = rotationDegrees * pi / 180.0f;
  const float cs = std::cos(radians) * scale;
  const float sn = std::sin(radians) * scale;
  return Transform{cs, sn, -sn, cs, x, y};
}

std::optional<std::vector<Uint32>> loadPPM(const std::filesystem::path& path, int& width, int& height) {
  std::ifstream input(path);
  if (!input) {
    return std::nullopt;
  }

  std::string magic;
  input >> magic;
  if (magic != "P3") {
    return std::nullopt;
  }

  input >> width >> height;
  int maxValue = 255;
  input >> maxValue;
  if (width <= 0 || height <= 0 || maxValue <= 0) {
    return std::nullopt;
  }

  std::vector<Uint32> pixels(static_cast<std::size_t>(width * height));
  for (int i = 0; i < width * height; ++i) {
    int r = 0;
    int g = 0;
    int b = 0;
    input >> r >> g >> b;
    r = std::clamp(r * 255 / maxValue, 0, 255);
    g = std::clamp(g * 255 / maxValue, 0, 255);
    b = std::clamp(b * 255 / maxValue, 0, 255);
    pixels[static_cast<std::size_t>(i)] =
        0xff000000u | (static_cast<Uint32>(r) << 16u) | (static_cast<Uint32>(g) << 8u) |
        static_cast<Uint32>(b);
  }
  return pixels;
}

TextureAsset makeFallbackTexture(SDL_Renderer* renderer, StringId name) {
  constexpr int size = 32;
  std::vector<Uint32> pixels(size * size);
  const Uint8 r = static_cast<Uint8>(80 + (name * 53) % 160);
  const Uint8 g = static_cast<Uint8>(80 + (name * 97) % 160);
  const Uint8 b = static_cast<Uint8>(80 + (name * 31) % 160);
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      const bool border = x < 2 || y < 2 || x >= size - 2 || y >= size - 2;
      const bool checker = ((x / 8) + (y / 8)) % 2 == 0;
      const Uint8 shade = static_cast<Uint8>(border ? 255 : (checker ? 210 : 140));
      pixels[static_cast<std::size_t>(y * size + x)] =
          0xff000000u | (static_cast<Uint32>((r * shade) / 255) << 16u) |
          (static_cast<Uint32>((g * shade) / 255) << 8u) | static_cast<Uint32>((b * shade) / 255);
    }
  }
  SDL_Texture* texture =
      SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, size, size);
  SDL_UpdateTexture(texture, nullptr, pixels.data(), size * static_cast<int>(sizeof(Uint32)));
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
  return TextureAsset{texture, size, size};
}

TextureAsset loadTexture(SDL_Renderer* renderer, const std::filesystem::path& path, StringId name) {
  int w = 0;
  int h = 0;
  if (auto ppm = loadPPM(path, w, h)) {
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, w, h);
    SDL_UpdateTexture(texture, nullptr, ppm->data(), w * static_cast<int>(sizeof(Uint32)));
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    return TextureAsset{texture, w, h};
  }

  SDL_Surface* surface = SDL_LoadBMP(path.string().c_str());
  if (surface) {
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    TextureAsset out{texture, surface->w, surface->h};
    SDL_FreeSurface(surface);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    return out;
  }

  std::cerr << "warning: using fallback texture for " << path << '\n';
  return makeFallbackTexture(renderer, name);
}

AudioAsset makeTone(float frequency, float seconds, float volume, int sampleRate) {
  const int frames = std::max(1, static_cast<int>(seconds * sampleRate));
  AudioAsset asset;
  asset.samples.resize(static_cast<std::size_t>(frames * 2));
  for (int i = 0; i < frames; ++i) {
    const float t = static_cast<float>(i) / static_cast<float>(sampleRate);
    const float envelope = std::min(1.0f, static_cast<float>(frames - i) / static_cast<float>(sampleRate) * 8.0f);
    const float sample = std::sin(t * frequency * 2.0f * pi) * volume * envelope;
    asset.samples[static_cast<std::size_t>(i * 2)] = sample;
    asset.samples[static_cast<std::size_t>(i * 2 + 1)] = sample;
  }
  return asset;
}

AudioAsset loadTone(const std::filesystem::path& path, int sampleRate, bool music) {
  std::ifstream input(path);
  float frequency = music ? 220.0f : 660.0f;
  float seconds = music ? 1.5f : 0.18f;
  float volume = music ? 0.18f : 0.35f;
  if (input) {
    input >> frequency >> seconds >> volume;
  }
  return makeTone(frequency, seconds, volume, sampleRate);
}

AudioAsset loadWav(const std::filesystem::path& path, const SDL_AudioSpec& targetSpec) {
  SDL_AudioSpec sourceSpec {};
  Uint8* sourceData = nullptr;
  Uint32 sourceBytes = 0;
  if (!SDL_LoadWAV(path.string().c_str(), &sourceSpec, &sourceData, &sourceBytes)) {
    return {};
  }

  SDL_AudioCVT cvt {};
  if (SDL_BuildAudioCVT(&cvt, sourceSpec.format, sourceSpec.channels, sourceSpec.freq, targetSpec.format,
                        targetSpec.channels, targetSpec.freq) < 0) {
    SDL_FreeWAV(sourceData);
    return {};
  }

  cvt.len = static_cast<int>(sourceBytes);
  cvt.buf = static_cast<Uint8*>(SDL_malloc(static_cast<std::size_t>(cvt.len * cvt.len_mult)));
  std::memcpy(cvt.buf, sourceData, sourceBytes);
  SDL_FreeWAV(sourceData);
  if (SDL_ConvertAudio(&cvt) < 0) {
    SDL_free(cvt.buf);
    return {};
  }

  AudioAsset asset;
  const auto sampleCount = static_cast<std::size_t>(cvt.len_cvt / sizeof(float));
  asset.samples.assign(reinterpret_cast<float*>(cvt.buf), reinterpret_cast<float*>(cvt.buf) + sampleCount);
  SDL_free(cvt.buf);
  return asset;
}

void queueAudio(SDL_AudioDeviceID device, const AudioAsset& asset, float volume) {
  if (device == 0 || asset.samples.empty()) {
    return;
  }
  std::vector<float> scaled = asset.samples;
  for (float& sample : scaled) {
    sample = std::clamp(sample * volume, -1.0f, 1.0f);
  }
  SDL_QueueAudio(device, scaled.data(), static_cast<Uint32>(scaled.size() * sizeof(float)));
}

void loadSDLAssets(SDLState& sdl, game::GameHost& host, const std::filesystem::path& gameFile) {
  for (const auto& asset : host.assets().assets()) {
    const auto path = resolveAssetPath(gameFile, asset.path);
    switch (asset.type) {
    case assets::AssetType::Texture:
      sdl.textures[asset.name] = loadTexture(sdl.renderer, path, asset.name);
      break;
    case assets::AssetType::Sound:
      if (path.extension() == ".tone") {
        sdl.audio[asset.name] = loadTone(path, sdl.audioSpec.freq, false);
      } else {
        sdl.audio[asset.name] = loadWav(path, sdl.audioSpec);
      }
      if (sdl.audio[asset.name].samples.empty()) {
        sdl.audio[asset.name] = makeTone(660.0f, 0.18f, 0.35f, sdl.audioSpec.freq);
      }
      break;
    case assets::AssetType::Music:
      if (path.extension() == ".tone" || path.extension() == ".music") {
        sdl.audio[asset.name] = loadTone(path, sdl.audioSpec.freq, true);
      } else {
        sdl.audio[asset.name] = loadWav(path, sdl.audioSpec);
      }
      if (sdl.audio[asset.name].samples.empty()) {
        sdl.audio[asset.name] = makeTone(220.0f, 1.5f, 0.18f, sdl.audioSpec.freq);
      }
      break;
    case assets::AssetType::Font:
    case assets::AssetType::Script:
    case assets::AssetType::Unknown:
      break;
    }
  }
}

void clearLoadedAssets(SDLState& sdl) {
  for (auto& [_, texture] : sdl.textures) {
    if (texture.texture) {
      SDL_DestroyTexture(texture.texture);
    }
  }
  sdl.textures.clear();
  sdl.audio.clear();
}

void processAudio(SDLState& sdl, game::GameHost& host) {
  for (const auto& command : host.audio().commands()) {
    switch (command.type) {
    case audio::AudioCommandType::PlaySound:
    case audio::AudioCommandType::PlayMusic: {
      auto found = sdl.audio.find(command.asset);
      if (found != sdl.audio.end()) {
        queueAudio(sdl.audioDevice, found->second, command.volume);
      }
      break;
    }
    case audio::AudioCommandType::StopMusic:
      SDL_ClearQueuedAudio(sdl.audioDevice);
      break;
    case audio::AudioCommandType::SetMusicVolume:
      break;
    }
  }
  host.audio().flush();
}

std::string overlayText(std::string value) {
  constexpr std::size_t maxLength = 36;
  if (value.size() > maxLength) {
    value.resize(maxLength);
  }
  for (char& c : value) {
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != ' ') {
      c = ' ';
    }
  }
  return value;
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
  case 'C':
    return {"111", "100", "100", "100", "100", "100", "111"};
  case 'D':
    return {"110", "101", "101", "101", "101", "101", "110"};
  case 'E':
    return {"111", "100", "100", "111", "100", "100", "111"};
  case 'H':
    return {"101", "101", "101", "111", "101", "101", "101"};
  case 'I':
    return {"111", "010", "010", "010", "010", "010", "111"};
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
  case 'R':
    return {"110", "101", "101", "110", "101", "101", "101"};
  case 'S':
    return {"111", "100", "100", "111", "001", "001", "111"};
  case 'T':
    return {"111", "010", "010", "010", "010", "010", "010"};
  case 'U':
    return {"101", "101", "101", "101", "101", "101", "111"};
  case 'Y':
    return {"101", "101", "101", "010", "010", "010", "010"};
  default:
    return {"000", "000", "000", "000", "000", "000", "000"};
  }
}

void drawText(SDL_Renderer* renderer, const Transform& transform, const render::DrawCommand& command) {
  setColor(renderer, command.color.empty() ? "#fff" : command.color);
  const float pixel = std::max(2.0f, static_cast<float>(command.scale) / 8.0f);
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
          const SDL_FPoint p = apply(transform, cursor + x * pixel, static_cast<float>(command.y) + y * pixel);
          SDL_FRect rect{p.x, p.y, pixel, pixel};
          SDL_RenderFillRectF(renderer, &rect);
        }
      }
    }
    cursor += pixel * 4.0f;
  }
}

void drawReloadErrorOverlay(SDL_Renderer* renderer, int width, const std::string& error) {
  if (error.empty()) {
    return;
  }

  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 15, 10, 16, 220);
  SDL_FRect background{8.0f, 8.0f, static_cast<float>(std::max(240, width - 16)), 58.0f};
  SDL_RenderFillRectF(renderer, &background);
  SDL_SetRenderDrawColor(renderer, 255, 85, 85, 255);
  SDL_RenderDrawRectF(renderer, &background);

  render::DrawCommand title;
  title.type = render::DrawCommandType::Text;
  title.x = 16;
  title.y = 18;
  title.scale = 16;
  title.color = "#f55";
  title.text = "RELOAD ERROR";
  drawText(renderer, Transform{}, title);

  render::DrawCommand detail;
  detail.type = render::DrawCommandType::Text;
  detail.x = 16;
  detail.y = 42;
  detail.scale = 12;
  detail.color = "#fff";
  detail.text = overlayText(error);
  drawText(renderer, Transform{}, detail);
}

void drawCircle(SDL_Renderer* renderer, const Transform& transform, const render::DrawCommand& command) {
  setColor(renderer, command.color.empty() ? "#fff" : command.color);
  const SDL_FPoint center = apply(transform, static_cast<float>(command.x), static_cast<float>(command.y));
  const int radius = static_cast<int>(std::abs(command.r * transform.a));
  for (int y = -radius; y <= radius; ++y) {
    for (int x = -radius; x <= radius; ++x) {
      if (x * x + y * y <= radius * radius) {
        SDL_RenderDrawPointF(renderer, center.x + static_cast<float>(x), center.y + static_cast<float>(y));
      }
    }
  }
}

void renderCommands(SDLState& sdl, const std::vector<render::DrawCommand>& commands) {
  std::vector<Transform> stack;
  stack.push_back(Transform{});

  for (const auto& command : commands) {
    Transform current = stack.back();
    switch (command.type) {
    case render::DrawCommandType::Clear:
      setColor(sdl.renderer, command.color.empty() ? "#000" : command.color);
      SDL_RenderClear(sdl.renderer);
      break;
    case render::DrawCommandType::Rect: {
      setColor(sdl.renderer, command.color.empty() ? "#fff" : command.color);
      const SDL_FPoint p = apply(current, static_cast<float>(command.x), static_cast<float>(command.y));
      SDL_FRect rect{p.x, p.y, static_cast<float>(command.w * current.a), static_cast<float>(command.h * current.d)};
      SDL_RenderFillRectF(sdl.renderer, &rect);
      break;
    }
    case render::DrawCommandType::Circle:
      drawCircle(sdl.renderer, current, command);
      break;
    case render::DrawCommandType::Line: {
      setColor(sdl.renderer, command.color.empty() ? "#fff" : command.color);
      const SDL_FPoint a = apply(current, static_cast<float>(command.x1), static_cast<float>(command.y1));
      const SDL_FPoint b = apply(current, static_cast<float>(command.x2), static_cast<float>(command.y2));
      SDL_RenderDrawLineF(sdl.renderer, a.x, a.y, b.x, b.y);
      break;
    }
    case render::DrawCommandType::Sprite: {
      auto found = sdl.textures.find(command.image);
      if (found != sdl.textures.end() && found->second.texture) {
        const SDL_FPoint p = apply(current, static_cast<float>(command.x), static_cast<float>(command.y));
        const float scale = static_cast<float>(command.scale);
        SDL_FRect dst{p.x, p.y, found->second.w * scale, found->second.h * scale};
        SDL_RenderCopyExF(sdl.renderer, found->second.texture, nullptr, &dst, command.rotation, nullptr,
                          SDL_FLIP_NONE);
      }
      break;
    }
    case render::DrawCommandType::Text:
      drawText(sdl.renderer, current, command);
      break;
    case render::DrawCommandType::PushCamera:
      stack.push_back(multiply(current, Transform{static_cast<float>(command.zoom), 0.0f, 0.0f,
                                                  static_cast<float>(command.zoom),
                                                  static_cast<float>(-command.x * command.zoom),
                                                  static_cast<float>(-command.y * command.zoom)}));
      break;
    case render::DrawCommandType::PopCamera:
      if (stack.size() > 1) {
        stack.pop_back();
      }
      break;
    case render::DrawCommandType::PushTransform:
      stack.push_back(multiply(current, translateScaleRotate(static_cast<float>(command.x),
                                                             static_cast<float>(command.y),
                                                             static_cast<float>(command.scale),
                                                             static_cast<float>(command.rotation))));
      break;
    case render::DrawCommandType::PopTransform:
      if (stack.size() > 1) {
        stack.pop_back();
      }
      break;
    }
  }
}

void consumeEvent(game::GameHost& host, const SDL_Event& event, bool& running) {
  const auto tap = host.vm().interner().intern(":tap");
  const auto confirm = host.vm().interner().intern(":confirm");
  const auto cancel = host.vm().interner().intern(":cancel");
  const auto left = host.vm().interner().intern(":left");
  const auto right = host.vm().interner().intern(":right");
  const auto up = host.vm().interner().intern(":up");
  const auto down = host.vm().interner().intern(":down");
  const auto moveX = host.vm().interner().intern(":move-x");
  const auto moveY = host.vm().interner().intern(":move-y");

  switch (event.type) {
  case SDL_QUIT:
    running = false;
    break;
  case SDL_KEYDOWN:
  case SDL_KEYUP: {
    const bool pressed = event.type == SDL_KEYDOWN;
    if (event.key.keysym.sym == SDLK_ESCAPE) {
      host.input().setActionDown(cancel, pressed);
      if (pressed) {
        running = false;
      }
    }
    if (event.key.keysym.sym == SDLK_SPACE || event.key.keysym.sym == SDLK_RETURN) {
      host.input().setActionDown(tap, pressed);
      host.input().setActionDown(confirm, pressed);
    }
    if (event.key.keysym.sym == SDLK_LEFT || event.key.keysym.sym == SDLK_a) {
      host.input().setActionDown(left, pressed);
      host.input().setAxis(moveX, pressed ? -1.0f : 0.0f);
    }
    if (event.key.keysym.sym == SDLK_RIGHT || event.key.keysym.sym == SDLK_d) {
      host.input().setActionDown(right, pressed);
      host.input().setAxis(moveX, pressed ? 1.0f : 0.0f);
    }
    if (event.key.keysym.sym == SDLK_UP || event.key.keysym.sym == SDLK_w) {
      host.input().setActionDown(up, pressed);
      host.input().setAxis(moveY, pressed ? -1.0f : 0.0f);
    }
    if (event.key.keysym.sym == SDLK_DOWN || event.key.keysym.sym == SDLK_s) {
      host.input().setActionDown(down, pressed);
      host.input().setAxis(moveY, pressed ? 1.0f : 0.0f);
    }
    break;
  }
  case SDL_MOUSEBUTTONDOWN:
    host.input().setActionDown(tap, true);
    host.input().setPointerDown(true, Vec2{static_cast<float>(event.button.x), static_cast<float>(event.button.y)});
    break;
  case SDL_MOUSEBUTTONUP: {
    host.input().setActionDown(tap, false);
    const Vec2 pos{static_cast<float>(event.button.x), static_cast<float>(event.button.y)};
    const Vec2 start = host.input().pointerStartPosition();
    host.input().setPointerDown(false, pos);
    const float dx = pos.x - start.x;
    const float dy = pos.y - start.y;
    if (std::abs(dx) > 32.0f || std::abs(dy) > 32.0f) {
      if (std::abs(dx) > std::abs(dy)) {
        host.input().setSwipe(dx < 0 ? input::SwipeDirection::Left : input::SwipeDirection::Right);
      } else {
        host.input().setSwipe(dy < 0 ? input::SwipeDirection::Up : input::SwipeDirection::Down);
      }
    }
    break;
  }
  case SDL_MOUSEMOTION:
    host.input().setPointerPosition(Vec2{static_cast<float>(event.motion.x), static_cast<float>(event.motion.y)});
    break;
  default:
    break;
  }
}

void cleanup(SDLState& sdl) {
  clearLoadedAssets(sdl);
  if (sdl.audioDevice != 0) {
    SDL_CloseAudioDevice(sdl.audioDevice);
  }
  if (sdl.renderer) {
    SDL_DestroyRenderer(sdl.renderer);
  }
  if (sdl.window) {
    SDL_DestroyWindow(sdl.window);
  }
  SDL_Quit();
}

} // namespace

int runSDLDesktop(const std::string& gameFileString, int maxFrames) {
  const std::filesystem::path gameFile(gameFileString);
  game::GameHost host;
  host.loadSource(readFile(gameFileString), gameFileString);

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) {
    std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
    return 1;
  }

  SDLState sdl;
  const int width = static_cast<int>(host.definition().logicalSize.x);
  const int height = static_cast<int>(host.definition().logicalSize.y);
  sdl.window = SDL_CreateWindow(host.definition().title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                width, height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (sdl.window) {
    sdl.renderer = SDL_CreateRenderer(sdl.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!sdl.renderer) {
      sdl.renderer = SDL_CreateRenderer(sdl.window, -1, SDL_RENDERER_SOFTWARE);
    }
  }
  if (!sdl.window || !sdl.renderer) {
    std::cerr << "SDL window/renderer creation failed: " << SDL_GetError() << '\n';
    cleanup(sdl);
    return 1;
  }
  SDL_RenderSetLogicalSize(sdl.renderer, width, height);
  SDL_SetRenderDrawBlendMode(sdl.renderer, SDL_BLENDMODE_BLEND);

  SDL_AudioSpec desired {};
  desired.freq = 48000;
  desired.format = AUDIO_F32SYS;
  desired.channels = 2;
  desired.samples = 2048;
  sdl.audioDevice = SDL_OpenAudioDevice(nullptr, 0, &desired, &sdl.audioSpec, 0);
  if (sdl.audioDevice != 0) {
    SDL_PauseAudioDevice(sdl.audioDevice, 0);
  } else {
    std::cerr << "warning: audio disabled: " << SDL_GetError() << '\n';
    sdl.audioSpec = desired;
  }

  loadSDLAssets(sdl, host, gameFile);
  auto lastWriteTime = std::filesystem::exists(gameFile) ? std::filesystem::last_write_time(gameFile)
                                                        : std::filesystem::file_time_type{};
  double reloadPollSeconds = 0.0;
  std::string reloadError;

  bool running = true;
  Uint64 last = SDL_GetPerformanceCounter();
  const double frequency = static_cast<double>(SDL_GetPerformanceFrequency());
  int frames = 0;

  while (running) {
    const Uint64 now = SDL_GetPerformanceCounter();
    const double dt = static_cast<double>(now - last) / frequency;
    last = now;

    host.input().beginFrame();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      consumeEvent(host, event, running);
    }

    reloadPollSeconds += dt;
    if (reloadPollSeconds >= 0.25) {
      reloadPollSeconds = 0.0;
      if (std::filesystem::exists(gameFile)) {
        const auto writeTime = std::filesystem::last_write_time(gameFile);
        if (writeTime != lastWriteTime) {
          lastWriteTime = writeTime;
          try {
            const auto result = host.reloadSourcePreservingState(readFile(gameFileString), gameFileString);
            if (result.success) {
              reloadError.clear();
              clearLoadedAssets(sdl);
              loadSDLAssets(sdl, host, gameFile);
              SDL_SetWindowTitle(sdl.window, host.definition().title.c_str());
              SDL_RenderSetLogicalSize(sdl.renderer, static_cast<int>(host.definition().logicalSize.x),
                                       static_cast<int>(host.definition().logicalSize.y));
              std::cerr << "reloaded " << gameFileString << '\n';
            } else {
              reloadError = result.error;
              std::cerr << "reload failed: " << reloadError << '\n';
            }
          } catch (const script::ScriptError& error) {
            reloadError = error.what();
            std::cerr << "reload failed: " << reloadError << '\n';
          }
        }
      }
    }

    host.tick(std::min(dt, 0.25));
    processAudio(sdl, host);
    const auto commands = host.renderView();
    renderCommands(sdl, commands);
    drawReloadErrorOverlay(sdl.renderer, static_cast<int>(host.definition().logicalSize.x), reloadError);
    SDL_RenderPresent(sdl.renderer);
    host.input().endFrame();

    ++frames;
    if (maxFrames >= 0 && frames >= maxFrames) {
      running = false;
    }
  }

  cleanup(sdl);
  return 0;
}

} // namespace glyph::platform
