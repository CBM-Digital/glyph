#include "platform/SDLDesktopApp.h"

#include "audio/AudioSystem.h"
#include "game/GameHost.h"
#include "render/DrawCommand.h"
#include "script/Error.h"

#include <SDL.h>
#if GLYPH_HAS_SDL_IMAGE
#include <SDL_image.h>
#endif
#if GLYPH_HAS_SDL_TTF
#include <SDL_ttf.h>
#endif
#if GLYPH_HAS_SDL_MIXER
#include <SDL_mixer.h>
#endif

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
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

struct GlyphTexture {
  SDL_Texture* texture = nullptr;
  int w = 0;
  int h = 0;
  int bearingX = 0;
  int bearingY = 0;
  long advance = 0;
};

struct FontAsset {
#if GLYPH_HAS_SDL_TTF
  std::string path;
  std::map<int, TTF_Font*> fonts;
#endif
  std::map<std::pair<int, char>, GlyphTexture> glyphs;
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
  std::unordered_map<StringId, FontAsset> fonts;
  std::unordered_map<StringId, AudioAsset> audio;
#if GLYPH_HAS_SDL_MIXER
  bool mixerOpen = false;
  std::unordered_map<StringId, Mix_Chunk*> chunks;
  std::unordered_map<StringId, Mix_Music*> music;
#endif
};

struct Scene {
  game::GameHost host;
  std::filesystem::path file;
  std::filesystem::file_time_type lastWriteTime {};
  std::string reloadError;
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

std::filesystem::path canonicalSceneRoot(const std::filesystem::path& gameFile) {
  std::error_code ec;
  auto root = std::filesystem::weakly_canonical(gameFile.parent_path(), ec);
  return ec ? std::filesystem::absolute(gameFile.parent_path()) : root;
}

bool pathInsideRoot(const std::filesystem::path& root, const std::filesystem::path& candidate) {
  const auto relative = candidate.lexically_relative(root);
  if (relative.empty()) {
    return true;
  }
  auto it = relative.begin();
  return it != relative.end() && *it != ".." && !relative.is_absolute();
}

std::optional<std::filesystem::path> resolveScenePath(const std::filesystem::path& root,
                                                      const std::filesystem::path& currentFile,
                                                      const std::string& target,
                                                      std::string& error) {
  if (target.empty()) {
    error = "navigation target is empty";
    return std::nullopt;
  }

  const std::filesystem::path raw(target);
  std::filesystem::path candidate = raw.is_absolute() ? raw : currentFile.parent_path() / raw;
  if (candidate.extension() != ".glyph") {
    error = "navigation target must be a .glyph file";
    return std::nullopt;
  }

  std::error_code ec;
  candidate = std::filesystem::weakly_canonical(candidate, ec);
  if (ec || !std::filesystem::exists(candidate)) {
    error = "navigation target does not exist: " + target;
    return std::nullopt;
  }
  if (!pathInsideRoot(root, candidate)) {
    error = "navigation target escapes the game bundle: " + target;
    return std::nullopt;
  }
  return candidate;
}

std::unique_ptr<Scene> loadSceneFile(const std::filesystem::path& sceneFile) {
  auto scene = std::make_unique<Scene>();
  scene->file = sceneFile;
  scene->host.loadSource(readFile(sceneFile.string()), sceneFile.string());
  scene->lastWriteTime = std::filesystem::exists(sceneFile) ? std::filesystem::last_write_time(sceneFile)
                                                            : std::filesystem::file_time_type{};
  return scene;
}

void applySceneWindow(SDLState& sdl, const game::GameHost& host) {
  const int width = static_cast<int>(host.definition().logicalSize.x);
  const int height = static_cast<int>(host.definition().logicalSize.y);
  SDL_SetWindowTitle(sdl.window, host.definition().title.c_str());
  SDL_SetWindowSize(sdl.window, width, height);
  SDL_RenderSetLogicalSize(sdl.renderer, width, height);
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

TextureAsset textureFromPixels(SDL_Renderer* renderer, const std::vector<Uint32>& pixels, int width, int height) {
  SDL_Texture* texture =
      SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, width, height);
  SDL_UpdateTexture(texture, nullptr, pixels.data(), width * static_cast<int>(sizeof(Uint32)));
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
  return TextureAsset{texture, width, height};
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
  return textureFromPixels(renderer, pixels, size, size);
}

TextureAsset loadTexture(SDL_Renderer* renderer, const std::filesystem::path& path, StringId name) {
  int w = 0;
  int h = 0;
#if GLYPH_HAS_SDL_IMAGE
  SDL_Surface* imageSurface = IMG_Load(path.string().c_str());
  if (imageSurface) {
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, imageSurface);
    TextureAsset out{texture, imageSurface->w, imageSurface->h};
    SDL_FreeSurface(imageSurface);
    if (texture) {
      SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
      return out;
    }
  }
#endif
  if (auto ppm = loadPPM(path, w, h)) {
    return textureFromPixels(renderer, *ppm, w, h);
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

FontAsset loadFont(const std::filesystem::path& path, SDLState& sdl) {
  FontAsset font;
#if GLYPH_HAS_SDL_TTF
  font.path = path.string();
  if (!std::filesystem::exists(path)) {
    std::cerr << "warning: font file missing " << path << "; using built-in bitmap text\n";
  }
#else
  (void)path;
#endif
  (void)sdl;
  return font;
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
    case assets::AssetType::Font:
      sdl.fonts[asset.name] = loadFont(path, sdl);
      break;
    case assets::AssetType::Sound:
#if GLYPH_HAS_SDL_MIXER
      if (path.extension() != ".tone") {
        sdl.chunks[asset.name] = Mix_LoadWAV(path.string().c_str());
        if (sdl.chunks[asset.name]) {
          break;
        }
      }
#endif
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
#if GLYPH_HAS_SDL_MIXER
      if (path.extension() != ".tone" && path.extension() != ".music") {
        sdl.music[asset.name] = Mix_LoadMUS(path.string().c_str());
        if (sdl.music[asset.name]) {
          break;
        }
      }
#endif
      if (path.extension() == ".tone" || path.extension() == ".music") {
        sdl.audio[asset.name] = loadTone(path, sdl.audioSpec.freq, true);
      } else {
        sdl.audio[asset.name] = loadWav(path, sdl.audioSpec);
      }
      if (sdl.audio[asset.name].samples.empty()) {
        sdl.audio[asset.name] = makeTone(220.0f, 1.5f, 0.18f, sdl.audioSpec.freq);
      }
      break;
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
  for (auto& [_, font] : sdl.fonts) {
    for (auto& [__, glyph] : font.glyphs) {
      if (glyph.texture) {
        SDL_DestroyTexture(glyph.texture);
      }
    }
#if GLYPH_HAS_SDL_TTF
    for (auto& [__, ttf] : font.fonts) {
      if (ttf) {
        TTF_CloseFont(ttf);
      }
    }
#endif
  }
  sdl.fonts.clear();
#if GLYPH_HAS_SDL_MIXER
  for (auto& [_, chunk] : sdl.chunks) {
    if (chunk) {
      Mix_FreeChunk(chunk);
    }
  }
  sdl.chunks.clear();
  for (auto& [_, music] : sdl.music) {
    if (music) {
      Mix_FreeMusic(music);
    }
  }
  sdl.music.clear();
#endif
  sdl.audio.clear();
}

void processAudio(SDLState& sdl, game::GameHost& host) {
  for (const auto& command : host.audio().commands()) {
    switch (command.type) {
    case audio::AudioCommandType::PlaySound:
#if GLYPH_HAS_SDL_MIXER
      if (auto chunk = sdl.chunks.find(command.asset); chunk != sdl.chunks.end() && chunk->second) {
        Mix_VolumeChunk(chunk->second, static_cast<int>(std::clamp(command.volume, 0.0f, 1.0f) * MIX_MAX_VOLUME));
        Mix_PlayChannel(-1, chunk->second, 0);
        break;
      }
#endif
      [[fallthrough]];
    case audio::AudioCommandType::PlayMusic: {
#if GLYPH_HAS_SDL_MIXER
      if (command.type == audio::AudioCommandType::PlayMusic) {
        if (auto music = sdl.music.find(command.asset); music != sdl.music.end() && music->second) {
          Mix_VolumeMusic(static_cast<int>(std::clamp(command.volume, 0.0f, 1.0f) * MIX_MAX_VOLUME));
          Mix_PlayMusic(music->second, -1);
          break;
        }
      }
#endif
      auto found = sdl.audio.find(command.asset);
      if (found != sdl.audio.end()) {
        queueAudio(sdl.audioDevice, found->second, command.volume);
      }
      break;
    }
    case audio::AudioCommandType::StopMusic:
#if GLYPH_HAS_SDL_MIXER
      Mix_HaltMusic();
#endif
      SDL_ClearQueuedAudio(sdl.audioDevice);
      break;
    case audio::AudioCommandType::SetMusicVolume:
#if GLYPH_HAS_SDL_MIXER
      Mix_VolumeMusic(static_cast<int>(std::clamp(command.volume, 0.0f, 1.0f) * MIX_MAX_VOLUME));
#endif
      break;
    }
  }
  host.audio().flush();
}

void activateScene(SDLState& sdl, Scene& scene) {
  clearLoadedAssets(sdl);
  loadSDLAssets(sdl, scene.host, scene.file);
  applySceneWindow(sdl, scene.host);
}

void processNavigation(SDLState& sdl, std::vector<std::unique_ptr<Scene>>& scenes,
                       const std::filesystem::path& root) {
  if (scenes.empty()) {
    return;
  }

  Scene& current = *scenes.back();
  const auto commands = current.host.navigation().commands();
  current.host.navigation().clear();

  for (const auto& command : commands) {
    if (command.type == game::NavigationCommandType::Push) {
      std::string error;
      const auto target = resolveScenePath(root, current.file, command.target, error);
      if (!target) {
        current.reloadError = error;
        std::cerr << "navigation failed: " << error << '\n';
        return;
      }

      try {
        scenes.push_back(loadSceneFile(*target));
        activateScene(sdl, *scenes.back());
        std::cerr << "pushed scene " << target->string() << '\n';
      } catch (const script::ScriptError& err) {
        current.reloadError = err.what();
        std::cerr << "navigation failed: " << current.reloadError << '\n';
      }
      return;
    }

    if (command.type == game::NavigationCommandType::Pop) {
      if (scenes.size() > 1) {
        scenes.pop_back();
        activateScene(sdl, *scenes.back());
        std::cerr << "popped scene\n";
      }
      return;
    }
  }
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

#if GLYPH_HAS_SDL_TTF
TTF_Font* ttfFont(FontAsset& font, int size) {
  if (font.path.empty()) {
    return nullptr;
  }
  size = std::max(8, size);
  auto found = font.fonts.find(size);
  if (found != font.fonts.end()) {
    return found->second;
  }
  TTF_Font* loaded = TTF_OpenFont(font.path.c_str(), size);
  font.fonts[size] = loaded;
  return loaded;
}

bool drawTTFText(SDLState& sdl, const Transform& transform, const render::DrawCommand& command) {
  auto found = sdl.fonts.find(command.font);
  if (found == sdl.fonts.end()) {
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
  SDL_Texture* texture = SDL_CreateTextureFromSurface(sdl.renderer, surface);
  const SDL_FPoint p = apply(transform, static_cast<float>(command.x), static_cast<float>(command.y));
  SDL_FRect dst{p.x, p.y, static_cast<float>(surface->w), static_cast<float>(surface->h)};
  SDL_FreeSurface(surface);
  if (!texture) {
    return false;
  }
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
  SDL_RenderCopyF(sdl.renderer, texture, nullptr, &dst);
  SDL_DestroyTexture(texture);
  return true;
}
#endif

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

int frameIndex(const StringInterner& interner, StringId frame) {
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

void renderCommands(SDLState& sdl, const StringInterner& interner,
                    const std::vector<render::DrawCommand>& commands) {
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
      drawLine(sdl.renderer, a, b, static_cast<float>(command.width * std::abs(current.a)));
      break;
    }
    case render::DrawCommandType::Sprite: {
      auto found = sdl.textures.find(command.image);
      if (found != sdl.textures.end() && found->second.texture) {
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
        SDL_RenderCopyExF(sdl.renderer, found->second.texture, &src, &dst, command.rotation, &pivot,
                          flip);
      }
      break;
    }
    case render::DrawCommandType::Text:
#if GLYPH_HAS_SDL_TTF
      if (command.font != 0 && drawTTFText(sdl, current, command)) {
        break;
      }
#endif
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
#if GLYPH_HAS_SDL_MIXER
  if (sdl.mixerOpen) {
    Mix_CloseAudio();
  }
#endif
#if GLYPH_HAS_SDL_TTF
  TTF_Quit();
#endif
#if GLYPH_HAS_SDL_IMAGE
  IMG_Quit();
#endif
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
  const std::filesystem::path root = canonicalSceneRoot(gameFile);
  std::vector<std::unique_ptr<Scene>> scenes;
  scenes.push_back(loadSceneFile(gameFile));

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) {
    std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
    return 1;
  }

  SDLState sdl;
#if GLYPH_HAS_SDL_IMAGE
  const int imageFlags = IMG_INIT_PNG | IMG_INIT_JPG;
  if ((IMG_Init(imageFlags) & IMG_INIT_PNG) == 0) {
    std::cerr << "warning: SDL_image PNG initialization failed: " << IMG_GetError() << '\n';
  }
#endif
#if GLYPH_HAS_SDL_TTF
  if (TTF_Init() != 0) {
    std::cerr << "warning: SDL_ttf initialization failed; using built-in bitmap text: " << TTF_GetError()
              << '\n';
  }
#endif
#if GLYPH_HAS_SDL_MIXER
  if (Mix_OpenAudio(48000, MIX_DEFAULT_FORMAT, 2, 2048) != 0) {
    std::cerr << "warning: SDL_mixer initialization failed; using queued WAV/tone audio: " << Mix_GetError()
              << '\n';
  } else {
    sdl.mixerOpen = true;
  }
#endif

  const auto& initialHost = scenes.back()->host;
  const int width = static_cast<int>(initialHost.definition().logicalSize.x);
  const int height = static_cast<int>(initialHost.definition().logicalSize.y);
  sdl.window = SDL_CreateWindow(initialHost.definition().title.c_str(), SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED, width, height,
                                SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
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
  sdl.audioSpec = desired;
#if GLYPH_HAS_SDL_MIXER
  if (!sdl.mixerOpen)
#endif
  {
    sdl.audioDevice = SDL_OpenAudioDevice(nullptr, 0, &desired, &sdl.audioSpec, 0);
  }
  if (sdl.audioDevice != 0) {
    SDL_PauseAudioDevice(sdl.audioDevice, 0);
#if GLYPH_HAS_SDL_MIXER
  } else if (sdl.mixerOpen) {
    sdl.audioSpec = desired;
#endif
  } else {
    std::cerr << "warning: audio disabled: " << SDL_GetError() << '\n';
    sdl.audioSpec = desired;
  }

  loadSDLAssets(sdl, scenes.back()->host, scenes.back()->file);
  double reloadPollSeconds = 0.0;

  bool running = true;
  Uint64 last = SDL_GetPerformanceCounter();
  const double frequency = static_cast<double>(SDL_GetPerformanceFrequency());
  int frames = 0;

  while (running) {
    const Uint64 now = SDL_GetPerformanceCounter();
    const double dt = static_cast<double>(now - last) / frequency;
    last = now;

    Scene& scene = *scenes.back();
    game::GameHost& host = scene.host;
    host.input().beginFrame();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      consumeEvent(host, event, running);
    }

    reloadPollSeconds += dt;
    if (reloadPollSeconds >= 0.25) {
      reloadPollSeconds = 0.0;
      if (std::filesystem::exists(scene.file)) {
        const auto writeTime = std::filesystem::last_write_time(scene.file);
        if (writeTime != scene.lastWriteTime) {
          scene.lastWriteTime = writeTime;
          try {
            const auto result =
                host.reloadSourcePreservingState(readFile(scene.file.string()), scene.file.string());
            if (result.success) {
              scene.reloadError.clear();
              clearLoadedAssets(sdl);
              loadSDLAssets(sdl, host, scene.file);
              SDL_SetWindowTitle(sdl.window, host.definition().title.c_str());
              SDL_RenderSetLogicalSize(sdl.renderer, static_cast<int>(host.definition().logicalSize.x),
                                       static_cast<int>(host.definition().logicalSize.y));
              std::cerr << "reloaded " << scene.file.string() << '\n';
            } else {
              scene.reloadError = result.error;
              std::cerr << "reload failed: " << scene.reloadError << '\n';
            }
          } catch (const script::ScriptError& error) {
            scene.reloadError = error.what();
            std::cerr << "reload failed: " << scene.reloadError << '\n';
          }
        }
      }
    }

    host.tick(std::min(dt, 0.25));
    processAudio(sdl, host);
    processNavigation(sdl, scenes, root);
    Scene& renderScene = *scenes.back();
    const auto commands = renderScene.host.renderView();
    renderCommands(sdl, renderScene.host.vm().interner(), commands);
    drawReloadErrorOverlay(sdl.renderer, static_cast<int>(renderScene.host.definition().logicalSize.x),
                           renderScene.reloadError);
    SDL_RenderPresent(sdl.renderer);
    renderScene.host.input().endFrame();

    ++frames;
    if (maxFrames >= 0 && frames >= maxFrames) {
      running = false;
    }
  }

  cleanup(sdl);
  return 0;
}

} // namespace glyph::platform
