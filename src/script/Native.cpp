#include "script/Native.h"

#include "script/Error.h"
#include "input/InputSystem.h"
#include "script/VM.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <random>
#include <sstream>
#include <string>

namespace glyph::script {
namespace {

constexpr double pi = 3.14159265358979323846;

double numberArg(const Value& value) {
  if (value.kind != ValueKind::Number) {
    throw RuntimeError("expected number");
  }
  return value.number;
}

StringId keywordArg(const Value& value) {
  if (value.kind != ValueKind::Keyword) {
    throw RuntimeError("expected keyword");
  }
  return value.id;
}

const std::vector<Value>& vectorArg(const Value& value) {
  if (value.kind != ValueKind::Vector) {
    throw RuntimeError("expected vector");
  }
  return *value.vector;
}

const std::map<StringId, Value>& mapArg(const Value& value) {
  if (value.kind != ValueKind::Map) {
    throw RuntimeError("expected map");
  }
  return *value.map;
}

std::size_t indexArg(const Value& value) {
  const double raw = numberArg(value);
  if (raw < 0.0 || std::floor(raw) != raw) {
    throw RuntimeError("expected non-negative integer index");
  }
  return static_cast<std::size_t>(raw);
}

std::string displayString(const Value& value, const StringInterner& interner) {
  if (value.kind == ValueKind::String) {
    return value.text;
  }
  return valueToString(value, interner);
}

std::vector<StringId> pathArg(const Value& value) {
  const auto& pathValues = vectorArg(value);
  std::vector<StringId> path;
  path.reserve(pathValues.size());
  for (const auto& item : pathValues) {
    path.push_back(keywordArg(item));
  }
  return path;
}

Value getFromMap(const Value& map, StringId key, Value fallback = Value::nil()) {
  if (map.kind != ValueKind::Map) {
    return fallback;
  }
  auto found = map.map->find(key);
  return found == map.map->end() ? fallback : found->second;
}

Value getIn(const Value& root, const std::vector<StringId>& path, Value fallback = Value::nil()) {
  Value current = root;
  for (StringId key : path) {
    if (current.kind != ValueKind::Map) {
      return fallback;
    }
    auto found = current.map->find(key);
    if (found == current.map->end()) {
      return fallback;
    }
    current = found->second;
  }
  return current;
}

Value assocIn(const Value& root, const std::vector<StringId>& path, std::size_t index,
              const Value& replacement) {
  if (index >= path.size()) {
    return replacement;
  }

  std::map<StringId, Value> entries;
  if (root.kind == ValueKind::Map) {
    entries = *root.map;
  } else if (root.kind != ValueKind::Nil) {
    throw RuntimeError("assoc-in can only descend through maps or nil");
  }

  const StringId key = path[index];
  Value child = Value::nil();
  auto found = entries.find(key);
  if (found != entries.end()) {
    child = found->second;
  }
  entries[key] = assocIn(child, path, index + 1, replacement);
  return Value::mapValue(std::move(entries));
}

Value callFunction(VM& vm, const Value& fn, std::vector<Value> args) { return vm.call(fn, args); }

double requiredNumberField(VM& vm, const Value& map, std::string_view key) {
  const Value value = getFromMap(map, vm.interner().intern(key));
  if (value.kind != ValueKind::Number) {
    throw RuntimeError("expected numeric field " + std::string(key));
  }
  return value.number;
}

std::map<StringId, Value> makeMap(VM& vm,
                                  std::initializer_list<std::pair<std::string_view, Value>> entries) {
  std::map<StringId, Value> map;
  for (const auto& [key, value] : entries) {
    map[vm.interner().intern(key)] = value;
  }
  return map;
}

std::mt19937& rng() {
  static std::mt19937 generator{std::random_device{}()};
  return generator;
}

Value arithmeticAdd(VM&, const std::vector<Value>& args) {
  double result = 0.0;
  for (const auto& arg : args) {
    result += numberArg(arg);
  }
  return Value::numberValue(result);
}

Value arithmeticSub(VM&, const std::vector<Value>& args) {
  if (args.empty()) {
    throw RuntimeError("- expects at least one number");
  }
  double result = numberArg(args[0]);
  if (args.size() == 1) {
    return Value::numberValue(-result);
  }
  for (std::size_t i = 1; i < args.size(); ++i) {
    result -= numberArg(args[i]);
  }
  return Value::numberValue(result);
}

Value arithmeticMul(VM&, const std::vector<Value>& args) {
  double result = 1.0;
  for (const auto& arg : args) {
    result *= numberArg(arg);
  }
  return Value::numberValue(result);
}

Value arithmeticDiv(VM&, const std::vector<Value>& args) {
  if (args.empty()) {
    throw RuntimeError("/ expects at least one number");
  }
  double result = numberArg(args[0]);
  for (std::size_t i = 1; i < args.size(); ++i) {
    const double divisor = numberArg(args[i]);
    if (divisor == 0.0) {
      throw RuntimeError("division by zero");
    }
    result /= divisor;
  }
  return Value::numberValue(result);
}

Value equal(VM&, const std::vector<Value>& args) {
  if (args.size() < 2) {
    return Value::booleanValue(true);
  }
  for (std::size_t i = 1; i < args.size(); ++i) {
    if (!valueEquals(args[i - 1], args[i])) {
      return Value::booleanValue(false);
    }
  }
  return Value::booleanValue(true);
}

Value notEqual(VM& vm, const std::vector<Value>& args) {
  return Value::booleanValue(!equal(vm, args).boolean);
}

template <typename Compare>
Value compareNumbers(const std::vector<Value>& args, Compare compare) {
  if (args.size() < 2) {
    return Value::booleanValue(true);
  }
  for (std::size_t i = 1; i < args.size(); ++i) {
    if (!compare(numberArg(args[i - 1]), numberArg(args[i]))) {
      return Value::booleanValue(false);
    }
  }
  return Value::booleanValue(true);
}

Value logicalNot(VM&, const std::vector<Value>& args) {
  return Value::booleanValue(!isTruthy(args[0]));
}

Value makeString(VM& vm, const std::vector<Value>& args) {
  std::ostringstream out;
  for (const auto& arg : args) {
    out << displayString(arg, vm.interner());
  }
  return Value::stringValue(out.str());
}

Value nativeGet(VM&, const std::vector<Value>& args) {
  return getFromMap(args[0], keywordArg(args[1]), args.size() == 3 ? args[2] : Value::nil());
}

Value nativeGetIn(VM&, const std::vector<Value>& args) {
  return getIn(args[0], pathArg(args[1]), args.size() == 3 ? args[2] : Value::nil());
}

Value nativeAssoc(VM&, const std::vector<Value>& args) {
  std::map<StringId, Value> entries = mapArg(args[0]);
  if ((args.size() - 1) % 2 != 0) {
    throw RuntimeError("assoc expects key/value pairs");
  }
  for (std::size_t i = 1; i < args.size(); i += 2) {
    entries[keywordArg(args[i])] = args[i + 1];
  }
  return Value::mapValue(std::move(entries));
}

Value nativeAssocIn(VM&, const std::vector<Value>& args) {
  const auto path = pathArg(args[1]);
  if (path.empty()) {
    return args[2];
  }
  return assocIn(args[0], path, 0, args[2]);
}

Value nativeUpdate(VM& vm, const std::vector<Value>& args) {
  std::vector<Value> callArgs;
  callArgs.reserve(args.size() - 2);
  callArgs.push_back(getFromMap(args[0], keywordArg(args[1])));
  for (std::size_t i = 3; i < args.size(); ++i) {
    callArgs.push_back(args[i]);
  }
  return nativeAssoc(vm, {args[0], args[1], callFunction(vm, args[2], std::move(callArgs))});
}

Value nativeUpdateIn(VM& vm, const std::vector<Value>& args) {
  const auto path = pathArg(args[1]);
  std::vector<Value> callArgs;
  callArgs.reserve(args.size() - 2);
  callArgs.push_back(getIn(args[0], path));
  for (std::size_t i = 3; i < args.size(); ++i) {
    callArgs.push_back(args[i]);
  }
  const Value replacement = callFunction(vm, args[2], std::move(callArgs));
  return path.empty() ? replacement : assocIn(args[0], path, 0, replacement);
}

Value nativeKeys(VM&, const std::vector<Value>& args) {
  std::vector<Value> keys;
  for (const auto& [key, _] : mapArg(args[0])) {
    keys.push_back(Value::keywordValue(key));
  }
  return Value::vectorValue(std::move(keys));
}

Value nativeValues(VM&, const std::vector<Value>& args) {
  std::vector<Value> values;
  for (const auto& [_, value] : mapArg(args[0])) {
    values.push_back(value);
  }
  return Value::vectorValue(std::move(values));
}

Value nativeHas(VM&, const std::vector<Value>& args) {
  const auto& map = mapArg(args[0]);
  return Value::booleanValue(map.find(keywordArg(args[1])) != map.end());
}

Value nativeMerge(VM&, const std::vector<Value>& args) {
  std::map<StringId, Value> result;
  for (const auto& arg : args) {
    for (const auto& [key, value] : mapArg(arg)) {
      result[key] = value;
    }
  }
  return Value::mapValue(std::move(result));
}

Value nativeCount(VM&, const std::vector<Value>& args) {
  const Value& value = args[0];
  if (value.kind == ValueKind::Vector) {
    return Value::numberValue(static_cast<double>(value.vector->size()));
  }
  if (value.kind == ValueKind::Map) {
    return Value::numberValue(static_cast<double>(value.map->size()));
  }
  if (value.kind == ValueKind::String) {
    return Value::numberValue(static_cast<double>(value.text.size()));
  }
  if (value.kind == ValueKind::Nil) {
    return Value::numberValue(0.0);
  }
  throw RuntimeError("count expects vector, map, string, or nil");
}

Value nativeEmpty(VM& vm, const std::vector<Value>& args) {
  return Value::booleanValue(nativeCount(vm, args).number == 0.0);
}

Value nativeFirst(VM&, const std::vector<Value>& args) {
  const auto& values = vectorArg(args[0]);
  return values.empty() ? Value::nil() : values.front();
}

Value nativeLast(VM&, const std::vector<Value>& args) {
  const auto& values = vectorArg(args[0]);
  return values.empty() ? Value::nil() : values.back();
}

Value nativeNth(VM&, const std::vector<Value>& args) {
  const auto& values = vectorArg(args[0]);
  const std::size_t index = indexArg(args[1]);
  if (index >= values.size()) {
    return args.size() == 3 ? args[2] : Value::nil();
  }
  return values[index];
}

Value nativeConj(VM&, const std::vector<Value>& args) {
  std::vector<Value> values = vectorArg(args[0]);
  values.insert(values.end(), args.begin() + 1, args.end());
  return Value::vectorValue(std::move(values));
}

Value nativePop(VM&, const std::vector<Value>& args) {
  std::vector<Value> values = vectorArg(args[0]);
  if (!values.empty()) {
    values.pop_back();
  }
  return Value::vectorValue(std::move(values));
}

Value nativeSlice(VM&, const std::vector<Value>& args) {
  const auto& values = vectorArg(args[0]);
  std::size_t start = indexArg(args[1]);
  std::size_t end = indexArg(args[2]);
  start = std::min(start, values.size());
  end = std::min(end, values.size());
  if (end < start) {
    end = start;
  }
  return Value::vectorValue(std::vector<Value>(values.begin() + static_cast<std::ptrdiff_t>(start),
                                               values.begin() + static_cast<std::ptrdiff_t>(end)));
}

Value nativeMap(VM& vm, const std::vector<Value>& args) {
  const auto& values = vectorArg(args[0]);
  std::vector<Value> results;
  results.reserve(values.size());
  for (const auto& value : values) {
    results.push_back(callFunction(vm, args[1], {value}));
  }
  return Value::vectorValue(std::move(results));
}

Value nativeFilter(VM& vm, const std::vector<Value>& args) {
  const auto& values = vectorArg(args[0]);
  std::vector<Value> results;
  for (const auto& value : values) {
    if (isTruthy(callFunction(vm, args[1], {value}))) {
      results.push_back(value);
    }
  }
  return Value::vectorValue(std::move(results));
}

Value nativeRemove(VM& vm, const std::vector<Value>& args) {
  const auto& values = vectorArg(args[0]);
  std::vector<Value> results;
  for (const auto& value : values) {
    if (!isTruthy(callFunction(vm, args[1], {value}))) {
      results.push_back(value);
    }
  }
  return Value::vectorValue(std::move(results));
}

Value nativeAny(VM& vm, const std::vector<Value>& args) {
  for (const auto& value : vectorArg(args[0])) {
    if (isTruthy(callFunction(vm, args[1], {value}))) {
      return Value::booleanValue(true);
    }
  }
  return Value::booleanValue(false);
}

Value nativeAll(VM& vm, const std::vector<Value>& args) {
  for (const auto& value : vectorArg(args[0])) {
    if (!isTruthy(callFunction(vm, args[1], {value}))) {
      return Value::booleanValue(false);
    }
  }
  return Value::booleanValue(true);
}

Value nativeFind(VM& vm, const std::vector<Value>& args) {
  for (const auto& value : vectorArg(args[0])) {
    if (isTruthy(callFunction(vm, args[1], {value}))) {
      return value;
    }
  }
  return Value::nil();
}

Value nativeRange(VM&, const std::vector<Value>& args) {
  double start = 0.0;
  double end = numberArg(args[0]);
  if (args.size() == 2) {
    start = end;
    end = numberArg(args[1]);
  }

  std::vector<Value> values;
  for (double i = start; i < end; i += 1.0) {
    values.push_back(Value::numberValue(i));
  }
  return Value::vectorValue(std::move(values));
}

Value nativeSubstr(VM&, const std::vector<Value>& args) {
  if (args[0].kind != ValueKind::String) {
    throw RuntimeError("substr expects a string");
  }
  std::size_t start = indexArg(args[1]);
  std::size_t length = indexArg(args[2]);
  start = std::min(start, args[0].text.size());
  return Value::stringValue(args[0].text.substr(start, length));
}

Value nativeStringLength(VM&, const std::vector<Value>& args) {
  if (args[0].kind != ValueKind::String) {
    throw RuntimeError("string-length expects a string");
  }
  return Value::numberValue(static_cast<double>(args[0].text.size()));
}

Value nativeRand(VM&, const std::vector<Value>&) {
  return Value::numberValue(std::uniform_real_distribution<double>(0.0, 1.0)(rng()));
}

Value nativeRandInt(VM&, const std::vector<Value>& args) {
  const int max = static_cast<int>(numberArg(args[0]));
  if (max <= 0) {
    throw RuntimeError("rand-int max must be positive");
  }
  return Value::numberValue(static_cast<double>(std::uniform_int_distribution<int>(0, max - 1)(rng())));
}

Value nativeRandRange(VM&, const std::vector<Value>& args) {
  const double min = numberArg(args[0]);
  const double max = numberArg(args[1]);
  if (max < min) {
    throw RuntimeError("rand-range max must be >= min");
  }
  return Value::numberValue(std::uniform_real_distribution<double>(min, max)(rng()));
}

Value nativeChoose(VM&, const std::vector<Value>& args) {
  const auto& values = vectorArg(args[0]);
  if (values.empty()) {
    return Value::nil();
  }
  const auto index = static_cast<std::size_t>(
      std::uniform_int_distribution<int>(0, static_cast<int>(values.size() - 1))(rng()));
  return values[index];
}

Value nativeChance(VM&, const std::vector<Value>& args) {
  const double p = numberArg(args[0]);
  return Value::booleanValue(std::uniform_real_distribution<double>(0.0, 1.0)(rng()) < p);
}

Value nativeRectBox(VM& vm, const std::vector<Value>& args) {
  return Value::mapValue(makeMap(vm, {{":x", Value::numberValue(numberArg(args[0]))},
                                      {":y", Value::numberValue(numberArg(args[1]))},
                                      {":w", Value::numberValue(numberArg(args[2]))},
                                      {":h", Value::numberValue(numberArg(args[3]))}}));
}

Value nativeOverlap(VM& vm, const std::vector<Value>& args) {
  const double ax = requiredNumberField(vm, args[0], ":x");
  const double ay = requiredNumberField(vm, args[0], ":y");
  const double aw = requiredNumberField(vm, args[0], ":w");
  const double ah = requiredNumberField(vm, args[0], ":h");
  const double bx = requiredNumberField(vm, args[1], ":x");
  const double by = requiredNumberField(vm, args[1], ":y");
  const double bw = requiredNumberField(vm, args[1], ":w");
  const double bh = requiredNumberField(vm, args[1], ":h");
  return Value::booleanValue(ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by);
}

Value nativePointInRect(VM& vm, const std::vector<Value>& args) {
  const double x = numberArg(args[0]);
  const double y = numberArg(args[1]);
  const double rx = requiredNumberField(vm, args[2], ":x");
  const double ry = requiredNumberField(vm, args[2], ":y");
  const double rw = requiredNumberField(vm, args[2], ":w");
  const double rh = requiredNumberField(vm, args[2], ":h");
  return Value::booleanValue(x >= rx && x <= rx + rw && y >= ry && y <= ry + rh);
}

Value nativeRectCenter(VM& vm, const std::vector<Value>& args) {
  const double x = requiredNumberField(vm, args[0], ":x");
  const double y = requiredNumberField(vm, args[0], ":y");
  const double w = requiredNumberField(vm, args[0], ":w");
  const double h = requiredNumberField(vm, args[0], ":h");
  return Value::mapValue(makeMap(vm, {{":x", Value::numberValue(x + w / 2.0)},
                                      {":y", Value::numberValue(y + h / 2.0)}}));
}

Value nativeCircleBox(VM& vm, const std::vector<Value>& args) {
  return Value::mapValue(makeMap(vm, {{":x", Value::numberValue(numberArg(args[0]))},
                                      {":y", Value::numberValue(numberArg(args[1]))},
                                      {":r", Value::numberValue(numberArg(args[2]))}}));
}

Value nativeCircleOverlap(VM& vm, const std::vector<Value>& args) {
  const double ax = requiredNumberField(vm, args[0], ":x");
  const double ay = requiredNumberField(vm, args[0], ":y");
  const double ar = requiredNumberField(vm, args[0], ":r");
  const double bx = requiredNumberField(vm, args[1], ":x");
  const double by = requiredNumberField(vm, args[1], ":y");
  const double br = requiredNumberField(vm, args[1], ":r");
  const double dx = ax - bx;
  const double dy = ay - by;
  const double radius = ar + br;
  return Value::booleanValue(dx * dx + dy * dy <= radius * radius);
}

Value nativePointInCircle(VM& vm, const std::vector<Value>& args) {
  const double x = numberArg(args[0]);
  const double y = numberArg(args[1]);
  const double cx = requiredNumberField(vm, args[2], ":x");
  const double cy = requiredNumberField(vm, args[2], ":y");
  const double r = requiredNumberField(vm, args[2], ":r");
  const double dx = x - cx;
  const double dy = y - cy;
  return Value::booleanValue(dx * dx + dy * dy <= r * r);
}

const glyph::input::InputSystem* inputOrNull(const VM& vm) { return vm.input(); }

glyph::input::SwipeDirection swipeDirection(VM& vm, const Value& value) {
  const StringId id = keywordArg(value);
  const auto name = vm.interner().resolve(id);
  if (name == ":left") {
    return glyph::input::SwipeDirection::Left;
  }
  if (name == ":right") {
    return glyph::input::SwipeDirection::Right;
  }
  if (name == ":up") {
    return glyph::input::SwipeDirection::Up;
  }
  if (name == ":down") {
    return glyph::input::SwipeDirection::Down;
  }
  return glyph::input::SwipeDirection::None;
}

Value nativePressed(VM& vm, const std::vector<Value>& args) {
  const auto* input = inputOrNull(vm);
  return Value::booleanValue(input && input->pressed(keywordArg(args[0])));
}

Value nativeHeld(VM& vm, const std::vector<Value>& args) {
  const auto* input = inputOrNull(vm);
  return Value::booleanValue(input && input->held(keywordArg(args[0])));
}

Value nativeReleased(VM& vm, const std::vector<Value>& args) {
  const auto* input = inputOrNull(vm);
  return Value::booleanValue(input && input->released(keywordArg(args[0])));
}

Value nativeAxis(VM& vm, const std::vector<Value>& args) {
  const auto* input = inputOrNull(vm);
  return Value::numberValue(input ? input->axis(keywordArg(args[0])) : 0.0);
}

Value nativeSwipe(VM& vm, const std::vector<Value>& args) {
  const auto* input = inputOrNull(vm);
  return Value::booleanValue(input && input->swipe(swipeDirection(vm, args[0])));
}

Value nativePointerX(VM& vm, const std::vector<Value>&) {
  const auto* input = inputOrNull(vm);
  return Value::numberValue(input ? input->pointerPosition().x : 0.0);
}

Value nativePointerY(VM& vm, const std::vector<Value>&) {
  const auto* input = inputOrNull(vm);
  return Value::numberValue(input ? input->pointerPosition().y : 0.0);
}

Value nativePointerPos(VM& vm, const std::vector<Value>&) {
  const auto* input = inputOrNull(vm);
  const Vec2 pos = input ? input->pointerPosition() : Vec2{};
  return Value::vectorValue({Value::numberValue(pos.x), Value::numberValue(pos.y)});
}

Value nativePointerHeld(VM& vm, const std::vector<Value>&) {
  const auto* input = inputOrNull(vm);
  return Value::booleanValue(input && input->pointerHeld());
}

Value nativePointerPressed(VM& vm, const std::vector<Value>&) {
  const auto* input = inputOrNull(vm);
  return Value::booleanValue(input && input->pointerPressed());
}

Value nativePointerReleased(VM& vm, const std::vector<Value>&) {
  const auto* input = inputOrNull(vm);
  return Value::booleanValue(input && input->pointerReleased());
}

Value renderNode(VM& vm, std::string_view type) {
  std::map<StringId, Value> entries;
  entries[vm.interner().intern(":node")] = Value::keywordValue(vm.interner().intern(type));
  return Value::mapValue(std::move(entries));
}

std::map<StringId, Value> renderOptionMap(VM& vm, std::string_view type,
                                           const std::vector<Value>& args, std::size_t start,
                                           std::size_t end) {
  if ((end - start) % 2 != 0) {
    throw RuntimeError("render function expects keyword/value option pairs");
  }

  std::map<StringId, Value> entries;
  entries[vm.interner().intern(":node")] = Value::keywordValue(vm.interner().intern(type));
  for (std::size_t i = start; i < end; i += 2) {
    entries[keywordArg(args[i])] = args[i + 1];
  }
  return entries;
}

Value nativeClear(VM& vm, const std::vector<Value>& args) {
  auto entries = renderOptionMap(vm, ":clear", {}, 0, 0);
  entries[vm.interner().intern(":color")] = args[0];
  return Value::mapValue(std::move(entries));
}

Value nativeRenderOptionsOnly(VM& vm, const std::vector<Value>& args, std::string_view type) {
  return Value::mapValue(renderOptionMap(vm, type, args, 0, args.size()));
}

Value nativeGroup(VM& vm, const std::vector<Value>& args) {
  auto entries = renderOptionMap(vm, ":group", {}, 0, 0);
  entries[vm.interner().intern(":children")] = Value::vectorValue(args);
  return Value::mapValue(std::move(entries));
}

Value nativeLayer(VM& vm, const std::vector<Value>& args) {
  std::size_t childStart = 0;
  auto entries = renderOptionMap(vm, ":layer", {}, 0, 0);
  if (args.size() >= 2 && args[0].kind == ValueKind::Keyword) {
    entries[keywordArg(args[0])] = args[1];
    childStart = 2;
  }
  std::vector<Value> children(args.begin() + static_cast<std::ptrdiff_t>(childStart), args.end());
  entries[vm.interner().intern(":children")] = Value::vectorValue(std::move(children));
  return Value::mapValue(std::move(entries));
}

Value nativeParentRenderNode(VM& vm, const std::vector<Value>& args, std::string_view type) {
  std::size_t childStart = 0;
  while (childStart < args.size()) {
    if (args[childStart].kind != ValueKind::Keyword) {
      break;
    }
    if (childStart + 1 >= args.size()) {
      throw RuntimeError("render parent option is missing a value");
    }
    childStart += 2;
  }

  auto entries = renderOptionMap(vm, type, args, 0, childStart);
  std::vector<Value> children(args.begin() + static_cast<std::ptrdiff_t>(childStart), args.end());
  entries[vm.interner().intern(":children")] = Value::vectorValue(std::move(children));
  return Value::mapValue(std::move(entries));
}

void define(VM& vm, std::string_view name, NativeFn fn, int minArgs = 0, int maxArgs = -1) {
  vm.defineGlobal(name, Value::nativeValue(vm.interner().intern(name), std::move(fn), minArgs, maxArgs));
}

} // namespace

void registerCoreNatives(VM& vm) {
  define(vm, "+", arithmeticAdd, 0, -1);
  define(vm, "-", arithmeticSub, 1, -1);
  define(vm, "*", arithmeticMul, 0, -1);
  define(vm, "/", arithmeticDiv, 1, -1);
  define(vm, "mod", [](VM&, const std::vector<Value>& args) {
    return Value::numberValue(std::fmod(numberArg(args[0]), numberArg(args[1])));
  }, 2, 2);
  define(vm, "abs", [](VM&, const std::vector<Value>& args) {
    return Value::numberValue(std::fabs(numberArg(args[0])));
  }, 1, 1);
  define(vm, "floor", [](VM&, const std::vector<Value>& args) {
    return Value::numberValue(std::floor(numberArg(args[0])));
  }, 1, 1);
  define(vm, "ceil", [](VM&, const std::vector<Value>& args) {
    return Value::numberValue(std::ceil(numberArg(args[0])));
  }, 1, 1);
  define(vm, "round", [](VM&, const std::vector<Value>& args) {
    return Value::numberValue(std::round(numberArg(args[0])));
  }, 1, 1);
  define(vm, "min", [](VM&, const std::vector<Value>& args) {
    double result = numberArg(args[0]);
    for (std::size_t i = 1; i < args.size(); ++i) {
      result = std::min(result, numberArg(args[i]));
    }
    return Value::numberValue(result);
  }, 1, -1);
  define(vm, "max", [](VM&, const std::vector<Value>& args) {
    double result = numberArg(args[0]);
    for (std::size_t i = 1; i < args.size(); ++i) {
      result = std::max(result, numberArg(args[i]));
    }
    return Value::numberValue(result);
  }, 1, -1);
  define(vm, "clamp", [](VM&, const std::vector<Value>& args) {
    return Value::numberValue(std::clamp(numberArg(args[0]), numberArg(args[1]), numberArg(args[2])));
  }, 3, 3);
  define(vm, "lerp", [](VM&, const std::vector<Value>& args) {
    const double a = numberArg(args[0]);
    const double b = numberArg(args[1]);
    const double t = numberArg(args[2]);
    return Value::numberValue(a + (b - a) * t);
  }, 3, 3);

  define(vm, "=", equal, 0, -1);
  define(vm, "not=", notEqual, 0, -1);
  define(vm, "<", [](VM&, const std::vector<Value>& args) {
    return compareNumbers(args, std::less<double>());
  }, 0, -1);
  define(vm, "<=", [](VM&, const std::vector<Value>& args) {
    return compareNumbers(args, std::less_equal<double>());
  }, 0, -1);
  define(vm, ">", [](VM&, const std::vector<Value>& args) {
    return compareNumbers(args, std::greater<double>());
  }, 0, -1);
  define(vm, ">=", [](VM&, const std::vector<Value>& args) {
    return compareNumbers(args, std::greater_equal<double>());
  }, 0, -1);

  define(vm, "not", logicalNot, 1, 1);

  define(vm, "sin", [](VM&, const std::vector<Value>& args) {
    return Value::numberValue(std::sin(numberArg(args[0])));
  }, 1, 1);
  define(vm, "cos", [](VM&, const std::vector<Value>& args) {
    return Value::numberValue(std::cos(numberArg(args[0])));
  }, 1, 1);
  define(vm, "tan", [](VM&, const std::vector<Value>& args) {
    return Value::numberValue(std::tan(numberArg(args[0])));
  }, 1, 1);
  define(vm, "sqrt", [](VM&, const std::vector<Value>& args) {
    return Value::numberValue(std::sqrt(numberArg(args[0])));
  }, 1, 1);
  define(vm, "deg->rad", [](VM&, const std::vector<Value>& args) {
    return Value::numberValue(numberArg(args[0]) * pi / 180.0);
  }, 1, 1);
  define(vm, "rad->deg", [](VM&, const std::vector<Value>& args) {
    return Value::numberValue(numberArg(args[0]) * 180.0 / pi);
  }, 1, 1);
  define(vm, "dist", [](VM&, const std::vector<Value>& args) {
    const double dx = numberArg(args[2]) - numberArg(args[0]);
    const double dy = numberArg(args[3]) - numberArg(args[1]);
    return Value::numberValue(std::sqrt(dx * dx + dy * dy));
  }, 4, 4);
  define(vm, "angle-to", [](VM&, const std::vector<Value>& args) {
    return Value::numberValue(std::atan2(numberArg(args[3]) - numberArg(args[1]),
                                         numberArg(args[2]) - numberArg(args[0])));
  }, 4, 4);

  define(vm, "str", makeString, 0, -1);
  define(vm, "substr", nativeSubstr, 3, 3);
  define(vm, "string-length", nativeStringLength, 1, 1);

  define(vm, "get", nativeGet, 2, 3);
  define(vm, "get-in", nativeGetIn, 2, 3);
  define(vm, "assoc", nativeAssoc, 3, -1);
  define(vm, "assoc-in", nativeAssocIn, 3, 3);
  define(vm, "update", nativeUpdate, 3, -1);
  define(vm, "update-in", nativeUpdateIn, 3, -1);
  define(vm, "keys", nativeKeys, 1, 1);
  define(vm, "values", nativeValues, 1, 1);
  define(vm, "has?", nativeHas, 2, 2);
  define(vm, "merge", nativeMerge, 0, -1);

  define(vm, "count", nativeCount, 1, 1);
  define(vm, "empty?", nativeEmpty, 1, 1);
  define(vm, "first", nativeFirst, 1, 1);
  define(vm, "last", nativeLast, 1, 1);
  define(vm, "nth", nativeNth, 2, 3);
  define(vm, "conj", nativeConj, 2, -1);
  define(vm, "push", nativeConj, 2, -1);
  define(vm, "pop", nativePop, 1, 1);
  define(vm, "slice", nativeSlice, 3, 3);
  define(vm, "map", nativeMap, 2, 2);
  define(vm, "filter", nativeFilter, 2, 2);
  define(vm, "remove", nativeRemove, 2, 2);
  define(vm, "any", nativeAny, 2, 2);
  define(vm, "all", nativeAll, 2, 2);
  define(vm, "find", nativeFind, 2, 2);
  define(vm, "range", nativeRange, 1, 2);

  define(vm, "rand", nativeRand, 0, 0);
  define(vm, "rand-int", nativeRandInt, 1, 1);
  define(vm, "rand-range", nativeRandRange, 2, 2);
  define(vm, "choose", nativeChoose, 1, 1);
  define(vm, "chance", nativeChance, 1, 1);

  define(vm, "rect-box", nativeRectBox, 4, 4);
  define(vm, "overlap?", nativeOverlap, 2, 2);
  define(vm, "point-in-rect?", nativePointInRect, 3, 3);
  define(vm, "rect-center", nativeRectCenter, 1, 1);
  define(vm, "circle-box", nativeCircleBox, 3, 3);
  define(vm, "circle-overlap?", nativeCircleOverlap, 2, 2);
  define(vm, "point-in-circle?", nativePointInCircle, 3, 3);

  define(vm, "pressed?", nativePressed, 1, 1);
  define(vm, "held?", nativeHeld, 1, 1);
  define(vm, "released?", nativeReleased, 1, 1);
  define(vm, "axis", nativeAxis, 1, 1);
  define(vm, "swipe?", nativeSwipe, 1, 1);
  define(vm, "pointer-x", nativePointerX, 0, 0);
  define(vm, "pointer-y", nativePointerY, 0, 0);
  define(vm, "pointer-pos", nativePointerPos, 0, 0);
  define(vm, "pointer-held?", nativePointerHeld, 0, 0);
  define(vm, "pointer-pressed?", nativePointerPressed, 0, 0);
  define(vm, "pointer-released?", nativePointerReleased, 0, 0);

  vm.defineGlobal("empty", renderNode(vm, ":empty"));
  define(vm, "clear", nativeClear, 1, 1);
  define(vm, "group", nativeGroup, 0, -1);
  define(vm, "layer", nativeLayer, 0, -1);
  define(vm, "rect", [](VM& vm, const std::vector<Value>& args) {
    return nativeRenderOptionsOnly(vm, args, ":rect");
  }, 0, -1);
  define(vm, "circle", [](VM& vm, const std::vector<Value>& args) {
    return nativeRenderOptionsOnly(vm, args, ":circle");
  }, 0, -1);
  define(vm, "line", [](VM& vm, const std::vector<Value>& args) {
    return nativeRenderOptionsOnly(vm, args, ":line");
  }, 0, -1);
  define(vm, "sprite", [](VM& vm, const std::vector<Value>& args) {
    return nativeRenderOptionsOnly(vm, args, ":sprite");
  }, 0, -1);
  define(vm, "text", [](VM& vm, const std::vector<Value>& args) {
    return nativeRenderOptionsOnly(vm, args, ":text");
  }, 0, -1);
  define(vm, "camera", [](VM& vm, const std::vector<Value>& args) {
    return nativeParentRenderNode(vm, args, ":camera");
  }, 0, -1);
  define(vm, "transform", [](VM& vm, const std::vector<Value>& args) {
    return nativeParentRenderNode(vm, args, ":transform");
  }, 0, -1);
}

} // namespace glyph::script
