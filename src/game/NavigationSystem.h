#pragma once

#include <string>
#include <utility>
#include <vector>

namespace glyph::game {

enum class NavigationCommandType {
  Push,
  Pop
};

struct NavigationCommand {
  NavigationCommandType type = NavigationCommandType::Push;
  std::string target;
};

class NavigationSystem {
public:
  void push(std::string target) { commands_.push_back({NavigationCommandType::Push, std::move(target)}); }
  void pop() { commands_.push_back({NavigationCommandType::Pop, ""}); }
  void clear() { commands_.clear(); }

  const std::vector<NavigationCommand>& commands() const { return commands_; }

private:
  std::vector<NavigationCommand> commands_;
};

} // namespace glyph::game
