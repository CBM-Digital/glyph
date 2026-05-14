#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_main.h>

#include "platform/SDLDesktopApp.h"
#include "script/Error.h"

#include <filesystem>
#include <iostream>
#include <string>

namespace {

std::filesystem::path bundledArcadeEntry() {
  std::filesystem::path basePath;
  char* base = SDL_GetBasePath();
  if (base) {
    basePath = base;
    SDL_free(base);
  }
  const auto iOSResourcePath = basePath / "examples" / "arcade" / "index.glyph";
  if (std::filesystem::exists(iOSResourcePath)) {
    return iOSResourcePath;
  }
  return basePath / "Resources" / "examples" / "arcade" / "index.glyph";
}

int runGlyphIOS() {
  SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
  SDL_SetMainReady();

  glyph::platform::SDLAppOptions options;
  options.fullscreen = true;

  const std::filesystem::path entry = bundledArcadeEntry();
  return glyph::platform::runSDLApp(entry.string(), -1, options);
}

} // namespace

extern "C" int SDL_main(int, char**) {
  try {
    return runGlyphIOS();
  } catch (const glyph::script::ScriptError& error) {
    std::cerr << error.what() << '\n';
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
  }
  return 1;
}

int main(int argc, char** argv) {
  return SDL_UIKitRunApp(argc, argv, SDL_main);
}
