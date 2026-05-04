#include "audio/AudioSystem.h"

namespace glyph::audio {

void AudioSystem::setAssetManager(const assets::AssetManager* assets) { assets_ = assets; }

void AudioSystem::enqueue(const AudioCommand& command) {
  AudioCommand resolved = command;
  if (assets_ && resolved.asset != 0) {
    switch (resolved.type) {
    case AudioCommandType::PlaySound:
      resolved.handle = assets_->sound(resolved.asset);
      break;
    case AudioCommandType::PlayMusic:
      resolved.handle = assets_->music(resolved.asset);
      break;
    case AudioCommandType::StopMusic:
    case AudioCommandType::SetMusicVolume:
      break;
    }
  }
  commands_.push_back(resolved);
}

void AudioSystem::flush() { commands_.clear(); }

void AudioSystem::pauseAll() { paused_ = true; }

void AudioSystem::resumeAll() { paused_ = false; }

bool AudioSystem::paused() const { return paused_; }

const std::vector<AudioCommand>& AudioSystem::commands() const { return commands_; }

} // namespace glyph::audio
