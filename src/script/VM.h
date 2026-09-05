#pragma once

#include "core/StringInterner.h"
#include "script/Env.h"
#include "script/Value.h"

#include <memory>
#include <random>
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace glyph::input {
class InputSystem;
}

namespace glyph::audio {
class AudioSystem;
}

namespace glyph::game {
class NavigationSystem;
}

namespace glyph::profile { class ProfileStore; }

namespace glyph::script {

class VM {
public:
  VM();
  ~VM();
  VM(VM&&) noexcept = default;
  VM& operator=(VM&&) noexcept;
  VM(const VM&) = delete;
  VM& operator=(const VM&) = delete;
  void setProfile(profile::ProfileStore* value) { profile_ = value; }
  profile::ProfileStore* profile() const { return profile_; }
  void seedRandom(std::uint32_t seed);
  double randomUnit();
  std::uint32_t randomIndex(std::uint32_t bound);
  std::mt19937 randomState() const { return random_; }
  void restoreRandom(const std::mt19937& state) { random_ = state; }

  Value evalSource(std::string_view source, std::string file = "<input>");
  Value evalProgram(const std::vector<AstPtr>& program);
  Value eval(const AstPtr& node, const std::shared_ptr<Env>& env);
  Value call(Value callee, const std::vector<Value>& args);

  void defineGlobal(std::string_view name, Value value);
  void defineGlobal(StringId name, Value value);

  StringInterner& interner();
  const StringInterner& interner() const;
  std::shared_ptr<Env> globals() const;
  void setInputSystem(const glyph::input::InputSystem* input);
  const glyph::input::InputSystem* input() const;
  void setAudioSystem(glyph::audio::AudioSystem* audio);
  glyph::audio::AudioSystem* audio() const;
  void setNavigationSystem(glyph::game::NavigationSystem* navigation);
  glyph::game::NavigationSystem* navigation() const;

private:
  Value evalList(const AstPtr& node, const std::shared_ptr<Env>& env);
  Value evalDef(const AstPtr& node, const std::shared_ptr<Env>& env);
  Value evalDefn(const AstPtr& node, const std::shared_ptr<Env>& env);
  Value evalFn(const AstPtr& node, const std::shared_ptr<Env>& env);
  Value evalLet(const AstPtr& node, const std::shared_ptr<Env>& env);
  Value evalIf(const AstPtr& node, const std::shared_ptr<Env>& env);
  Value evalCond(const AstPtr& node, const std::shared_ptr<Env>& env);
  Value evalCase(const AstPtr& node, const std::shared_ptr<Env>& env);
  Value evalDo(const AstPtr& node, const std::shared_ptr<Env>& env);
  Value evalFor(const AstPtr& node, const std::shared_ptr<Env>& env);
  Value evalAnd(const AstPtr& node, const std::shared_ptr<Env>& env);
  Value evalOr(const AstPtr& node, const std::shared_ptr<Env>& env);
  Value evalGame(const AstPtr& node, const std::shared_ptr<Env>& env);

  std::vector<StringId> readParamList(const AstPtr& node, std::string_view formName);
  bool isSymbolNamed(const AstPtr& node, std::string_view name) const;
  std::string symbolName(const AstPtr& node) const;

  profile::ProfileStore* profile_ = nullptr;
  std::mt19937 random_;
  StringInterner interner_;
  std::shared_ptr<Env> globals_;
  std::unordered_map<StringId, Value> nativeFallbacks_;
  const glyph::input::InputSystem* input_ = nullptr;
  glyph::audio::AudioSystem* audio_ = nullptr;
  glyph::game::NavigationSystem* navigation_ = nullptr;
};

} // namespace glyph::script
