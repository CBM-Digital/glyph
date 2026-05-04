#pragma once

#include <string>

namespace glyph::script {

enum class TokenType {
  LeftParen,
  RightParen,
  LeftBracket,
  RightBracket,
  LeftBrace,
  RightBrace,
  Number,
  String,
  Symbol,
  Keyword,
  End
};

struct Token {
  TokenType type = TokenType::End;
  std::string text;
  double number = 0.0;
  int line = 1;
  int column = 1;
};

} // namespace glyph::script
