#pragma once

#include "core/StringInterner.h"
#include "script/Ast.h"
#include "script/Token.h"

#include <string>
#include <vector>

namespace glyph::script {

class Parser {
public:
  Parser(std::vector<Token> tokens, StringInterner& interner, std::string file = "<input>");

  std::vector<AstPtr> parseProgram();

private:
  bool atEnd() const;
  const Token& peek() const;
  const Token& previous() const;
  const Token& advance();
  bool check(TokenType type) const;
  bool match(TokenType type);
  const Token& consume(TokenType type, const std::string& message);

  AstPtr parseExpression();
  AstPtr parseList(const Token& start);
  AstPtr parseVector(const Token& start);
  AstPtr parseMap(const Token& start);
  AstPtr node(AstKind kind, const Token& token) const;

  std::vector<Token> tokens_;
  StringInterner& interner_;
  std::string file_;
  std::size_t current_ = 0;
};

} // namespace glyph::script
