#include "game/GameHost.h"
#include "render/DrawCommand.h"
#include "script/Error.h"
#include "script/VM.h"

#if GLYPH_HAS_SDL
#include "platform/SDLDesktopApp.h"
#endif

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

std::string readFile(const std::string& file) {
  std::ifstream input(file);
  if (!input) {
    throw glyph::script::RuntimeError("unable to open " + file);
  }

  std::ostringstream source;
  source << input.rdbuf();
  return source.str();
}

std::string commandName(glyph::render::DrawCommandType type) {
  switch (type) {
  case glyph::render::DrawCommandType::Clear:
    return "clear";
  case glyph::render::DrawCommandType::Rect:
    return "rect";
  case glyph::render::DrawCommandType::Circle:
    return "circle";
  case glyph::render::DrawCommandType::Line:
    return "line";
  case glyph::render::DrawCommandType::Sprite:
    return "sprite";
  case glyph::render::DrawCommandType::Text:
    return "text";
  case glyph::render::DrawCommandType::PushTransform:
    return "push-transform";
  case glyph::render::DrawCommandType::PopTransform:
    return "pop-transform";
  case glyph::render::DrawCommandType::PushCamera:
    return "push-camera";
  case glyph::render::DrawCommandType::PopCamera:
    return "pop-camera";
  }
  return "unknown";
}

std::string audioCommandName(glyph::audio::AudioCommandType type) {
  switch (type) {
  case glyph::audio::AudioCommandType::PlaySound:
    return "sound/play";
  case glyph::audio::AudioCommandType::PlayMusic:
    return "music/play";
  case glyph::audio::AudioCommandType::StopMusic:
    return "music/stop";
  case glyph::audio::AudioCommandType::SetMusicVolume:
    return "music/set-volume";
  }
  return "unknown";
}

int runGame(int argc, char** argv) {
  if (argc < 3) {
    std::cerr << "usage: glyph --run path/to/game.glyph [--frames N]\n";
    return 1;
  }

  const std::string file = argv[2];
  int frames = 1;
  for (int i = 3; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--frames" && i + 1 < argc) {
      frames = std::max(0, std::atoi(argv[++i]));
    } else {
      std::cerr << "unknown option: " << arg << '\n';
      return 1;
    }
  }

  glyph::game::GameHost host;
  host.loadSource(readFile(file), file);

  for (int frame = 0; frame < frames; ++frame) {
    host.input().beginFrame();
    host.tick(glyph::game::GameHost::fixedDt);
    host.input().endFrame();
  }

  const auto commands = host.renderView();

  std::cout << "game: " << host.definition().title << '\n';
  std::cout << "size: " << host.definition().logicalSize.x << "x" << host.definition().logicalSize.y
            << '\n';
  std::cout << "frames: " << frames << '\n';
  std::cout << "state: " << glyph::script::valueToString(host.state(), host.vm().interner()) << '\n';
  std::cout << "assets: " << host.assets().assets().size() << '\n';
  std::cout << "draw-commands: " << commands.size() << '\n';
  for (const auto& command : commands) {
    std::cout << "  - " << commandName(command.type);
    if (command.asset.id != 0) {
      std::cout << " asset#" << command.asset.id;
    }
    if (!command.text.empty()) {
      std::cout << " \"" << command.text << "\"";
    }
    std::cout << '\n';
  }

  std::cout << "audio-commands: " << host.audio().commands().size() << '\n';
  for (const auto& command : host.audio().commands()) {
    std::cout << "  - " << audioCommandName(command.type);
    if (command.handle.id != 0) {
      std::cout << " asset#" << command.handle.id;
    }
    std::cout << " volume=" << command.volume << " pitch=" << command.pitch << '\n';
  }

  return 0;
}

} // namespace

int main(int argc, char** argv) {
  try {
    if (argc > 1 && std::string(argv[1]) == "--run") {
      return runGame(argc, argv);
    }
    if (argc > 1 && std::string(argv[1]) == "--desktop") {
      if (argc < 3) {
        std::cerr << "usage: glyph --desktop path/to/game.glyph [--frames N]\n";
        return 1;
      }
      int frames = -1;
      for (int i = 3; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--frames" && i + 1 < argc) {
          frames = std::max(0, std::atoi(argv[++i]));
        } else {
          std::cerr << "unknown option: " << arg << '\n';
          return 1;
        }
      }
#if GLYPH_HAS_SDL
      return glyph::platform::runSDLDesktop(argv[2], frames);
#else
      std::cerr << "desktop target was not built because SDL2 was not found\n";
      return 1;
#endif
    }
  } catch (const glyph::script::ScriptError& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }

  std::ostringstream source;
  std::string file = "<stdin>";

  if (argc > 1) {
    file = argv[1];
    source << readFile(file);
  } else {
    source << std::cin.rdbuf();
  }

  try {
    glyph::script::VM vm;
    const auto value = vm.evalSource(source.str(), file);
    std::cout << glyph::script::valueToString(value, vm.interner()) << '\n';
  } catch (const glyph::script::ScriptError& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }

  return 0;
}
