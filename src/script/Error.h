#pragma once

#include <stdexcept>
#include <string>

namespace glyph::script {

class ScriptError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

class ParseError : public ScriptError {
public:
  using ScriptError::ScriptError;
};

class RuntimeError : public ScriptError {
public:
  using ScriptError::ScriptError;
};

} // namespace glyph::script
