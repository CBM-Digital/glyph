#pragma once

#include "core/StringInterner.h"
#include "script/Ast.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace glyph::script {

class VM;
struct Env;
struct Value;

using NativeFn = std::function<Value(VM&, const std::vector<Value>&)>;

enum class ValueKind {
  Nil,
  Bool,
  Number,
  String,
  Keyword,
  Vector,
  Map,
  Function,
  NativeFunction
};

struct FunctionObject {
  std::vector<StringId> params;
  std::vector<AstPtr> body;
  std::shared_ptr<Env> closure;
};

struct NativeFunctionObject {
  StringId name = 0;
  NativeFn fn;
  int minArgs = 0;
  int maxArgs = -1;
};

struct Value {
  ValueKind kind = ValueKind::Nil;

  bool boolean = false;
  double number = 0.0;
  StringId id = 0;
  std::string text;

  std::shared_ptr<std::vector<Value>> vector;
  std::shared_ptr<std::map<StringId, Value>> map;
  std::shared_ptr<FunctionObject> function;
  std::shared_ptr<NativeFunctionObject> native;

  static Value nil();
  static Value booleanValue(bool value);
  static Value numberValue(double value);
  static Value stringValue(std::string value);
  static Value keywordValue(StringId id);
  static Value vectorValue(std::vector<Value> values);
  static Value mapValue(std::map<StringId, Value> values);
  static Value functionValue(std::vector<StringId> params, std::vector<AstPtr> body,
                             std::shared_ptr<Env> closure);
  static Value nativeValue(StringId name, NativeFn fn, int minArgs = 0, int maxArgs = -1);
};

bool isTruthy(const Value& value);
bool valueEquals(const Value& lhs, const Value& rhs);
std::string valueToString(const Value& value, const StringInterner& interner);

} // namespace glyph::script
