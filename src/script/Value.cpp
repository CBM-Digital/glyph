#include "script/Value.h"

#include <cmath>
#include <sstream>

namespace glyph::script {

Value Value::nil() { return {}; }

Value Value::booleanValue(bool value) {
  Value out;
  out.kind = ValueKind::Bool;
  out.boolean = value;
  return out;
}

Value Value::numberValue(double value) {
  Value out;
  out.kind = ValueKind::Number;
  out.number = value;
  return out;
}

Value Value::stringValue(std::string value) {
  Value out;
  out.kind = ValueKind::String;
  out.text = std::move(value);
  return out;
}

Value Value::keywordValue(StringId id) {
  Value out;
  out.kind = ValueKind::Keyword;
  out.id = id;
  return out;
}

Value Value::vectorValue(std::vector<Value> values) {
  Value out;
  out.kind = ValueKind::Vector;
  out.vector = std::make_shared<std::vector<Value>>(std::move(values));
  return out;
}

Value Value::mapValue(std::map<StringId, Value> values) {
  Value out;
  out.kind = ValueKind::Map;
  out.map = std::make_shared<std::map<StringId, Value>>(std::move(values));
  return out;
}

Value Value::functionValue(std::vector<StringId> params, std::vector<AstPtr> body,
                           std::shared_ptr<Env> closure) {
  Value out;
  out.kind = ValueKind::Function;
  out.function = std::make_shared<FunctionObject>();
  out.function->params = std::move(params);
  out.function->body = std::move(body);
  out.function->closure = std::move(closure);
  return out;
}

Value Value::nativeValue(StringId name, NativeFn fn, int minArgs, int maxArgs) {
  Value out;
  out.kind = ValueKind::NativeFunction;
  out.native = std::make_shared<NativeFunctionObject>();
  out.native->name = name;
  out.native->fn = std::move(fn);
  out.native->minArgs = minArgs;
  out.native->maxArgs = maxArgs;
  return out;
}

bool isTruthy(const Value& value) {
  return value.kind != ValueKind::Nil && !(value.kind == ValueKind::Bool && !value.boolean);
}

bool valueEquals(const Value& lhs, const Value& rhs) {
  if (lhs.kind != rhs.kind) {
    return false;
  }

  switch (lhs.kind) {
  case ValueKind::Nil:
    return true;
  case ValueKind::Bool:
    return lhs.boolean == rhs.boolean;
  case ValueKind::Number:
    return std::fabs(lhs.number - rhs.number) < 0.0000001;
  case ValueKind::String:
    return lhs.text == rhs.text;
  case ValueKind::Keyword:
    return lhs.id == rhs.id;
  case ValueKind::Vector:
    if (lhs.vector->size() != rhs.vector->size()) {
      return false;
    }
    for (std::size_t i = 0; i < lhs.vector->size(); ++i) {
      if (!valueEquals((*lhs.vector)[i], (*rhs.vector)[i])) {
        return false;
      }
    }
    return true;
  case ValueKind::Map:
    if (lhs.map->size() != rhs.map->size()) {
      return false;
    }
    for (const auto& [key, value] : *lhs.map) {
      auto found = rhs.map->find(key);
      if (found == rhs.map->end() || !valueEquals(value, found->second)) {
        return false;
      }
    }
    return true;
  case ValueKind::Function:
  case ValueKind::NativeFunction:
    return false;
  }

  return false;
}

std::string valueToString(const Value& value, const StringInterner& interner) {
  std::ostringstream out;

  switch (value.kind) {
  case ValueKind::Nil:
    return "nil";
  case ValueKind::Bool:
    return value.boolean ? "true" : "false";
  case ValueKind::Number:
    out << value.number;
    return out.str();
  case ValueKind::String:
    out << '"' << value.text << '"';
    return out.str();
  case ValueKind::Keyword:
    return std::string(interner.resolve(value.id));
  case ValueKind::Vector:
    out << '[';
    for (std::size_t i = 0; i < value.vector->size(); ++i) {
      if (i > 0) {
        out << ' ';
      }
      out << valueToString((*value.vector)[i], interner);
    }
    out << ']';
    return out.str();
  case ValueKind::Map: {
    out << '{';
    bool first = true;
    for (const auto& [key, item] : *value.map) {
      if (!first) {
        out << ' ';
      }
      first = false;
      out << interner.resolve(key) << ' ' << valueToString(item, interner);
    }
    out << '}';
    return out.str();
  }
  case ValueKind::Function:
    return "<function>";
  case ValueKind::NativeFunction:
    return "<native>";
  }

  return "<unknown>";
}

} // namespace glyph::script
