#pragma once

#include "assets/AssetSource.h"

#include <memory>
#include <string>

namespace glyph::platform {

struct SDLAppOptions {
  bool hotReload = false;
  bool resizableWindow = false;
  bool fullscreen = false;
};

int runSDLApp(const std::string& gameFile, int maxFrames = -1, const SDLAppOptions& options = {});
int runSDLApp(std::shared_ptr<assets::IAssetSource> assetSource, const std::string& entryPath,
              int maxFrames = -1, const SDLAppOptions& options = {});
std::string bundledDemoPath();

int runSDLDesktop(const std::string& gameFile, int maxFrames = -1);

} // namespace glyph::platform
