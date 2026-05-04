#pragma once

#include "core/StringInterner.h"
#include "script/Env.h"
#include "script/Value.h"

#include <memory>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace glyph::input {
class InputSystem;
}

namespace glyph::audio {
class AudioSystem;
}

namespace glyph::script {

class VM {
public:
  VM();

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

  StringInterner interner_;
  std::shared_ptr<Env> globals_;
  std::unordered_map<StringId, Value> nativeFallbacks_;
  const glyph::input::InputSystem* input_ = nullptr;
  glyph::audio::AudioSystem* audio_ = nullptr;
};

} // namespace glyph::script
