#include "script/VM.h"

#include "script/Error.h"
#include "script/Lexer.h"
#include "script/Native.h"
#include "script/Parser.h"

#include <sstream>

namespace glyph::script {

VM::VM() : globals_(std::make_shared<Env>()) { registerCoreNatives(*this); }

Value VM::evalSource(std::string_view source, std::string file) {
  Lexer lexer(source);
  auto tokens = lexer.lex();
  Parser parser(tokens, interner_, std::move(file));
  return evalProgram(parser.parseProgram());
}

Value VM::evalProgram(const std::vector<AstPtr>& program) {
  Value result = Value::nil();
  for (const auto& expr : program) {
    result = eval(expr, globals_);
  }
  return result;
}

Value VM::eval(const AstPtr& node, const std::shared_ptr<Env>& env) {
  switch (node->kind) {
  case AstKind::Nil:
    return Value::nil();
  case AstKind::Bool:
    return Value::booleanValue(node->boolean);
  case AstKind::Number:
    return Value::numberValue(node->number);
  case AstKind::String:
    return Value::stringValue(node->text);
  case AstKind::Keyword:
    return Value::keywordValue(node->id);
  case AstKind::Symbol: {
    auto value = env->lookup(node->id);
    if (!value) {
      throw RuntimeError("undefined symbol: " + std::string(interner_.resolve(node->id)));
    }
    return *value;
  }
  case AstKind::Vector: {
    std::vector<Value> values;
    values.reserve(node->children.size());
    for (const auto& child : node->children) {
      values.push_back(eval(child, env));
    }
    return Value::vectorValue(std::move(values));
  }
  case AstKind::Map: {
    std::map<StringId, Value> values;
    for (const auto& [keyNode, valueNode] : node->entries) {
      const Value key = eval(keyNode, env);
      if (key.kind != ValueKind::Keyword) {
        throw RuntimeError("map keys must be keywords in v0.1");
      }
      values[key.id] = eval(valueNode, env);
    }
    return Value::mapValue(std::move(values));
  }
  case AstKind::List:
    return evalList(node, env);
  }

  return Value::nil();
}

void VM::defineGlobal(std::string_view name, Value value) {
  globals_->define(interner_.intern(name), std::move(value));
}

void VM::defineGlobal(StringId name, Value value) { globals_->define(name, std::move(value)); }

StringInterner& VM::interner() { return interner_; }

const StringInterner& VM::interner() const { return interner_; }

std::shared_ptr<Env> VM::globals() const { return globals_; }

Value VM::evalList(const AstPtr& node, const std::shared_ptr<Env>& env) {
  if (node->children.empty()) {
    return Value::nil();
  }

  const AstPtr& head = node->children[0];
  if (head->kind == AstKind::Symbol) {
    const std::string name = symbolName(head);
    if (name == "def") {
      return evalDef(node, env);
    }
    if (name == "defn") {
      return evalDefn(node, env);
    }
    if (name == "fn") {
      return evalFn(node, env);
    }
    if (name == "let") {
      return evalLet(node, env);
    }
    if (name == "if") {
      return evalIf(node, env);
    }
    if (name == "cond") {
      return evalCond(node, env);
    }
    if (name == "case") {
      return evalCase(node, env);
    }
    if (name == "do") {
      return evalDo(node, env);
    }
    if (name == "for") {
      return evalFor(node, env);
    }
    if (name == "and") {
      return evalAnd(node, env);
    }
    if (name == "or") {
      return evalOr(node, env);
    }
  }

  Value callee = eval(head, env);
  std::vector<Value> args;
  args.reserve(node->children.size() - 1);
  for (std::size_t i = 1; i < node->children.size(); ++i) {
    args.push_back(eval(node->children[i], env));
  }
  return call(std::move(callee), args);
}

Value VM::evalDef(const AstPtr& node, const std::shared_ptr<Env>& env) {
  if (node->children.size() != 3 || node->children[1]->kind != AstKind::Symbol) {
    throw RuntimeError("def expects a symbol and a value");
  }

  Value value = eval(node->children[2], env);
  env->define(node->children[1]->id, value);
  return value;
}

Value VM::evalDefn(const AstPtr& node, const std::shared_ptr<Env>& env) {
  if (node->children.size() < 4 || node->children[1]->kind != AstKind::Symbol) {
    throw RuntimeError("defn expects a name, parameter vector, and body");
  }

  auto params = readParamList(node->children[2], "defn");
  std::vector<AstPtr> body(node->children.begin() + 3, node->children.end());
  Value function = Value::functionValue(std::move(params), std::move(body), env);
  env->define(node->children[1]->id, function);
  return function;
}

Value VM::evalFn(const AstPtr& node, const std::shared_ptr<Env>& env) {
  if (node->children.size() < 3) {
    throw RuntimeError("fn expects a parameter vector and body");
  }

  auto params = readParamList(node->children[1], "fn");
  std::vector<AstPtr> body(node->children.begin() + 2, node->children.end());
  return Value::functionValue(std::move(params), std::move(body), env);
}

Value VM::evalLet(const AstPtr& node, const std::shared_ptr<Env>& env) {
  if (node->children.size() < 3 || node->children[1]->kind != AstKind::Vector) {
    throw RuntimeError("let expects a binding vector and body");
  }

  const auto& bindings = node->children[1]->children;
  if (bindings.size() % 2 != 0) {
    throw RuntimeError("let binding vector must contain symbol/value pairs");
  }

  auto local = std::make_shared<Env>(env);
  for (std::size_t i = 0; i < bindings.size(); i += 2) {
    if (bindings[i]->kind != AstKind::Symbol) {
      throw RuntimeError("let binding names must be symbols");
    }
    local->define(bindings[i]->id, eval(bindings[i + 1], local));
  }

  Value result = Value::nil();
  for (std::size_t i = 2; i < node->children.size(); ++i) {
    result = eval(node->children[i], local);
  }
  return result;
}

Value VM::evalIf(const AstPtr& node, const std::shared_ptr<Env>& env) {
  if (node->children.size() < 3 || node->children.size() > 4) {
    throw RuntimeError("if expects condition, then branch, and optional else branch");
  }

  if (isTruthy(eval(node->children[1], env))) {
    return eval(node->children[2], env);
  }
  if (node->children.size() == 4) {
    return eval(node->children[3], env);
  }
  return Value::nil();
}

Value VM::evalCond(const AstPtr& node, const std::shared_ptr<Env>& env) {
  if ((node->children.size() - 1) % 2 != 0) {
    throw RuntimeError("cond expects test/expression pairs");
  }

  for (std::size_t i = 1; i < node->children.size(); i += 2) {
    if (isSymbolNamed(node->children[i], "else") || isTruthy(eval(node->children[i], env))) {
      return eval(node->children[i + 1], env);
    }
  }
  return Value::nil();
}

Value VM::evalCase(const AstPtr& node, const std::shared_ptr<Env>& env) {
  if (node->children.size() < 4 || (node->children.size() - 2) % 2 != 0) {
    throw RuntimeError("case expects a value followed by match/expression pairs");
  }

  const Value target = eval(node->children[1], env);
  for (std::size_t i = 2; i < node->children.size(); i += 2) {
    if (isSymbolNamed(node->children[i], "else")) {
      return eval(node->children[i + 1], env);
    }
    if (valueEquals(target, eval(node->children[i], env))) {
      return eval(node->children[i + 1], env);
    }
  }
  return Value::nil();
}

Value VM::evalDo(const AstPtr& node, const std::shared_ptr<Env>& env) {
  Value result = Value::nil();
  for (std::size_t i = 1; i < node->children.size(); ++i) {
    result = eval(node->children[i], env);
  }
  return result;
}

Value VM::evalFor(const AstPtr& node, const std::shared_ptr<Env>& env) {
  if (node->children.size() < 3 || node->children[1]->kind != AstKind::Vector ||
      node->children[1]->children.size() != 2 ||
      node->children[1]->children[0]->kind != AstKind::Symbol) {
    throw RuntimeError("for expects [name collection] and body");
  }

  const StringId itemName = node->children[1]->children[0]->id;
  const Value collection = eval(node->children[1]->children[1], env);
  if (collection.kind != ValueKind::Vector) {
    throw RuntimeError("for collection must be a vector");
  }

  std::vector<Value> results;
  results.reserve(collection.vector->size());
  for (const auto& item : *collection.vector) {
    auto local = std::make_shared<Env>(env);
    local->define(itemName, item);
    Value result = Value::nil();
    for (std::size_t i = 2; i < node->children.size(); ++i) {
      result = eval(node->children[i], local);
    }
    results.push_back(result);
  }
  return Value::vectorValue(std::move(results));
}

Value VM::evalAnd(const AstPtr& node, const std::shared_ptr<Env>& env) {
  Value result = Value::booleanValue(true);
  for (std::size_t i = 1; i < node->children.size(); ++i) {
    result = eval(node->children[i], env);
    if (!isTruthy(result)) {
      return result;
    }
  }
  return result;
}

Value VM::evalOr(const AstPtr& node, const std::shared_ptr<Env>& env) {
  for (std::size_t i = 1; i < node->children.size(); ++i) {
    Value result = eval(node->children[i], env);
    if (isTruthy(result)) {
      return result;
    }
  }
  return Value::nil();
}

Value VM::call(Value callee, const std::vector<Value>& args) {
  if (callee.kind == ValueKind::Keyword) {
    if (args.empty() || args.size() > 2 || args[0].kind != ValueKind::Map) {
      throw RuntimeError("keyword call expects a map and optional fallback");
    }
    auto found = args[0].map->find(callee.id);
    if (found != args[0].map->end()) {
      return found->second;
    }
    return args.size() == 2 ? args[1] : Value::nil();
  }

  if (callee.kind == ValueKind::NativeFunction) {
    const auto& native = *callee.native;
    if (static_cast<int>(args.size()) < native.minArgs ||
        (native.maxArgs >= 0 && static_cast<int>(args.size()) > native.maxArgs)) {
      throw RuntimeError("native function received the wrong number of arguments");
    }
    return native.fn(*this, args);
  }

  if (callee.kind == ValueKind::Function) {
    const auto& function = *callee.function;
    if (function.params.size() != args.size()) {
      throw RuntimeError("function received the wrong number of arguments");
    }

    auto local = std::make_shared<Env>(function.closure);
    for (std::size_t i = 0; i < args.size(); ++i) {
      local->define(function.params[i], args[i]);
    }

    Value result = Value::nil();
    for (const auto& expr : function.body) {
      result = eval(expr, local);
    }
    return result;
  }

  throw RuntimeError("value is not callable");
}

std::vector<StringId> VM::readParamList(const AstPtr& node, std::string_view formName) {
  if (node->kind != AstKind::Vector) {
    throw RuntimeError(std::string(formName) + " expects a parameter vector");
  }

  std::vector<StringId> params;
  params.reserve(node->children.size());
  for (const auto& param : node->children) {
    if (param->kind != AstKind::Symbol) {
      throw RuntimeError(std::string(formName) + " parameters must be symbols");
    }
    params.push_back(param->id);
  }
  return params;
}

bool VM::isSymbolNamed(const AstPtr& node, std::string_view name) const {
  return node->kind == AstKind::Symbol && interner_.resolve(node->id) == name;
}

std::string VM::symbolName(const AstPtr& node) const {
  return std::string(interner_.resolve(node->id));
}

} // namespace glyph::script
