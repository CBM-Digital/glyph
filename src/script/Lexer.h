#pragma once

#include "script/Token.h"

#include <string>
#include <string_view>
#include <vector>

namespace glyph::script {

class Lexer {
public:
  explicit Lexer(std::string_view source);

  std::vector<Token> lex();

private:
  bool atEnd() const;
  char peek() const;
  char peekNext() const;
  char advance();
  bool match(char expected);

  void skipWhitespaceAndComments();
  Token makeToken(TokenType type, std::string text, int line, int column) const;
  Token readString(int line, int column);
  Token readNumber(int line, int column);
  Token readSymbolOrKeyword(int line, int column);

  std::string source_;
  std::size_t current_ = 0;
  int line_ = 1;
  int column_ = 1;
};

} // namespace glyph::script
