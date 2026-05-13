#pragma once

#include <string>

namespace glyph::platform {

struct SDLAppOptions {
  bool hotReload = false;
  bool resizableWindow = false;
  bool fullscreen = false;
};

int runSDLApp(const std::string& gameFile, int maxFrames = -1, const SDLAppOptions& options = {});
int runSDLDesktop(const std::string& gameFile, int maxFrames = -1);

} // namespace glyph::platform
