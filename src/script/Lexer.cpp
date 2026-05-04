#include "script/Lexer.h"

#include "script/Error.h"

#include <cctype>
#include <cstdlib>

namespace glyph::script {

Lexer::Lexer(std::string_view source) : source_(source) {}

std::vector<Token> Lexer::lex() {
  std::vector<Token> tokens;

  while (true) {
    skipWhitespaceAndComments();
    if (atEnd()) {
      tokens.push_back(makeToken(TokenType::End, "", line_, column_));
      return tokens;
    }

    const int line = line_;
    const int column = column_;
    const char c = advance();

    switch (c) {
    case '(':
      tokens.push_back(makeToken(TokenType::LeftParen, "(", line, column));
      break;
    case ')':
      tokens.push_back(makeToken(TokenType::RightParen, ")", line, column));
      break;
    case '[':
      tokens.push_back(makeToken(TokenType::LeftBracket, "[", line, column));
      break;
    case ']':
      tokens.push_back(makeToken(TokenType::RightBracket, "]", line, column));
      break;
    case '{':
      tokens.push_back(makeToken(TokenType::LeftBrace, "{", line, column));
      break;
    case '}':
      tokens.push_back(makeToken(TokenType::RightBrace, "}", line, column));
      break;
    case '"':
      tokens.push_back(readString(line, column));
      break;
    default:
      if (std::isdigit(static_cast<unsigned char>(c)) ||
          (c == '-' && std::isdigit(static_cast<unsigned char>(peek())))) {
        --current_;
        --column_;
        tokens.push_back(readNumber(line, column));
      } else {
        --current_;
        --column_;
        tokens.push_back(readSymbolOrKeyword(line, column));
      }
      break;
    }
  }
}

bool Lexer::atEnd() const { return current_ >= source_.size(); }

char Lexer::peek() const { return atEnd() ? '\0' : source_[current_]; }

char Lexer::peekNext() const {
  return current_ + 1 >= source_.size() ? '\0' : source_[current_ + 1];
}

char Lexer::advance() {
  const char c = source_[current_++];
  if (c == '\n') {
    ++line_;
    column_ = 1;
  } else {
    ++column_;
  }
  return c;
}

bool Lexer::match(char expected) {
  if (atEnd() || source_[current_] != expected) {
    return false;
  }
  advance();
  return true;
}

void Lexer::skipWhitespaceAndComments() {
  while (!atEnd()) {
    const char c = peek();
    if (c == ' ' || c == '\r' || c == '\t' || c == '\n') {
      advance();
      continue;
    }
    if (c == ';') {
      while (!atEnd() && peek() != '\n') {
        advance();
      }
      continue;
    }
    return;
  }
}

Token Lexer::makeToken(TokenType type, std::string text, int line, int column) const {
  Token token;
  token.type = type;
  token.text = std::move(text);
  token.line = line;
  token.column = column;
  return token;
}

Token Lexer::readString(int line, int column) {
  std::string value;
  while (!atEnd() && peek() != '"') {
    const char c = advance();
    if (c == '\\') {
      if (atEnd()) {
        throw ParseError("unterminated string escape");
      }
      const char escaped = advance();
      switch (escaped) {
      case 'n':
        value.push_back('\n');
        break;
      case 't':
        value.push_back('\t');
        break;
      case '"':
        value.push_back('"');
        break;
      case '\\':
        value.push_back('\\');
        break;
      default:
        value.push_back(escaped);
        break;
      }
    } else {
      value.push_back(c);
    }
  }

  if (atEnd()) {
    throw ParseError("unterminated string");
  }

  advance();
  return makeToken(TokenType::String, value, line, column);
}

Token Lexer::readNumber(int line, int column) {
  const auto start = current_;
  if (peek() == '-') {
    advance();
  }
  while (std::isdigit(static_cast<unsigned char>(peek()))) {
    advance();
  }
  if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peekNext()))) {
    advance();
    while (std::isdigit(static_cast<unsigned char>(peek()))) {
      advance();
    }
  }

  Token token = makeToken(TokenType::Number, source_.substr(start, current_ - start), line, column);
  token.number = std::strtod(token.text.c_str(), nullptr);
  return token;
}

Token Lexer::readSymbolOrKeyword(int line, int column) {
  const auto start = current_;
  while (!atEnd()) {
    const char c = peek();
    if (std::isspace(static_cast<unsigned char>(c)) || c == ';' || c == '(' ||
        c == ')' || c == '[' || c == ']' || c == '{' || c == '}') {
      break;
    }
    advance();
  }

  const std::string text = source_.substr(start, current_ - start);
  if (text.empty()) {
    throw ParseError("unexpected character");
  }
  if (text == ":") {
    throw ParseError("keyword requires a name");
  }
  return makeToken(text[0] == ':' ? TokenType::Keyword : TokenType::Symbol, text, line, column);
}

} // namespace glyph::script
