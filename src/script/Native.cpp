#include "script/Native.h"

#include "script/Error.h"
#include "script/VM.h"

#include <cmath>
#include <sstream>

namespace glyph::script {
namespace {

double numberArg(const Value& value) {
  if (value.kind != ValueKind::Number) {
    throw RuntimeError("expected number");
  }
  return value.number;
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
    out << valueToString(arg, vm.interner());
  }
  return Value::stringValue(out.str());
}

} // namespace

void registerCoreNatives(VM& vm) {
  vm.defineGlobal("+", Value::nativeValue(vm.interner().intern("+"), arithmeticAdd, 0, -1));
  vm.defineGlobal("-", Value::nativeValue(vm.interner().intern("-"), arithmeticSub, 1, -1));
  vm.defineGlobal("*", Value::nativeValue(vm.interner().intern("*"), arithmeticMul, 0, -1));
  vm.defineGlobal("/", Value::nativeValue(vm.interner().intern("/"), arithmeticDiv, 1, -1));

  vm.defineGlobal("=", Value::nativeValue(vm.interner().intern("="), equal, 0, -1));
  vm.defineGlobal("not=", Value::nativeValue(vm.interner().intern("not="), notEqual, 0, -1));
  vm.defineGlobal("<", Value::nativeValue(vm.interner().intern("<"),
                                          [](VM&, const std::vector<Value>& args) {
                                            return compareNumbers(args, std::less<double>());
                                          },
                                          0, -1));
  vm.defineGlobal("<=", Value::nativeValue(vm.interner().intern("<="),
                                           [](VM&, const std::vector<Value>& args) {
                                             return compareNumbers(args, std::less_equal<double>());
                                           },
                                           0, -1));
  vm.defineGlobal(">", Value::nativeValue(vm.interner().intern(">"),
                                          [](VM&, const std::vector<Value>& args) {
                                            return compareNumbers(args, std::greater<double>());
                                          },
                                          0, -1));
  vm.defineGlobal(">=", Value::nativeValue(vm.interner().intern(">="),
                                           [](VM&, const std::vector<Value>& args) {
                                             return compareNumbers(args, std::greater_equal<double>());
                                           },
                                           0, -1));

  vm.defineGlobal("not", Value::nativeValue(vm.interner().intern("not"), logicalNot, 1, 1));
  vm.defineGlobal("str", Value::nativeValue(vm.interner().intern("str"), makeString, 0, -1));
}

} // namespace glyph::script
