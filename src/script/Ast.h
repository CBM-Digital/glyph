#pragma once

#include "core/Types.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace glyph::script {

enum class AstKind {
  Nil,
  Bool,
  Number,
  String,
  Symbol,
  Keyword,
  List,
  Vector,
  Map
};

struct SourceSpan {
  std::string file;
  int line = 1;
  int column = 1;
};

struct AstNode;
using AstPtr = std::shared_ptr<AstNode>;

struct AstNode {
  AstKind kind = AstKind::Nil;
  SourceSpan span;

  bool boolean = false;
  double number = 0.0;
  StringId id = 0;
  std::string text;

  std::vector<AstPtr> children;
  std::vector<std::pair<AstPtr, AstPtr>> entries;
};

} // namespace glyph::script
