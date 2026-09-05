#include "platform/SDLDesktopApp.h"

#include "audio/AudioSystem.h"
#include "render/DrawCommand.h"
#include "runtime/RuntimeShell.h"
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
#include <set>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
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
  std::optional<SDL_FingerID> activeFinger;
  bool backgrounded = false;
  bool userPaused = false;
  float musicVolume = 0.2f;
  SDL_GameController* controller = nullptr;
  std::set<SDL_Keycode> keys;
  std::set<Uint8> controllerButtons;
  float controllerX = 0, controllerY = 0;
  std::optional<SDL_FingerID> steeringFinger;
  Vec2 steeringOrigin{};
  float touchX = 0, touchY = 0;
  bool touchJump = false;
  bool mouseDown = false;
  bool nativeAudioPaused = false;
  std::unordered_map<StringId, TextureAsset> textures;
  std::unordered_map<StringId, FontAsset> fonts;
  std::unordered_map<StringId, AudioAsset> audio;
#if GLYPH_HAS_SDL_MIXER
  bool mixerOpen = false;
  std::unordered_map<StringId, Mix_Chunk*> chunks;
  std::unordered_map<StringId, Mix_Music*> music;
  std::map<std::pair<StringId,int>, Mix_Chunk*> pitched;
#endif
};

void applySceneWindow(SDLState& sdl, const game::GameHost& host) {
  const int width = static_cast<int>(host.definition().logicalSize.x);
  const int height = static_cast<int>(host.definition().logicalSize.y);
  SDL_SetWindowTitle(sdl.window, host.definition().title.c_str());
  // Keep the player's chosen window size across cabinet navigation.
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

void queueAudio(SDL_AudioDeviceID device, const AudioAsset& asset, float volume, float pitch = 1.0f) {
  if (device == 0 || asset.samples.empty()) {
    return;
  }
  pitch = std::clamp(pitch, 0.25f, 4.0f);
  const std::size_t frames = asset.samples.size()/2;
  std::vector<float> scaled(static_cast<std::size_t>(frames / pitch)*2);
  for (std::size_t i=0;i<scaled.size()/2;++i) {
    const double position=i*pitch;
    const auto a=std::min(static_cast<std::size_t>(position),frames-1);
    const auto b=std::min(a+1,frames-1);
    const float frac=static_cast<float>(position-a);
    for(int c=0;c<2;++c) scaled[i*2+c]=asset.samples[a*2+c]*(1-frac)+asset.samples[b*2+c]*frac;
  }
  for (float& sample : scaled) {
    sample = std::clamp(sample * volume, -1.0f, 1.0f);
  }
  SDL_QueueAudio(device, scaled.data(), static_cast<Uint32>(scaled.size() * sizeof(float)));
}

void loadSDLAssets(SDLState& sdl, runtime::RuntimeShell& shell) {
  for (const auto& asset : shell.host().assets().assets()) {
    const auto path = shell.assetPath(asset.path);
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
  Mix_HaltChannel(-1);
  Mix_HaltMusic();
  for (auto& [_, chunk] : sdl.pitched) Mix_FreeChunk(chunk);
  sdl.pitched.clear();
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

#if GLYPH_HAS_SDL_MIXER
Mix_Chunk* pitchedChunk(SDLState& sdl, StringId asset, Mix_Chunk* source, float pitch) {
  const int key=static_cast<int>(std::round(std::clamp(pitch,0.25f,4.0f)*100));
  if(key==100) return source;
  const auto cacheKey=std::make_pair(asset,key);
  if(auto found=sdl.pitched.find(cacheKey);found!=sdl.pitched.end()) return found->second;
  int frequency=0, channels=0; Uint16 format=0;
  if(!Mix_QuerySpec(&frequency,&format,&channels) || format!=AUDIO_S16SYS || channels!=2) return source;
  const auto* samples=reinterpret_cast<const Sint16*>(source->abuf);
  const std::size_t frames=source->alen/(sizeof(Sint16)*2);
  if(frames==0) return source;
  const double rate=key/100.0;
  const std::size_t count=static_cast<std::size_t>(frames/rate);
  auto* output=static_cast<Sint16*>(SDL_malloc(count*2*sizeof(Sint16)));
  if(!output) return source;
  for(std::size_t i=0;i<count;++i) {
    const double position=i*rate;
    const auto a=std::min(static_cast<std::size_t>(position),frames-1), b=std::min(a+1,frames-1);
    const double fraction=position-a;
    for(int c=0;c<2;++c) output[i*2+c]=static_cast<Sint16>(samples[a*2+c]*(1-fraction)+samples[b*2+c]*fraction);
  }
  auto* chunk=Mix_QuickLoad_RAW(reinterpret_cast<Uint8*>(output),static_cast<Uint32>(count*2*sizeof(Sint16)));
  if(!chunk) { SDL_free(output); return source; }
  chunk->allocated=1;
  sdl.pitched[cacheKey]=chunk;
  return chunk;
}
#endif

void processAudio(SDLState& sdl, game::GameHost& host) {
  const auto mute=host.vm().profile()?host.vm().profile()->get(":muted",script::Value::booleanValue(false),host.vm().interner()):script::Value::booleanValue(false);
  const bool muted=mute.kind==script::ValueKind::Bool && mute.boolean;
#if GLYPH_HAS_SDL_MIXER
  if(muted) Mix_Volume(-1,0);
  Mix_VolumeMusic(muted?0:static_cast<int>(sdl.musicVolume*MIX_MAX_VOLUME));
#endif
  for (const auto& command : host.audio().commands()) {
    switch (command.type) {
    case audio::AudioCommandType::PlaySound:
#if GLYPH_HAS_SDL_MIXER
      if (auto chunk = sdl.chunks.find(command.asset); chunk != sdl.chunks.end() && chunk->second) {
        const int channel=Mix_PlayChannel(-1, pitchedChunk(sdl,command.asset,chunk->second,command.pitch),0);
        if(channel>=0) Mix_Volume(channel,static_cast<int>(std::clamp((muted?0.0f:command.volume),0.0f,1.0f)*MIX_MAX_VOLUME));
        break;
      }
#endif
      [[fallthrough]];
    case audio::AudioCommandType::PlayMusic: {
#if GLYPH_HAS_SDL_MIXER
      if (command.type == audio::AudioCommandType::PlayMusic) {
        if (auto music = sdl.music.find(command.asset); music != sdl.music.end() && music->second) {
          sdl.musicVolume=command.volume;
          Mix_VolumeMusic(static_cast<int>(std::clamp((muted?0.0f:command.volume), 0.0f, 1.0f) * MIX_MAX_VOLUME));
          Mix_PlayMusic(music->second, -1);
          break;
        }
      }
#endif
      auto found = sdl.audio.find(command.asset);
      if (found != sdl.audio.end()) {
        queueAudio(sdl.audioDevice, found->second, muted?0.0f:command.volume, command.pitch);
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
      Mix_VolumeMusic(static_cast<int>(std::clamp((muted?0.0f:command.volume), 0.0f, 1.0f) * MIX_MAX_VOLUME));
#endif
      break;
    }
  }
  host.audio().flush();
}

void activateScene(SDLState& sdl, runtime::RuntimeShell& shell) {
  clearLoadedAssets(sdl);
  loadSDLAssets(sdl, shell);
  applySceneWindow(sdl, shell.host());
  for(const auto& asset:shell.host().assets().assets()) {
    if(asset.type==assets::AssetType::Music) {
      audio::AudioCommand command{}; command.type=audio::AudioCommandType::PlayMusic; command.asset=asset.name;
      command.handle=shell.host().assets().music(asset.name); command.volume=0.2f;
      shell.host().audio().enqueue(command); break;
    }
  }
}

void setNativeAudioPaused(SDLState& sdl, bool paused) {
  if (sdl.nativeAudioPaused == paused) {
    return;
  }

  if (sdl.audioDevice != 0) {
    SDL_PauseAudioDevice(sdl.audioDevice, paused ? 1 : 0);
  }
#if GLYPH_HAS_SDL_MIXER
  if (sdl.mixerOpen) {
    if (paused) {
      Mix_Pause(-1);
      Mix_PauseMusic();
    } else {
      Mix_Resume(-1);
      Mix_ResumeMusic();
    }
  }
#endif
  sdl.nativeAudioPaused = paused;
}

void updatePause(SDLState& sdl, runtime::RuntimeShell& shell) {
  shell.setPaused(sdl.backgrounded || sdl.userPaused);
  setNativeAudioPaused(sdl, shell.paused());
}
void clearControls(SDLState& sdl, runtime::RuntimeShell& shell) {
  sdl.keys.clear(); sdl.controllerButtons.clear();
  sdl.activeFinger.reset(); sdl.steeringFinger.reset();
  sdl.touchX=sdl.touchY=sdl.controllerX=sdl.controllerY=0;
  sdl.touchJump=sdl.mouseDown=false;
  shell.input().clear();
}
void setLifecyclePaused(SDLState& sdl, runtime::RuntimeShell& shell, bool paused) {
  sdl.backgrounded=paused;
  if(paused) { clearControls(sdl,shell); sdl.userPaused=true; }
  updatePause(sdl,shell);
}
void refreshControls(SDLState& sdl, runtime::RuntimeShell& shell) {
  const auto key=[&](SDL_Keycode k){ return sdl.keys.count(k)>0; };
  const auto button=[&](Uint8 b){ return sdl.controllerButtons.count(b)>0; };
  const bool left=key(SDLK_a)||key(SDLK_LEFT)||button(SDL_CONTROLLER_BUTTON_DPAD_LEFT);
  const bool right=key(SDLK_d)||key(SDLK_RIGHT)||button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
  const bool up=key(SDLK_w)||key(SDLK_UP)||button(SDL_CONTROLLER_BUTTON_DPAD_UP);
  const bool down=key(SDLK_s)||key(SDLK_DOWN)||button(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
  shell.setAxis(":move-x",std::clamp(float(right)-float(left)+sdl.controllerX+sdl.touchX,-1.0f,1.0f));
  shell.setAxis(":move-y",std::clamp(float(down)-float(up)+sdl.controllerY+sdl.touchY,-1.0f,1.0f));
  shell.setActionDown(":left",left); shell.setActionDown(":right",right);
  shell.setActionDown(":up",up); shell.setActionDown(":down",down);
  const bool confirm=key(SDLK_SPACE)||key(SDLK_RETURN)||button(SDL_CONTROLLER_BUTTON_A);
  shell.setActionDown(":confirm",confirm);
  shell.setActionDown(":tap",confirm||sdl.mouseDown||sdl.touchJump||sdl.activeFinger.has_value());
  shell.setActionDown(":secondary",key(SDLK_x)||button(SDL_CONTROLLER_BUTTON_X));
  shell.setActionDown(":cancel",key(SDLK_BACKSPACE)||button(SDL_CONTROLLER_BUTTON_B));
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

void drawText(SDL_Renderer* renderer, const Transform& transform, const render::DrawCommand& command) {
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
  // Android assets live inside the APK, so filesystem::exists is not a valid check.
  SDL_RWops* stream = SDL_RWFromFile(font.path.c_str(), "rb");
  TTF_Font* loaded = stream ? TTF_OpenFontRW(stream, 1, size) : nullptr;
  if (!loaded) std::cerr << "warning: could not load font " << font.path << ": " << TTF_GetError() << '\n';
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

Vec2 normalizedTouchToLogical(const SDLState& sdl, float x, float y) {
#if SDL_VERSION_ATLEAST(2, 0, 18)
  int windowWidth = 0;
  int windowHeight = 0;
  SDL_GetWindowSize(sdl.window, &windowWidth, &windowHeight);

  const int windowX = static_cast<int>(std::round(x * static_cast<float>(windowWidth)));
  const int windowY = static_cast<int>(std::round(y * static_cast<float>(windowHeight)));
  float logicalX = 0.0f;
  float logicalY = 0.0f;
  SDL_RenderWindowToLogical(sdl.renderer, windowX, windowY, &logicalX, &logicalY);
  return Vec2{logicalX, logicalY};
#else
  int logicalWidth = 0;
  int logicalHeight = 0;
  SDL_RenderGetLogicalSize(sdl.renderer, &logicalWidth, &logicalHeight);
  return Vec2{x * static_cast<float>(logicalWidth), y * static_cast<float>(logicalHeight)};
#endif
}

void setSwipeFromPointerRelease(runtime::RuntimeShell& shell, Vec2 pos) {
  const Vec2 start = shell.input().pointerStartPosition();
  const float dx = pos.x - start.x;
  const float dy = pos.y - start.y;
  if (std::abs(dx) > 32.0f || std::abs(dy) > 32.0f) {
    if (std::abs(dx) > std::abs(dy)) {
      shell.setSwipe(dx < 0 ? input::SwipeDirection::Left : input::SwipeDirection::Right);
    } else {
      shell.setSwipe(dy < 0 ? input::SwipeDirection::Up : input::SwipeDirection::Down);
    }
  }
}

void consumeEvent(SDLState& sdl, runtime::RuntimeShell& shell, const SDL_Event& event, bool& running) {
  switch (event.type) {
  case SDL_QUIT:
    running = false;
    break;
  case SDL_APP_WILLENTERBACKGROUND:
  case SDL_APP_DIDENTERBACKGROUND:
    setLifecyclePaused(sdl, shell, true);
    break;
  case SDL_APP_DIDENTERFOREGROUND:
    setLifecyclePaused(sdl, shell, false);
    break;
  case SDL_WINDOWEVENT:
    if (event.window.event == SDL_WINDOWEVENT_MINIMIZED || event.window.event == SDL_WINDOWEVENT_HIDDEN || event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
      setLifecyclePaused(sdl, shell, true);
    } else if (event.window.event == SDL_WINDOWEVENT_RESTORED ||
               event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED ||
               event.window.event == SDL_WINDOWEVENT_SHOWN) {
      setLifecyclePaused(sdl, shell, false);
    }
    break;
  case SDL_KEYDOWN:
  case SDL_KEYUP: {
    const bool down=event.type==SDL_KEYDOWN;
    const auto key=event.key.keysym.sym;
    if(down && event.key.repeat) break;
    if(key==SDLK_ESCAPE || key==SDLK_AC_BACK) {
      if(down) { sdl.userPaused=!sdl.userPaused; clearControls(sdl,shell); updatePause(sdl,shell); }
      break;
    }
    if(sdl.userPaused && down) {
      if(key==SDLK_RETURN || key==SDLK_SPACE) { sdl.userPaused=false; updatePause(sdl,shell); }
      if(key==SDLK_BACKSPACE && shell.sceneDepth()>1) { shell.host().navigation().pop(); sdl.userPaused=false; updatePause(sdl,shell); }
      if(key==SDLK_q) running=false;
      break;
    }
    if(down) sdl.keys.insert(key); else sdl.keys.erase(key);
    refreshControls(sdl,shell);
    break;
  }
  case SDL_CONTROLLERDEVICEADDED:
    if(!sdl.controller && SDL_IsGameController(event.cdevice.which)) sdl.controller=SDL_GameControllerOpen(event.cdevice.which);
    break;
  case SDL_CONTROLLERDEVICEREMOVED:
    if(sdl.controller && event.cdevice.which==SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(sdl.controller))) {
      SDL_GameControllerClose(sdl.controller); sdl.controller=nullptr;
      clearControls(sdl,shell); sdl.userPaused=true; updatePause(sdl,shell);
    }
    break;
  case SDL_CONTROLLERAXISMOTION: {
    float value=event.caxis.value/32767.0f;
    value=std::abs(value)<0.18f?0.0f:std::copysign((std::abs(value)-0.18f)/0.82f,value);
    if(event.caxis.axis==SDL_CONTROLLER_AXIS_LEFTX) sdl.controllerX=value;
    if(event.caxis.axis==SDL_CONTROLLER_AXIS_LEFTY) sdl.controllerY=value;
    refreshControls(sdl,shell); break;
  }
  case SDL_CONTROLLERBUTTONDOWN:
  case SDL_CONTROLLERBUTTONUP: {
    const bool down=event.type==SDL_CONTROLLERBUTTONDOWN;
    const auto button=event.cbutton.button;
    if(button==SDL_CONTROLLER_BUTTON_START) {
      if(down) { sdl.userPaused=!sdl.userPaused; clearControls(sdl,shell); updatePause(sdl,shell); }
      break;
    }
    if(sdl.userPaused && down) {
      if(button==SDL_CONTROLLER_BUTTON_A) { sdl.userPaused=false; updatePause(sdl,shell); }
      if(button==SDL_CONTROLLER_BUTTON_B && shell.sceneDepth()>1) { shell.host().navigation().pop(); sdl.userPaused=false; updatePause(sdl,shell); }
      if(button==SDL_CONTROLLER_BUTTON_Y) running=false;
      break;
    }
    if(down) sdl.controllerButtons.insert(button); else sdl.controllerButtons.erase(button);
    refreshControls(sdl,shell); break;
  }
  case SDL_MOUSEBUTTONDOWN:
    if (event.button.which == SDL_TOUCH_MOUSEID) {
      break;
    }
    sdl.mouseDown=true; refreshControls(sdl,shell);
    shell.setPointerDown(true, Vec2{static_cast<float>(event.button.x), static_cast<float>(event.button.y)});
    break;
  case SDL_MOUSEBUTTONUP: {
    if (event.button.which == SDL_TOUCH_MOUSEID) {
      break;
    }
    sdl.mouseDown=false; refreshControls(sdl,shell);
    const Vec2 pos{static_cast<float>(event.button.x), static_cast<float>(event.button.y)};
    if(sdl.userPaused) {
      if(pos.y>=150 && pos.y<192) { sdl.userPaused=false; updatePause(sdl,shell); }
      else if(pos.y>=192 && pos.y<234 && shell.sceneDepth()>1) { shell.host().navigation().pop(); sdl.userPaused=false; updatePause(sdl,shell); }
      else if(pos.y>=234 && pos.y<276) running=false;
      shell.input().clear(); break;
    }
    shell.setPointerDown(false, pos);
    setSwipeFromPointerRelease(shell, pos);
    break;
  }
  case SDL_MOUSEMOTION:
    if (event.motion.which == SDL_TOUCH_MOUSEID) {
      break;
    }
    shell.setPointerPosition(Vec2{static_cast<float>(event.motion.x), static_cast<float>(event.motion.y)});
    break;
  case SDL_FINGERDOWN: {
    const Vec2 pos=normalizedTouchToLogical(sdl,event.tfinger.x,event.tfinger.y);
    const auto& state=shell.host().state();
    const auto phaseId=shell.host().vm().interner().intern(":phase");
    const auto playId=shell.host().vm().interner().intern(":play");
    const bool skiing=shell.host().vm().interner().resolve(shell.host().definition().id)==":alpine-rush" &&
      state.kind==script::ValueKind::Map && state.map->contains(phaseId) &&
      state.map->at(phaseId).kind==script::ValueKind::Keyword && state.map->at(phaseId).id==playId;
    if(skiing && !sdl.userPaused && pos.y>90) {
      if(pos.x<320 && !sdl.steeringFinger) { sdl.steeringFinger=event.tfinger.fingerId; sdl.steeringOrigin=pos; }
      else if(!sdl.activeFinger) { sdl.activeFinger=event.tfinger.fingerId; sdl.touchJump=true; }
      refreshControls(sdl,shell); break;
    }
    if(sdl.activeFinger) break;
    sdl.activeFinger=event.tfinger.fingerId;
    shell.setPointerDown(true,pos); refreshControls(sdl,shell); break;
  }
  case SDL_FINGERUP: {
    const Vec2 pos=normalizedTouchToLogical(sdl,event.tfinger.x,event.tfinger.y);
    if(sdl.steeringFinger==event.tfinger.fingerId) {
      sdl.steeringFinger.reset(); sdl.touchX=sdl.touchY=0; refreshControls(sdl,shell); break;
    }
    if(sdl.activeFinger!=event.tfinger.fingerId) break;
    sdl.activeFinger.reset(); sdl.touchJump=false;
    if(sdl.userPaused) {
      if(pos.y>=150 && pos.y<192) { sdl.userPaused=false; updatePause(sdl,shell); }
      else if(pos.y>=192 && pos.y<234 && shell.sceneDepth()>1) { shell.host().navigation().pop(); sdl.userPaused=false; updatePause(sdl,shell); }
      else if(pos.y>=234 && pos.y<276) running=false;
      shell.input().clear(); break;
    }
    shell.setPointerDown(false,pos); setSwipeFromPointerRelease(shell,pos); refreshControls(sdl,shell); break;
  }
  case SDL_FINGERMOTION: {
    const Vec2 pos=normalizedTouchToLogical(sdl,event.tfinger.x,event.tfinger.y);
    if(sdl.steeringFinger==event.tfinger.fingerId) {
      sdl.touchX=std::clamp((pos.x-sdl.steeringOrigin.x)/50.0f,-1.0f,1.0f);
      sdl.touchY=std::clamp((pos.y-sdl.steeringOrigin.y)/65.0f,-1.0f,1.0f);
      refreshControls(sdl,shell);
    } else if(sdl.activeFinger==event.tfinger.fingerId) shell.setPointerPosition(pos);
    break;
  }
  default:
    break;
  }
}

void cleanup(SDLState& sdl) {
  if(sdl.controller) SDL_GameControllerClose(sdl.controller);
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

int runSDLAppWithShell(runtime::RuntimeShell shell, int maxFrames, const SDLAppOptions& options) {
  SDL_SetMainReady();
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) != 0) {
    std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
    return 1;
  }

  SDLState sdl;
  if(const char* overridePath=std::getenv("GLYPH_PROFILE_PATH")) {
    shell.setProfile(std::make_shared<profile::ProfileStore>(overridePath));
  } else if(char* pref=SDL_GetPrefPath("Glyph","Arcade")) {
    shell.setProfile(std::make_shared<profile::ProfileStore>(std::filesystem::path(pref)/"profile.glyphdata"));
    SDL_free(pref);
  }
  for(int i=0;i<SDL_NumJoysticks();++i) if(SDL_IsGameController(i)) { sdl.controller=SDL_GameControllerOpen(i); break; }
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

  const auto& initialHost = shell.host();
  const int width = static_cast<int>(initialHost.definition().logicalSize.x);
  const int height = static_cast<int>(initialHost.definition().logicalSize.y);
  Uint32 windowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI;
  if (options.resizableWindow) {
    windowFlags |= SDL_WINDOW_RESIZABLE;
  }
  if (options.fullscreen) {
    windowFlags |= SDL_WINDOW_FULLSCREEN;
  }
  sdl.window = SDL_CreateWindow(initialHost.definition().title.c_str(), SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED, width, height, windowFlags);
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
    sdl.nativeAudioPaused = false;
#if GLYPH_HAS_SDL_MIXER
  } else if (sdl.mixerOpen) {
    sdl.audioSpec = desired;
    sdl.nativeAudioPaused = false;
#endif
  } else {
    std::cerr << "warning: audio disabled: " << SDL_GetError() << '\n';
    sdl.audioSpec = desired;
  }

  loadSDLAssets(sdl, shell);
  std::string reloadPath = shell.currentScenePath();
  std::filesystem::path reloadFile = shell.currentSceneFile();
  std::optional<std::filesystem::file_time_type> lastWriteTime =
      shell.assetSource().lastWriteTime(reloadPath);
  double reloadPollSeconds = 0.0;

  bool running = true;
  Uint64 last = SDL_GetPerformanceCounter();
  const double frequency = static_cast<double>(SDL_GetPerformanceFrequency());
  int frames = 0;

  while (running) {
    const Uint64 now = SDL_GetPerformanceCounter();
    const double dt = static_cast<double>(now - last) / frequency;
    last = now;

    shell.beginFrame();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      consumeEvent(sdl, shell, event, running);
    }

    if (sdl.backgrounded) {
      shell.endFrame();
      SDL_Delay(50);
      continue;
    }

    if (options.hotReload) {
      reloadPollSeconds += dt;
    }
    if (options.hotReload && reloadPollSeconds >= 0.25) {
      reloadPollSeconds = 0.0;
      if (auto writeTime = shell.assetSource().lastWriteTime(reloadPath)) {
        if (writeTime != lastWriteTime) {
          lastWriteTime = writeTime;
          try {
            const auto source = shell.assetSource().readText(reloadPath);
            if (!source) {
              throw script::RuntimeError("unable to open " + reloadPath);
            }
            const auto result = shell.host().reloadSourcePreservingState(*source, reloadPath);
            if (result.success) {
              shell.clearStatusError();
              clearLoadedAssets(sdl);
              loadSDLAssets(sdl, shell);
              SDL_SetWindowTitle(sdl.window, shell.host().definition().title.c_str());
              SDL_RenderSetLogicalSize(sdl.renderer, static_cast<int>(shell.host().definition().logicalSize.x),
                                       static_cast<int>(shell.host().definition().logicalSize.y));
              std::cerr << "reloaded " << reloadFile.string() << '\n';
            } else {
              shell.setStatusError(result.error);
              std::cerr << "reload failed: " << shell.statusError() << '\n';
            }
          } catch (const script::ScriptError& error) {
            shell.setStatusError(error.what());
            std::cerr << "reload failed: " << shell.statusError() << '\n';
          }
        }
      }
    }

    if(sdl.userPaused) shell.input().clear();
    shell.tick(std::min(dt, 0.25));
    processAudio(sdl, shell.host());
    try {
      const std::string previousStatusError = shell.statusError();
      if (shell.processNavigation()) {
        clearControls(sdl,shell);
        activateScene(sdl, shell);
        reloadPath = shell.currentScenePath();
        reloadFile = shell.currentSceneFile();
        lastWriteTime = shell.assetSource().lastWriteTime(reloadPath);
        std::cerr << "activated scene " << reloadFile.string() << '\n';
      } else if (!shell.statusError().empty() && shell.statusError() != previousStatusError) {
        std::cerr << "navigation failed: " << shell.statusError() << '\n';
      }
    } catch (const script::ScriptError& error) {
      shell.setStatusError(error.what());
      std::cerr << "navigation failed: " << shell.statusError() << '\n';
    }
    const auto commands = shell.renderView();
    renderCommands(sdl, shell.host().vm().interner(), commands);
    drawReloadErrorOverlay(sdl.renderer, static_cast<int>(shell.host().definition().logicalSize.x),
                           shell.statusError());
    if(sdl.userPaused) {
      SDL_SetRenderDrawColor(sdl.renderer,9,18,31,230);
      SDL_FRect panel{75,75,330,224}; SDL_RenderFillRectF(sdl.renderer,&panel);
      const auto label=[&](const char* value,float x,float y,int size){
        render::DrawCommand cmd{}; cmd.type=render::DrawCommandType::Text; cmd.font=shell.host().vm().interner().intern(":main");
        cmd.text=value; cmd.x=x; cmd.y=y; cmd.scale=size; cmd.color="#f3f4ef";
        renderCommands(sdl,shell.host().vm().interner(),{cmd});
      };
      label("PAUSED",170,99,24);
      label("Resume   /   A or Enter",111,159,16);
      label("Arcade   /   B or Backspace",111,201,16);
      label("Quit       /   Y or Q",111,243,16);
    }
    if(!shell.profile().error().empty()) drawReloadErrorOverlay(sdl.renderer,width,shell.profile().error());
    SDL_RenderPresent(sdl.renderer);
    shell.endFrame();

    ++frames;
    if (maxFrames >= 0 && frames >= maxFrames) {
      running = false;
    }
  }

  cleanup(sdl);
  return 0;
}

int runSDLApp(const std::string& gameFileString, int maxFrames, const SDLAppOptions& options) {
  return runSDLAppWithShell(runtime::RuntimeShell(std::filesystem::path(gameFileString)), maxFrames, options);
}

int runSDLApp(std::shared_ptr<assets::IAssetSource> assetSource, const std::string& entryPath, int maxFrames,
              const SDLAppOptions& options) {
  return runSDLAppWithShell(runtime::RuntimeShell(std::move(assetSource), entryPath), maxFrames, options);
}

std::string bundledDemoPath() {
  std::filesystem::path base;
  if(char* value=SDL_GetBasePath()) { base=value; SDL_free(value); }
  for(const auto& path : {base/"arcade/demo.glyph", base/"../Resources/arcade/demo.glyph", std::filesystem::path("examples/arcade/demo.glyph")}) {
    if(std::filesystem::exists(path)) return path.string();
  }
  throw script::RuntimeError("Demo assets missing. Install the arcade directory beside the executable.");
}

int runSDLDesktop(const std::string& gameFile, int maxFrames) {
  SDLAppOptions options;
  options.hotReload = true;
  options.resizableWindow = true;
  return runSDLApp(gameFile, maxFrames, options);
}

} // namespace glyph::platform
