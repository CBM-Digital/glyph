#pragma once

#include "assets/AssetManager.h"
#include "core/Types.h"

#include <vector>

namespace glyph::audio {

enum class AudioCommandType {
  PlaySound,
  PlayMusic,
  StopMusic,
  SetMusicVolume
};

struct AudioCommand {
  AudioCommandType type = AudioCommandType::PlaySound;
  StringId asset = 0;
  assets::AssetHandle handle {};
  float volume = 1.0f;
  float pitch = 1.0f;
};

class AudioSystem {
public:
  void setAssetManager(const assets::AssetManager* assets);

  void enqueue(const AudioCommand& command);
  void flush();
  void pauseAll();
  void resumeAll();

  bool paused() const;
  const std::vector<AudioCommand>& commands() const;

private:
  const assets::AssetManager* assets_ = nullptr;
  std::vector<AudioCommand> commands_;
  bool paused_ = false;
};

} // namespace glyph::audio
