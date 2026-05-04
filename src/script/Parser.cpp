#include "script/Parser.h"

#include "script/Error.h"

#include <sstream>
#include <utility>

namespace glyph::script {

Parser::Parser(std::vector<Token> tokens, StringInterner& interner, std::string file)
    : tokens_(std::move(tokens)), interner_(interner), file_(std::move(file)) {}

std::vector<AstPtr> Parser::parseProgram() {
  std::vector<AstPtr> expressions;
  while (!atEnd()) {
    expressions.push_back(parseExpression());
  }
  return expressions;
}

bool Parser::atEnd() const { return peek().type == TokenType::End; }

const Token& Parser::peek() const { return tokens_[current_]; }

const Token& Parser::previous() const { return tokens_[current_ - 1]; }

const Token& Parser::advance() {
  if (!atEnd()) {
    ++current_;
  }
  return previous();
}

bool Parser::check(TokenType type) const {
  if (atEnd()) {
    return false;
  }
  return peek().type == type;
}

bool Parser::match(TokenType type) {
  if (!check(type)) {
    return false;
  }
  advance();
  return true;
}

const Token& Parser::consume(TokenType type, const std::string& message) {
  if (check(type)) {
    return advance();
  }

  std::ostringstream out;
  out << message << " at " << file_ << ":" << peek().line << ":" << peek().column;
  throw ParseError(out.str());
}

AstPtr Parser::parseExpression() {
  const Token& token = advance();
  switch (token.type) {
  case TokenType::Number: {
    auto out = node(AstKind::Number, token);
    out->number = token.number;
    return out;
  }
  case TokenType::String: {
    auto out = node(AstKind::String, token);
    out->text = token.text;
    return out;
  }
  case TokenType::Keyword: {
    auto out = node(AstKind::Keyword, token);
    out->id = interner_.intern(token.text);
    return out;
  }
  case TokenType::Symbol:
    if (token.text == "nil") {
      return node(AstKind::Nil, token);
    }
    if (token.text == "true" || token.text == "false") {
      auto out = node(AstKind::Bool, token);
      out->boolean = token.text == "true";
      return out;
    }
    {
      auto out = node(AstKind::Symbol, token);
      out->id = interner_.intern(token.text);
      out->text = token.text;
      return out;
    }
  case TokenType::LeftParen:
    return parseList(token);
  case TokenType::LeftBracket:
    return parseVector(token);
  case TokenType::LeftBrace:
    return parseMap(token);
  default:
    break;
  }

  std::ostringstream out;
  out << "unexpected token at " << file_ << ":" << token.line << ":" << token.column;
  throw ParseError(out.str());
}

AstPtr Parser::parseList(const Token& start) {
  auto out = node(AstKind::List, start);
  while (!check(TokenType::RightParen)) {
    if (atEnd()) {
      throw ParseError("unterminated list");
    }
    out->children.push_back(parseExpression());
  }
  consume(TokenType::RightParen, "expected ')'");
  return out;
}

AstPtr Parser::parseVector(const Token& start) {
  auto out = node(AstKind::Vector, start);
  while (!check(TokenType::RightBracket)) {
    if (atEnd()) {
      throw ParseError("unterminated vector");
    }
    out->children.push_back(parseExpression());
  }
  consume(TokenType::RightBracket, "expected ']'");
  return out;
}

AstPtr Parser::parseMap(const Token& start) {
  auto out = node(AstKind::Map, start);
  while (!check(TokenType::RightBrace)) {
    if (atEnd()) {
      throw ParseError("unterminated map");
    }
    auto key = parseExpression();
    if (check(TokenType::RightBrace)) {
      throw ParseError("map literal has an odd number of forms");
    }
    auto value = parseExpression();
    out->entries.emplace_back(key, value);
  }
  consume(TokenType::RightBrace, "expected '}'");
  return out;
}

AstPtr Parser::node(AstKind kind, const Token& token) const {
  auto out = std::make_shared<AstNode>();
  out->kind = kind;
  out->span.file = file_;
  out->span.line = token.line;
  out->span.column = token.column;
  return out;
}

} // namespace glyph::script
